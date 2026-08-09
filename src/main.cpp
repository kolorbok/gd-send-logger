#include <Geode/Geode.hpp>
#include <Geode/modify/RateStarsLayer.hpp>
#include <Geode/modify/LevelSearchLayer.hpp>
#include <Geode/modify/LevelBrowserLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GJSearchObject.hpp>
#include <Geode/binding/SetTextPopup.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/loader/Loader.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/NodeIDs.hpp>
#include <Geode/ui/Popup.hpp>
#include "api_url.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace geode::prelude;

namespace {

constexpr char const* MOD_NAME = "GD Requests";
constexpr std::size_t FEEDBACK_LIMIT = 1500;

struct SendSnapshot {
    int levelID = 0;
    int stars = 0;
    int featureState = 0;
    bool hasPlatformer = false;
    bool platformer = false;
    std::string levelName;
    std::string creator;
};

struct RequestFilters {
    std::string difficulty = "all";
    std::string levelType = "all";
    std::string status = "unchecked";
    std::string minSend = "any";
    std::string rated = "all";
    std::string sort = "newest";
};

struct RequestMeta {
    int requestID = 0;
    int levelID = 0;
    std::string event = "0";
    int difficulty = 0;
    bool rated = false;
};

struct ClientState {
    std::string mode = "all";
    std::string serverID;
    std::string userID;
    bool moderator = false;
    bool helper = false;
    bool reviewer = false;
    int total = 0;
    int returned = 0;
};

struct RequestContext {
    bool active = false;
    RequestMeta request;
    std::string mode = "all";
};

static std::unordered_map<std::string, std::chrono::steady_clock::time_point> g_recentSends;
static RequestFilters g_filters;
static ClientState g_client;
static RequestContext g_context;
static std::unordered_map<int, RequestMeta> g_requestByLevel;
static std::unordered_map<int, std::string> g_feedbackDrafts;
static bool g_nextBrowserIsRequests = false;
static bool g_requestBrowserActive = false;
static LevelBrowserLayer* g_requestBrowser = nullptr;
static bool g_creatingHelperPopup = false;

static std::string gdToStd(gd::string const& value) {
    return std::string(value.c_str());
}

static std::string trim(std::string value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r' || value.front() == '\n')) {
        value.erase(value.begin());
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n')) {
        value.pop_back();
    }
    return value;
}

static std::vector<std::string> splitTabs(std::string const& line) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (true) {
        auto pos = line.find('\t', start);
        if (pos == std::string::npos) {
            out.push_back(line.substr(start));
            break;
        }
        out.push_back(line.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

static int parseInt(std::string const& value, int fallback = 0) {
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

static bool debugLogging() {
    return Mod::get()->getSettingValue<bool>("debug-logging");
}

static std::string connectionKey() {
    return trim(Mod::get()->getSettingValue<std::string>("connection-key"));
}

static std::string limitPopupText(std::string value, std::size_t limit = 700) {
    if (value.size() <= limit) return value;
    value.resize(limit);
    value += "...";
    return value;
}

static void showAlert(std::string const& title, std::string const& message) {
    FLAlertLayer::create(
        title.c_str(),
        gd::string(limitPopupText(message).c_str()),
        "OK"
    )->show();
}

static void showRequestError(std::string const& message) {
    showAlert(MOD_NAME, message);
}

static std::string apiBase() {
    std::string url = SEND_API_URL;
    std::string suffix = "/gd-send";
    if (url.size() >= suffix.size() && url.compare(url.size() - suffix.size(), suffix.size(), suffix) == 0) {
        url.resize(url.size() - suffix.size());
    }
    return url;
}

static std::string featureStateToSendType(int featureState) {
    switch (featureState) {
        case 1: return "featured";
        case 2: return "epic";
        case 3: return "legendary";
        case 4: return "mythic";
        default: return "star_rate";
    }
}

static std::string makeEventID(SendSnapshot const& snapshot, bool isTest) {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return std::string(isTest ? "test-" : "") + std::to_string(millis) + "-" +
        std::to_string(snapshot.levelID) + "-" + std::to_string(snapshot.stars) + "-" +
        std::to_string(snapshot.featureState);
}

static std::string makeRequestActionEventID(RequestContext const& context, std::string const& action) {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return "request-" + action + "-" + std::to_string(context.request.requestID) + "-" + std::to_string(millis);
}

static bool isRecentDuplicate(SendSnapshot const& snapshot) {
    auto now = std::chrono::steady_clock::now();
    std::string key = std::to_string(snapshot.levelID) + ":" + std::to_string(snapshot.stars) + ":" +
        std::to_string(snapshot.featureState);

    for (auto it = g_recentSends.begin(); it != g_recentSends.end();) {
        if (now - it->second > std::chrono::seconds(15)) it = g_recentSends.erase(it);
        else ++it;
    }

    auto found = g_recentSends.find(key);
    if (found != g_recentSends.end() && now - found->second < std::chrono::seconds(5)) return true;
    g_recentSends[key] = now;
    return false;
}

static SendSnapshot captureSend(RateStarsLayer* layer) {
    SendSnapshot snapshot;
    if (!layer) return snapshot;

    snapshot.levelID = layer->m_levelID;
    snapshot.stars = layer->m_starsRate;
    snapshot.featureState = layer->m_featureState;

    if (snapshot.levelID > 0) {
        if (auto* level = GameLevelManager::sharedState()->getSavedLevel(snapshot.levelID)) {
            snapshot.levelName = trim(gdToStd(level->m_levelName));
            snapshot.creator = trim(gdToStd(level->m_creatorName));
            snapshot.platformer = level->isPlatformer();
            snapshot.hasPlatformer = true;
        }
    }
    return snapshot;
}

static bool fakeGDModLoaded() {
    return Loader::get()->isModLoaded("bitz.fakegdmod");
}

static bool fakeGDModWillSimulateSend(RateStarsLayer* layer) {
    if (!layer || !fakeGDModLoaded()) return false;
    auto* children = layer->getChildren();
    if (!children || children->count() < 1) return false;
    auto* firstChild = children->objectAtIndex(0);
    auto* innerLayer = dynamic_cast<cocos2d::CCLayer*>(firstChild);
    return innerLayer && innerLayer->getChildrenCount() == 3;
}

static std::string feedbackFor(RequestContext const& context) {
    if (!context.active || context.request.requestID <= 0) return "";
    auto it = g_feedbackDrafts.find(context.request.requestID);
    return it == g_feedbackDrafts.end() ? std::string() : it->second;
}

static matjson::Value buildPayload(SendSnapshot const& snapshot, bool isTest, RequestContext const* context = nullptr) {
    auto body = matjson::Value();
    body["eventId"] = makeEventID(snapshot, isTest);
    body["levelId"] = snapshot.levelID;
    body["stars"] = snapshot.stars;
    body["featureState"] = snapshot.featureState;
    body["sendType"] = featureStateToSendType(snapshot.featureState);

    if (!snapshot.levelName.empty()) body["levelName"] = snapshot.levelName;
    if (!snapshot.creator.empty()) body["creator"] = snapshot.creator;
    if (snapshot.hasPlatformer) body["platformer"] = snapshot.platformer;

    if (!isTest && context && context->active && context->mode == "moderator" && context->request.requestID > 0) {
        body["requestId"] = context->request.requestID;
        body["requestMode"] = "moderator";
        body["requestEvent"] = context->request.event;
        auto feedback = feedbackFor(*context);
        if (!feedback.empty()) body["feedback"] = feedback.substr(0, FEEDBACK_LIMIT);
    }
    return body;
}

static void reportSend(SendSnapshot snapshot, bool isTest = false, RequestContext const* context = nullptr) {
    if (!isTest && !Mod::get()->getSettingValue<bool>("enabled")) return;
    if (snapshot.levelID <= 0 || snapshot.stars <= 0 || snapshot.stars > 10) {
        if (debugLogging() || isTest) {
            log::warn("Ignoring {}send with invalid values: levelID={}, stars={}, featureState={}",
                isTest ? "test " : "", snapshot.levelID, snapshot.stars, snapshot.featureState);
        }
        return;
    }
    if (!isTest && isRecentDuplicate(snapshot)) {
        if (debugLogging()) log::info("Duplicate moderator send suppressed for level {}", snapshot.levelID);
        return;
    }

    auto key = connectionKey();
    if (key.empty()) {
        log::warn("GD Requests is not configured. Fill Connection Key in mod settings.");
        if (isTest) showAlert(MOD_NAME, "Test was not sent: Connection Key is empty.");
        return;
    }

    auto body = buildPayload(snapshot, isTest, context);
    auto req = web::WebRequest();
    req.header("Content-Type", "application/json");
    req.header("Authorization", "Bearer " + key);
    req.bodyJSON(body);
    req.timeout(std::chrono::seconds(15));

    int requestID = (context && context->active) ? context->request.requestID : 0;
    async::spawn(req.post(SEND_API_URL), [snapshot, isTest, requestID](web::WebResponse res) {
        auto responseText = res.string().unwrapOr("");
        if (res.ok()) {
            if (debugLogging() || isTest) {
                log::info("Bot accepted {}send for level {}: {}", isTest ? "test " : "", snapshot.levelID, responseText);
            }
            if (requestID > 0) g_feedbackDrafts.erase(requestID);
            if (isTest) {
                bool published = responseText.find("\"published\": true") != std::string::npos ||
                    responseText.find("\"published\":true") != std::string::npos;
                if (published) showAlert(MOD_NAME, "Success: the cloud bot published the test send to Discord.");
                else showAlert(MOD_NAME, "Server accepted the request, but it was not published.\n\nHTTP " +
                    std::to_string(res.code()) + "\n" + (responseText.empty() ? "Empty response" : responseText));
            }
        } else {
            log::warn("Bot bridge rejected {}send for level {} (HTTP {}): {}",
                isTest ? "test " : "", snapshot.levelID, res.code(), responseText.empty() ? "empty response" : responseText);
            if (isTest) showAlert(MOD_NAME, "Test failed.\n\nHTTP " + std::to_string(res.code()) + "\n" +
                (responseText.empty() ? "No response body. Check the API URL and internet connection." : responseText));
        }
    });
}

static void sendTestRequest() {
    SendSnapshot snapshot;
    snapshot.levelID = 2147483001;
    snapshot.stars = 6;
    snapshot.featureState = 1;
    snapshot.hasPlatformer = true;
    snapshot.platformer = false;
    snapshot.levelName = "GD Requests Test";
    snapshot.creator = "Local Test";
    reportSend(snapshot, true, nullptr);
}

static std::string requestURL() {
    std::string mode = g_client.mode.empty() ? "auto" : g_client.mode;
    if (!g_requestBrowserActive) mode = "auto";
    return apiBase() + "/requests?mode=" + mode +
        "&difficulty=" + g_filters.difficulty +
        "&type=" + g_filters.levelType +
        "&status=" + g_filters.status +
        "&minSend=" + g_filters.minSend +
        "&rated=" + g_filters.rated +
        "&sort=" + g_filters.sort +
        "&limit=100";
}

static bool parseRequestsResponse(std::string const& text, std::string& idsCSV) {
    std::istringstream stream(text);
    std::string line;
    std::vector<int> ids;
    g_requestByLevel.clear();
    bool gotMeta = false;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto parts = splitTabs(line);
        if (parts.empty()) continue;
        if (parts[0] == "ERR") return false;
        if (parts[0] == "META" && parts.size() >= 9) {
            g_client.mode = parts[1];
            g_client.serverID = parts[2];
            g_client.userID = parts[3];
            g_client.moderator = parseInt(parts[4]) != 0;
            g_client.helper = parseInt(parts[5]) != 0;
            g_client.reviewer = parseInt(parts[6]) != 0;
            g_client.total = parseInt(parts[7]);
            g_client.returned = parseInt(parts[8]);
            gotMeta = true;
        } else if (parts[0] == "REQ" && parts.size() >= 6) {
            RequestMeta meta;
            meta.requestID = parseInt(parts[1]);
            meta.levelID = parseInt(parts[2]);
            meta.event = parts[3].empty() ? "0" : parts[3];
            meta.difficulty = parseInt(parts[4]);
            meta.rated = parseInt(parts[5]) != 0;
            if (meta.requestID > 0 && meta.levelID > 0) {
                g_requestByLevel[meta.levelID] = meta;
                ids.push_back(meta.levelID);
            }
        }
    }

    if (!gotMeta) return false;
    std::ostringstream out;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i) out << ',';
        out << ids[i];
    }
    idsCSV = out.str();
    return true;
}

static void fetchRequests(LevelSearchLayer* source, LevelBrowserLayer* reloadTarget = nullptr) {
    auto key = connectionKey();
    if (key.empty()) {
        showRequestError("Connection Key is empty. Use /geode-link in Discord and paste the key into this mod's settings.");
        return;
    }

    auto req = web::WebRequest();
    req.header("Authorization", "Bearer " + key);
    req.timeout(std::chrono::seconds(15));
    auto url = requestURL();

    async::spawn(req.get(url), [source, reloadTarget](web::WebResponse res) {
        auto text = res.string().unwrapOr("");
        if (!res.ok()) {
            showRequestError("Could not load server requests.\n\nHTTP " + std::to_string(res.code()) + "\n" +
                (text.empty() ? "Empty response" : text));
            return;
        }

        std::string ids;
        if (!parseRequestsResponse(text, ids)) {
            showRequestError("The request server returned an invalid response.");
            return;
        }
        if (ids.empty()) {
            showAlert(MOD_NAME, "No requests match these filters.");
            return;
        }

        if (reloadTarget && g_requestBrowserActive && reloadTarget == g_requestBrowser && reloadTarget->m_searchObject) {
            reloadTarget->m_searchObject->m_searchQuery = gd::string(ids.c_str());
            reloadTarget->m_searchObject->m_page = 0;
            reloadTarget->loadPage(reloadTarget->m_searchObject);
            return;
        }

        if (!source) return;
        auto* search = source->getSearchObject(SearchType::Search, gd::string(ids.c_str()));
        if (!search) {
            showRequestError("Could not create a Geometry Dash search for these requests.");
            return;
        }
        g_nextBrowserIsRequests = true;
        auto* scene = LevelBrowserLayer::scene(search);
        CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(0.35f, scene));
    });
}

class FeedbackDelegate final : public SetTextPopupDelegate {
public:
    int requestID = 0;

    void setTextPopupClosed(SetTextPopup* popup, gd::string text) override {
        if (!popup || popup->m_cancelled || requestID <= 0) return;
        auto value = gdToStd(text);
        if (value.size() > FEEDBACK_LIMIT) value.resize(FEEDBACK_LIMIT);
        g_feedbackDrafts[requestID] = value;
    }
};

static FeedbackDelegate g_feedbackDelegate;

static void openFeedbackEditor(RequestContext const& context) {
    if (!context.active || context.request.requestID <= 0) return;
    auto current = feedbackFor(context);
    auto* popup = SetTextPopup::create(
        gd::string(current.c_str()),
        "Write feedback...",
        static_cast<int>(FEEDBACK_LIMIT),
        "Request Feedback",
        "Save",
        true,
        0.f
    );
    if (!popup) return;
    g_feedbackDelegate.requestID = context.request.requestID;
    popup->m_delegate = &g_feedbackDelegate;
    if (popup->m_input) {
        popup->m_input->setMaxLabelLength(static_cast<int>(FEEDBACK_LIMIT));
        popup->m_input->setMaxLabelWidth(245.f);
        popup->m_input->setMaxLabelScale(.35f);
    }
    popup->show();
}

static std::vector<std::string> const DIFFICULTIES = {"all", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
static std::vector<std::string> const LEVEL_TYPES = {"all", "classic", "platformer"};
static std::vector<std::string> const STATUSES = {"unchecked", "sent", "rejected", "all"};
static std::vector<std::string> const MIN_SENDS = {"any", "star_rate", "featured", "epic", "legendary", "mythic"};
static std::vector<std::string> const RATED = {"all", "unrated", "rated"};
static std::vector<std::string> const SORTS = {"newest", "oldest", "random"};

static std::string prettyDifficulty(std::string const& v) { return v == "all" ? "Any" : (v == "1" ? "1 star" : v + " stars"); }
static std::string prettyType(std::string const& v) {
    if (v == "classic") return "Classic";
    if (v == "platformer") return "Platformer";
    return "Any";
}
static std::string prettyStatus(std::string const& v) {
    if (v == "unchecked") return "Not checked";
    if (v == "sent") return "Sent";
    if (v == "rejected") return "Rejected";
    return "All";
}
static std::string prettyMinSend(std::string const& v) {
    if (v == "star_rate") return "Rate+";
    if (v == "featured") return "Featured+";
    if (v == "epic") return "Epic+";
    if (v == "legendary") return "Legendary+";
    if (v == "mythic") return "Mythic";
    return "Any";
}
static std::string prettyRated(std::string const& v) {
    if (v == "rated") return "Rated";
    if (v == "unrated") return "Unrated";
    return "Any";
}
static std::string prettySort(std::string const& v) {
    if (v == "oldest") return "Oldest";
    if (v == "random") return "Random";
    return "Newest";
}

template <class T>
static void cycleValue(T& value, std::vector<T> const& values, int direction) {
    auto it = std::find(values.begin(), values.end(), value);
    std::size_t idx = it == values.end() ? 0 : static_cast<std::size_t>(std::distance(values.begin(), it));
    if (direction < 0) idx = idx == 0 ? values.size() - 1 : idx - 1;
    else idx = (idx + 1) % values.size();
    value = values[idx];
}

class RequestFiltersPopup final : public geode::Popup {
protected:
    LevelBrowserLayer* m_owner = nullptr;
    RequestFilters m_working;
    CCLabelBMFont* m_difficulty = nullptr;
    CCLabelBMFont* m_type = nullptr;
    CCLabelBMFont* m_status = nullptr;
    CCLabelBMFont* m_minSend = nullptr;
    CCLabelBMFont* m_rated = nullptr;
    CCLabelBMFont* m_sort = nullptr;
    bool m_staff = false;

    void addText(char const* text, CCPoint position, float scale = .42f, char const* font = "bigFont.fnt") {
        auto* label = CCLabelBMFont::create(text, font);
        label->setScale(scale);
        label->setPosition(position);
        m_mainLayer->addChild(label);
    }

    CCLabelBMFont* addValue(std::string const& text, CCPoint position) {
        auto* label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
        label->setScale(.38f);
        label->setPosition(position);
        m_mainLayer->addChild(label);
        return label;
    }

    void addArrows(float y, SEL_MenuHandler left, SEL_MenuHandler right) {
        auto* leftSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        leftSpr->setScale(.42f);
        auto* leftBtn = CCMenuItemSpriteExtra::create(leftSpr, this, left);
        leftBtn->setPosition({128.f, y});
        m_buttonMenu->addChild(leftBtn);

        auto* rightSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        rightSpr->setFlipX(true);
        rightSpr->setScale(.42f);
        auto* rightBtn = CCMenuItemSpriteExtra::create(rightSpr, this, right);
        rightBtn->setPosition({302.f, y});
        m_buttonMenu->addChild(rightBtn);
    }

    void refresh() {
        if (m_difficulty) m_difficulty->setString(prettyDifficulty(m_working.difficulty).c_str());
        if (m_type) m_type->setString(prettyType(m_working.levelType).c_str());
        if (m_status) m_status->setString(prettyStatus(m_working.status).c_str());
        if (m_minSend) m_minSend->setString(prettyMinSend(m_working.minSend).c_str());
        if (m_rated) m_rated->setString(prettyRated(m_working.rated).c_str());
        if (m_sort) m_sort->setString(prettySort(m_working.sort).c_str());
    }

    bool initFor(LevelBrowserLayer* owner) {
        m_owner = owner;
        m_working = g_filters;
        m_staff = g_client.mode == "helper" || g_client.mode == "moderator";
        float height = m_staff ? 290.f : 215.f;
        if (!Popup::init(430.f, height)) return false;
        setTitle("REQUEST FILTERS");

        float top = height - 62.f;
        auto addRow = [&](char const* title, CCLabelBMFont*& value, float y, SEL_MenuHandler left, SEL_MenuHandler right) {
            addText(title, {72.f, y}, .35f, "goldFont.fnt");
            value = addValue("", {215.f, y});
            addArrows(y, left, right);
        };

        addRow("Difficulty", m_difficulty, top, menu_selector(RequestFiltersPopup::difficultyPrev), menu_selector(RequestFiltersPopup::difficultyNext));
        addRow("Type", m_type, top - 31.f, menu_selector(RequestFiltersPopup::typePrev), menu_selector(RequestFiltersPopup::typeNext));
        float y = top - 62.f;
        if (m_staff) {
            addRow("My status", m_status, y, menu_selector(RequestFiltersPopup::statusPrev), menu_selector(RequestFiltersPopup::statusNext));
            y -= 31.f;
            addRow("My send", m_minSend, y, menu_selector(RequestFiltersPopup::sendPrev), menu_selector(RequestFiltersPopup::sendNext));
            y -= 31.f;
        }
        addRow("Rated", m_rated, y, menu_selector(RequestFiltersPopup::ratedPrev), menu_selector(RequestFiltersPopup::ratedNext));
        y -= 31.f;
        addRow("Sort", m_sort, y, menu_selector(RequestFiltersPopup::sortPrev), menu_selector(RequestFiltersPopup::sortNext));

        auto* resetSpr = ButtonSprite::create("Reset", 60, true, "bigFont.fnt", "GJ_button_04.png", 30.f, .65f);
        auto* resetBtn = CCMenuItemSpriteExtra::create(resetSpr, this, menu_selector(RequestFiltersPopup::onReset));
        resetBtn->setPosition({145.f, 30.f});
        m_buttonMenu->addChild(resetBtn);

        auto* applySpr = ButtonSprite::create("Apply", 60, true, "bigFont.fnt", "GJ_button_01.png", 30.f, .65f);
        auto* applyBtn = CCMenuItemSpriteExtra::create(applySpr, this, menu_selector(RequestFiltersPopup::onApply));
        applyBtn->setPosition({285.f, 30.f});
        m_buttonMenu->addChild(applyBtn);

        refresh();
        return true;
    }

    void difficultyPrev(CCObject*) { cycleValue(m_working.difficulty, DIFFICULTIES, -1); refresh(); }
    void difficultyNext(CCObject*) { cycleValue(m_working.difficulty, DIFFICULTIES, 1); refresh(); }
    void typePrev(CCObject*) { cycleValue(m_working.levelType, LEVEL_TYPES, -1); refresh(); }
    void typeNext(CCObject*) { cycleValue(m_working.levelType, LEVEL_TYPES, 1); refresh(); }
    void statusPrev(CCObject*) { cycleValue(m_working.status, STATUSES, -1); refresh(); }
    void statusNext(CCObject*) { cycleValue(m_working.status, STATUSES, 1); refresh(); }
    void sendPrev(CCObject*) { cycleValue(m_working.minSend, MIN_SENDS, -1); refresh(); }
    void sendNext(CCObject*) { cycleValue(m_working.minSend, MIN_SENDS, 1); refresh(); }
    void ratedPrev(CCObject*) { cycleValue(m_working.rated, RATED, -1); refresh(); }
    void ratedNext(CCObject*) { cycleValue(m_working.rated, RATED, 1); refresh(); }
    void sortPrev(CCObject*) { cycleValue(m_working.sort, SORTS, -1); refresh(); }
    void sortNext(CCObject*) { cycleValue(m_working.sort, SORTS, 1); refresh(); }

    void onReset(CCObject*) {
        m_working = RequestFilters{};
        if (!m_staff) m_working.status = "all";
        refresh();
    }

    void onApply(CCObject*) {
        g_filters = m_working;
        onClose(nullptr);
        fetchRequests(nullptr, m_owner);
    }

public:
    static RequestFiltersPopup* create(LevelBrowserLayer* owner) {
        auto* ret = new RequestFiltersPopup();
        if (ret && ret->initFor(owner)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

static void postRequestAction(RequestContext context, std::string action, std::string reason, SendSnapshot snapshot = {}) {
    auto key = connectionKey();
    if (key.empty() || !context.active || context.request.requestID <= 0) return;

    auto body = matjson::Value();
    body["eventId"] = makeRequestActionEventID(context, action);
    body["requestId"] = context.request.requestID;
    body["mode"] = context.mode;
    body["action"] = action;
    auto feedback = feedbackFor(context);
    if (!feedback.empty()) body["feedback"] = feedback.substr(0, FEEDBACK_LIMIT);

    if (action == "send") {
        body["stars"] = snapshot.stars;
        body["featureState"] = snapshot.featureState;
        body["sendType"] = featureStateToSendType(snapshot.featureState);
        if (snapshot.hasPlatformer) body["platformer"] = snapshot.platformer;
    } else if (action == "reject") {
        body["reason"] = reason;
    }

    auto req = web::WebRequest();
    req.header("Content-Type", "application/json");
    req.header("Authorization", "Bearer " + key);
    req.bodyJSON(body);
    req.timeout(std::chrono::seconds(15));

    int requestID = context.request.requestID;
    async::spawn(req.post(apiBase() + "/request-result"), [requestID, action](web::WebResponse res) {
        auto text = res.string().unwrapOr("");
        if (res.ok()) {
            g_feedbackDrafts.erase(requestID);
            showAlert(MOD_NAME, action == "send" ? "Request send result submitted." : "Request rejection submitted.");
        } else {
            showAlert(MOD_NAME, "Could not submit the request result.\n\nHTTP " + std::to_string(res.code()) + "\n" +
                (text.empty() ? "Empty response" : text));
        }
    });
}

class RejectPopup final : public geode::Popup {
protected:
    RequestContext m_context;
    std::string m_reason;
    std::vector<CCMenuItemSpriteExtra*> m_reasonButtons;

    void updateButtons() {
        static std::vector<std::string> values = {"not_sent", "already_seen", "already_rated", "report"};
        for (std::size_t i = 0; i < m_reasonButtons.size() && i < values.size(); ++i) {
            m_reasonButtons[i]->setColor(values[i] == m_reason ? ccColor3B{255, 255, 255} : ccColor3B{150, 150, 150});
        }
    }

    CCMenuItemSpriteExtra* reasonButton(char const* label, std::string reason, CCPoint pos, SEL_MenuHandler cb) {
        auto* spr = ButtonSprite::create(label, 100, true, "bigFont.fnt", "GJ_button_01.png", 26.f, .55f);
        auto* btn = CCMenuItemSpriteExtra::create(spr, this, cb);
        btn->setPosition(pos);
        btn->setUserObject(CCString::create(reason.c_str()));
        m_buttonMenu->addChild(btn);
        m_reasonButtons.push_back(btn);
        return btn;
    }

    bool initFor(RequestContext const& context) {
        m_context = context;
        if (!Popup::init(430.f, 235.f)) return false;
        setTitle("NOT SENT");

        reasonButton("Not Sent", "not_sent", {120.f, 146.f}, menu_selector(RejectPopup::onReason));
        reasonButton("Already Seen", "already_seen", {310.f, 146.f}, menu_selector(RejectPopup::onReason));
        reasonButton("Already Rated", "already_rated", {120.f, 100.f}, menu_selector(RejectPopup::onReason));
        reasonButton("Report", "report", {310.f, 100.f}, menu_selector(RejectPopup::onReason));

        auto* editSpr = CCSprite::createWithSpriteFrameName("GJ_editBtn_001.png");
        editSpr->setScale(.22f);
        auto* editBtn = CCMenuItemSpriteExtra::create(editSpr, this, menu_selector(RejectPopup::onFeedback));
        editBtn->setPosition({92.f, 43.f});
        editBtn->setID("kolorbok.gd-send-logger/request-feedback-button");
        m_buttonMenu->addChild(editBtn);

        auto* cancelSpr = ButtonSprite::create("Cancel", 70, true, "bigFont.fnt", "GJ_button_04.png", 30.f, .65f);
        auto* cancelBtn = CCMenuItemSpriteExtra::create(cancelSpr, this, menu_selector(RejectPopup::onCancel));
        cancelBtn->setPosition({200.f, 43.f});
        m_buttonMenu->addChild(cancelBtn);

        auto* submitSpr = ButtonSprite::create("Submit", 70, true, "bigFont.fnt", "GJ_button_01.png", 30.f, .65f);
        auto* submitBtn = CCMenuItemSpriteExtra::create(submitSpr, this, menu_selector(RejectPopup::onSubmit));
        submitBtn->setPosition({315.f, 43.f});
        m_buttonMenu->addChild(submitBtn);

        updateButtons();
        return true;
    }

    void onReason(CCObject* sender) {
        auto* node = typeinfo_cast<CCNode*>(sender);
        if (!node) return;
        auto* value = typeinfo_cast<CCString*>(node->getUserObject());
        if (!value) return;
        m_reason = value->getCString();
        updateButtons();
    }

    void onFeedback(CCObject*) { openFeedbackEditor(m_context); }
    void onCancel(CCObject*) { onClose(nullptr); }
    void onSubmit(CCObject*) {
        if (m_reason.empty()) {
            showAlert(MOD_NAME, "Choose a rejection reason first.");
            return;
        }
        auto context = m_context;
        auto reason = m_reason;
        onClose(nullptr);
        postRequestAction(context, "reject", reason);
    }

public:
    static RejectPopup* create(RequestContext const& context) {
        auto* ret = new RejectPopup();
        if (ret && ret->initFor(context)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

} // namespace

$execute {
    listenForSettingChanges<bool>("test-send", [](bool value) {
        if (!value) return;
        sendTestRequest();
        Mod::get()->setSettingValue<bool>("test-send", false);
    });
}

class $modify(GDRequestsLevelSearchLayer, LevelSearchLayer) {
    void onRequests(CCObject*) {
        log::info("[REQUESTS UI] Requests button pressed");
        g_filters = RequestFilters{};
        g_client = ClientState{};
        g_context = RequestContext{};
        g_requestBrowserActive = false;
        g_requestBrowser = nullptr;
        fetchRequests(this, nullptr);
    }

    void installAbsoluteProbeButton() {
        // TEMPORARY DIAGNOSTIC CONTROL.
        // This deliberately ignores every existing menu/layout and is attached straight
        // to LevelSearchLayer with a huge z-order. If this is visible, the hook definitely
        // ran and any missing right-side button is a layout/injection problem.
        if (this->getChildByID("kolorbok.gd-send-logger/requests-probe-menu")) return;

        auto winSize = CCDirector::get()->getWinSize();
        auto* menu = CCMenu::create();
        if (!menu) return;
        menu->setID("kolorbok.gd-send-logger/requests-probe-menu");
        menu->setPosition({0.f, 0.f});
        menu->setContentSize(winSize);
        menu->setAnchorPoint({0.f, 0.f});
        menu->setZOrder(100000);

        auto* sprite = ButtonSprite::create(
            "REQ TEST", 95, true, "bigFont.fnt", "GJ_button_01.png", 30.f, .62f
        );
        if (!sprite) return;

        auto* button = CCMenuItemSpriteExtra::create(
            sprite,
            this,
            menu_selector(GDRequestsLevelSearchLayer::onRequests)
        );
        if (!button) return;
        button->setID("kolorbok.gd-send-logger/requests-probe-button");

        // Bottom-center is intentionally absolute. We do not care about overlap in this
        // diagnostic build; the goal is to prove that LevelSearchLayer::init is reached.
        button->setPosition({winSize.width / 2.f, 28.f});
        menu->addChild(button);
        this->addChild(menu, 100000);

        log::info(
            "[REQUESTS UI] ABSOLUTE REQ TEST installed at x={}, y={}",
            button->getPositionX(), button->getPositionY()
        );
    }

    void installRequestsButton() {
        // This is safe to call repeatedly. NodeIDs::provideFor is the officially
        // documented way to ensure registered IDs exist before another mod uses them.
        NodeIDs::provideFor(this);

        // Node IDs' current LevelSearchLayer provider explicitly maps CCMenu #0 to
        // "other-filter-menu". Use that exact vanilla menu index first, then the ID as fallback.
        auto* menu = this->getChildByType<CCMenu>(0);
        bool usedVanillaFallback = false;
        if (!menu) {
            menu = typeinfo_cast<CCMenu*>(this->getChildByID("other-filter-menu"));
            usedVanillaFallback = menu != nullptr;
        }

        if (!menu) {
            log::error("[REQUESTS UI] LevelSearchLayer CCMenu #0 / other-filter-menu was not found");
            return;
        }

        log::info(
            "[REQUESTS UI] right menu found: id='{}', children={}, layout={}",
            menu->getID(),
            menu->getChildrenCount(),
            menu->getLayout() ? "yes" : "no"
        );

        if (menu->getChildByID("kolorbok.gd-send-logger/requests-button")) return;

        // Use a labelled vanilla GD button for this diagnostic build so it cannot be
        // mistaken for the existing '+' button on the Search screen.
        auto* sprite = ButtonSprite::create(
            "REQ", 48, true, "bigFont.fnt", "GJ_button_01.png", 28.f, .58f
        );
        if (!sprite) {
            log::error("[REQUESTS UI] Could not create REQ ButtonSprite");
            return;
        }

        auto* button = CCMenuItemSpriteExtra::create(
            sprite,
            this,
            menu_selector(GDRequestsLevelSearchLayer::onRequests)
        );
        if (!button) {
            log::error("[REQUESTS UI] Could not create Requests menu item");
            return;
        }

        button->setID("kolorbok.gd-send-logger/requests-button");
        button->setZOrder(1000);
        button->setVisible(true);
        menu->addChild(button);

        if (menu->getLayout()) {
            menu->updateLayout();
        }
        else {
            // Should not happen with Node IDs, but keep the real button visible anyway.
            button->setPosition({menu->getContentSize().width / 2.f, menu->getContentSize().height / 2.f});
        }

        log::info(
            "[REQUESTS UI] right-side REQ installed (fallback={}, children={}, x={}, y={})",
            usedVanillaFallback,
            menu->getChildrenCount(),
            button->getPositionX(),
            button->getPositionY()
        );
    }

    bool init(int type) {
        log::info("[REQUESTS UI] ENTER LevelSearchLayer::init(type={})", type);
        if (!LevelSearchLayer::init(type)) {
            log::error("[REQUESTS UI] base LevelSearchLayer::init returned false");
            return false;
        }

        log::info(
            "[REQUESTS UI] AFTER BASE LevelSearchLayer::init: direct children={}",
            this->getChildrenCount()
        );

        // Install both. The absolute probe is temporary and intentionally ignores layouts.
        this->installAbsoluteProbeButton();
        this->installRequestsButton();

        // Run again on the next main-thread turn in case another post-init hook rebuilds
        // the Search UI after us.
        this->retain();
        geode::queueInMainThread([self = this]() {
            log::info("[REQUESTS UI] queued LevelSearchLayer UI verification");
            self->installAbsoluteProbeButton();
            self->installRequestsButton();
            self->release();
        });
        return true;
    }
};

// Independent diagnostic hook. This does NOT depend on LevelSearchLayer or Node IDs.
// If REQ HOOK is visible on the Geometry Dash main menu, this build is installed and
// our modify hooks are being claimed correctly. Pressing it opens the vanilla Search UI.
class $modify(GDRequestsMenuLayerProbe, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto winSize = CCDirector::get()->getWinSize();
        auto* menu = CCMenu::create();
        if (!menu) return true;
        menu->setID("kolorbok.gd-send-logger/main-probe-menu");
        menu->setPosition({0.f, 0.f});
        menu->setContentSize(winSize);
        menu->setAnchorPoint({0.f, 0.f});
        menu->setZOrder(100000);

        auto* sprite = ButtonSprite::create(
            "REQ 2.0.4", 115, true, "bigFont.fnt", "GJ_button_04.png", 30.f, .60f
        );
        if (!sprite) return true;

        auto* button = CCMenuItemSpriteExtra::create(
            sprite,
            this,
            menu_selector(GDRequestsMenuLayerProbe::onRequestsProbe)
        );
        if (!button) return true;
        button->setID("kolorbok.gd-send-logger/main-probe-button");
        button->setPosition({winSize.width / 2.f, 22.f});
        menu->addChild(button);
        this->addChild(menu, 100000);

        log::info("[REQUESTS UI] REQ HOOK main-menu probe installed");
        return true;
    }

    void onRequestsProbe(CCObject*) {
        log::info("[REQUESTS UI] REQ HOOK pressed; opening vanilla LevelSearchLayer");
        auto* scene = LevelSearchLayer::scene(0);
        if (!scene) {
            showAlert("GD Requests", "LevelSearchLayer::scene(0) returned null.");
            return;
        }
        CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(.25f, scene));
    }
};

class $modify(GDRequestsLevelBrowserLayer, LevelBrowserLayer) {
    bool init(GJSearchObject* searchObj) {
        if (!LevelBrowserLayer::init(searchObj)) return false;
        NodeIDs::provideFor(this);

        if (g_nextBrowserIsRequests) {
            g_nextBrowserIsRequests = false;
            g_requestBrowserActive = true;
            g_requestBrowser = this;

            if (auto* menu = typeinfo_cast<CCMenu*>(getChildByID("search-menu"))) {
                auto* sprite = CCSprite::createWithSpriteFrameName("GJ_plusBtn_001.png");
                sprite->setScale(.7f);
                auto* button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(GDRequestsLevelBrowserLayer::onRequestFilters));
                button->setID("kolorbok.gd-send-logger/request-filter-button");
                menu->addChild(button);
                menu->updateLayout();
            }
        }
        return true;
    }

    gd::string getSearchTitle() {
        if (g_requestBrowserActive && g_requestBrowser == this) return "Server Requests";
        return LevelBrowserLayer::getSearchTitle();
    }

    void onRequestFilters(CCObject*) {
        if (auto* popup = RequestFiltersPopup::create(this)) popup->show();
    }

    void onBack(CCObject* sender) {
        bool wasRequestBrowser = g_requestBrowserActive && g_requestBrowser == this;
        if (wasRequestBrowser) {
            g_requestBrowserActive = false;
            g_requestBrowser = nullptr;
            g_requestByLevel.clear();
            g_context = RequestContext{};
        }
        LevelBrowserLayer::onBack(sender);
    }
};

class $modify(GDRequestsLevelInfoLayer, LevelInfoLayer) {
    struct Fields {
        RequestContext requestContext;
    };

    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;
        NodeIDs::provideFor(this);

        m_fields->requestContext = RequestContext{};
        if (g_requestBrowserActive && g_requestBrowser && level) {
            int levelID = level->m_levelID;
            auto it = g_requestByLevel.find(levelID);
            if (it != g_requestByLevel.end()) {
                m_fields->requestContext.active = true;
                m_fields->requestContext.request = it->second;
                m_fields->requestContext.mode = g_client.mode;
                g_context = m_fields->requestContext;
            }
        }

        if (!m_fields->requestContext.active) return true;
        if (m_fields->requestContext.mode != "helper" && m_fields->requestContext.mode != "moderator") return true;

        auto* menu = typeinfo_cast<CCMenu*>(getChildByID("left-side-menu"));
        if (!menu) return true;

        if (m_fields->requestContext.mode == "helper") {
            // A real GD moderator can also hold the Discord Helper role. In Helper mode,
            // remove the vanilla RobTop button on this scene and replace it with our
            // server-only lookalike so an accidental real send is impossible.
            if (auto* vanillaModButton = menu->getChildByID("mod-rate-button")) {
                menu->removeChild(vanillaModButton, true);
            }
            auto* fakeSprite = CCSprite::createWithSpriteFrameName("GJ_starBtnMod_001.png");
            fakeSprite->setScale(.8f);
            auto* fakeBtn = CCMenuItemSpriteExtra::create(fakeSprite, this, menu_selector(GDRequestsLevelInfoLayer::onHelperSend));
            fakeBtn->setID("kolorbok.gd-send-logger/helper-send-button");
            menu->addChild(fakeBtn);
        }

        auto* rejectSprite = CCSprite::createWithSpriteFrameName("GJ_dislikeBtn_001.png");
        rejectSprite->setScale(.48f);
        auto* rejectBtn = CCMenuItemSpriteExtra::create(rejectSprite, this, menu_selector(GDRequestsLevelInfoLayer::onRejectRequest));
        rejectBtn->setID("kolorbok.gd-send-logger/request-reject-button");
        menu->addChild(rejectBtn);
        menu->updateLayout();
        return true;
    }

    void onHelperSend(CCObject*) {
        if (!m_fields->requestContext.active || m_fields->requestContext.mode != "helper" || !m_level) return;
        g_context = m_fields->requestContext;
        g_creatingHelperPopup = true;
        auto* popup = RateStarsLayer::create(m_level->m_levelID, m_level->isPlatformer(), true);
        g_creatingHelperPopup = false;
        if (popup) popup->show();
    }

    void onRejectRequest(CCObject*) {
        if (!m_fields->requestContext.active) return;
        g_context = m_fields->requestContext;
        if (auto* popup = RejectPopup::create(m_fields->requestContext)) popup->show();
    }
};

class $modify(GDRequestsRateStarsLayer, RateStarsLayer) {
    struct Fields {
        bool helperRequestPopup = false;
        RequestContext requestContext;
        CCMenuItemSpriteExtra* feedbackButton = nullptr;
    };

    static void onModify(auto& self) {
        if (!self.getHook("RateStarsLayer::onRate")) log::error("GD Requests: failed to register RateStarsLayer::onRate hook");
        if (!self.getHook("RateStarsLayer::uploadActionFinished")) log::error("GD Requests: failed to register uploadActionFinished hook");
        if (!self.getHook("RateStarsLayer::uploadActionFailed")) log::error("GD Requests: failed to register uploadActionFailed hook");

        if (Loader::get()->isModInstalled("bitz.fakegdmod")) {
            auto priorityResult = self.setHookPriorityBeforePre("RateStarsLayer::onRate", "bitz.fakegdmod");
            if (!priorityResult) log::error("GD Requests: FakeGDMod detected, but failed to order onRate before it");
            else log::info("GD Requests: FakeGDMod compatibility armed");
        }
    }

    bool init(int levelID, bool platformer, bool moderator) {
        bool helperPopup = g_creatingHelperPopup;
        RequestContext captured;
        if (g_context.active && g_context.request.levelID == levelID &&
            (g_context.mode == "helper" || g_context.mode == "moderator")) {
            captured = g_context;
        }

        if (!RateStarsLayer::init(levelID, platformer, moderator)) return false;
        m_fields->helperRequestPopup = helperPopup && captured.active && captured.mode == "helper";
        m_fields->requestContext = captured;

        if (captured.active) {
            // Keep the feedback control inside the rate popup. We attach it to the popup's
            // own button menu; request/reject/helper buttons on shared GD screens use NodeIDs
            // + layouts, while this popup is isolated from LevelInfo/Search menu layouts.
            auto* sprite = CCSprite::createWithSpriteFrameName("GJ_editBtn_001.png");
            sprite->setScale(.20f);
            auto* button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(GDRequestsRateStarsLayer::onRequestFeedback));
            button->setID("kolorbok.gd-send-logger/request-feedback-button");
            if (m_submitButton && m_submitButton->getParent()) {
                auto* parent = m_submitButton->getParent();
                button->setPosition({m_submitButton->getPositionX() - 170.f, m_submitButton->getPositionY()});
                parent->addChild(button);
                if (auto* menu = typeinfo_cast<CCMenu*>(parent)) {
                    if (menu->getLayout()) menu->updateLayout();
                }
            } else if (m_buttonMenu) {
                button->setPosition({75.f, 35.f});
                m_buttonMenu->addChild(button);
            }
            m_fields->feedbackButton = button;
        }
        return true;
    }

    void onRequestFeedback(CCObject*) {
        if (m_fields->requestContext.active) openFeedbackEditor(m_fields->requestContext);
    }

    void onRate(CCObject* sender) {
        if (m_fields->helperRequestPopup) {
            auto snapshot = captureSend(this);
            if (snapshot.levelID <= 0 || snapshot.stars <= 0 || snapshot.stars > 10) {
                showAlert(MOD_NAME, "Choose a difficulty before submitting the helper result.");
                return;
            }
            auto context = m_fields->requestContext;
            postRequestAction(context, "send", "", snapshot);
            onClose(nullptr);
            return;
        }

        bool fakeSend = fakeGDModWillSimulateSend(this);
        SendSnapshot fakeSnapshot;
        RequestContext captured = m_fields->requestContext;
        if (fakeSend) fakeSnapshot = captureSend(this);

        RateStarsLayer::onRate(sender);

        if (fakeSend) {
            reportSend(fakeSnapshot, false, captured.active && captured.mode == "moderator" ? &captured : nullptr);
        }
    }

    void uploadActionFinished(int id, int response) override {
        bool wasModerator = m_moderator;
        auto snapshot = captureSend(this);
        auto captured = m_fields->requestContext;

        if (debugLogging()) {
            log::info("RateStarsLayer::uploadActionFinished id={}, response={}, moderator={}", id, response, wasModerator);
        }

        if (wasModerator && !m_fields->helperRequestPopup) {
            reportSend(snapshot, false, captured.active && captured.mode == "moderator" ? &captured : nullptr);
        }
        RateStarsLayer::uploadActionFinished(id, response);
    }

    void uploadActionFailed(int id, int response) override {
        if (m_moderator && debugLogging()) {
            log::warn("Moderator send failed in GD: id={}, response={}, levelID={}, stars={}, featureState={}",
                id, response, m_levelID, m_starsRate, m_featureState);
        }
        RateStarsLayer::uploadActionFailed(id, response);
    }
};
