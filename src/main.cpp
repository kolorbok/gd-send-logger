#include <Geode/Geode.hpp>
#include <Geode/modify/RateStarsLayer.hpp>
#include <Geode/modify/LevelSearchLayer.hpp>
#include <Geode/modify/LevelBrowserLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GJSearchObject.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/ui/TextInput.hpp>
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
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <functional>
#include <sstream>
#include <random>
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
    // Empty = any difficulty. Otherwise every key in this vector is accepted.
    std::vector<std::string> difficulties;
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
    std::string difficultyKey;
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
static std::vector<RequestMeta> g_requestList;
static RequestMeta g_selectedRequest;
static bool g_hasSelectedRequest = false;
static std::unordered_map<int, std::string> g_feedbackDrafts;
static std::unordered_map<int, bool> g_noPingDrafts;
static bool g_nextBrowserIsRequests = false;
static bool g_requestBrowserActive = false;
static LevelBrowserLayer* g_requestBrowser = nullptr;
static bool g_creatingHelperPopup = false;
static std::size_t g_requestNativeBatch = 0;
constexpr std::size_t REQUEST_NATIVE_BATCH_SIZE = 100;

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

static std::string upperCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
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

static bool noPingFor(RequestContext const& context) {
    if (!context.active || context.request.requestID <= 0) return false;
    auto it = g_noPingDrafts.find(context.request.requestID);
    return it != g_noPingDrafts.end() && it->second;
}

static void setNoPingFor(RequestContext const& context, bool value) {
    if (!context.active || context.request.requestID <= 0) return;
    g_noPingDrafts[context.request.requestID] = value;
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
        body["noPing"] = noPingFor(*context);
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
            if (requestID > 0) {
                g_feedbackDrafts.erase(requestID);
                g_noPingDrafts.erase(requestID);
            }
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

    // Fetch a stable superset and apply the fields present in REQ rows locally.
    // This is important for Random: the server must not randomize a capped prefix before
    // the client gets the full matching request set.
    return apiBase() + "/requests?mode=" + mode +
        "&difficulty=all" +
        "&type=" + g_filters.levelType +
        "&status=" + g_filters.status +
        "&minSend=" + g_filters.minSend +
        "&rated=all" +
        "&sort=newest" +
        "&limit=50000";
}

static std::string normalizeRequestDifficultyKey(std::string raw, int stars) {
    raw = trim(raw);
    std::transform(raw.begin(), raw.end(), raw.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (stars >= 1 && stars <= 9) return std::to_string(stars);
    if (stars != 10) return "";

    if (raw.find("easy demon") != std::string::npos || raw == "demon-easy") return "demon-easy";
    if (raw.find("medium demon") != std::string::npos || raw == "demon-medium") return "demon-medium";
    if (raw.find("hard demon") != std::string::npos || raw == "demon-hard") return "demon-hard";
    if (raw.find("insane demon") != std::string::npos || raw == "demon-insane") return "demon-insane";
    if (raw.find("extreme demon") != std::string::npos || raw == "demon-extreme") return "demon-extreme";
    return "demon";
}

static bool requestMetaMatchesLocalFilters(RequestMeta const& meta) {
    if (meta.event != "0") return false;

    if (!g_filters.difficulties.empty() &&
        std::find(g_filters.difficulties.begin(), g_filters.difficulties.end(), meta.difficultyKey) == g_filters.difficulties.end()) {
        return false;
    }

    if (g_filters.rated == "rated" && !meta.rated) return false;
    if (g_filters.rated == "unrated" && meta.rated) return false;
    return true;
}

static void applyLocalRequestOrdering() {
    if (g_filters.sort == "oldest") {
        std::stable_sort(g_requestList.begin(), g_requestList.end(), [](auto const& a, auto const& b) {
            return a.requestID < b.requestID;
        });
    } else if (g_filters.sort == "random") {
        static std::mt19937 rng(std::random_device{}());
        std::shuffle(g_requestList.begin(), g_requestList.end(), rng);
    } else {
        std::stable_sort(g_requestList.begin(), g_requestList.end(), [](auto const& a, auto const& b) {
            return a.requestID > b.requestID;
        });
    }
}

static bool parseRequestsResponse(std::string const& text) {
    std::istringstream stream(text);
    std::string line;
    g_requestByLevel.clear();
    g_requestList.clear();
    g_requestNativeBatch = 0;
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
            meta.event = trim(parts[3].empty() ? "0" : parts[3]);
            meta.difficulty = parseInt(parts[4]);
            meta.difficultyKey = normalizeRequestDifficultyKey(parts.size() >= 7 ? parts[6] : "", meta.difficulty);
            meta.rated = parseInt(parts[5]) != 0;

            if (!requestMetaMatchesLocalFilters(meta)) continue;

            if (meta.requestID > 0 && meta.levelID > 0) {
                g_requestList.push_back(meta);
            }
        }
    }

    applyLocalRequestOrdering();

    // Rebuild lookup after ordering so duplicate Level IDs resolve to the request that is
    // actually shown first for the selected sort mode.
    for (auto const& meta : g_requestList) {
        if (!g_requestByLevel.contains(meta.levelID)) g_requestByLevel.emplace(meta.levelID, meta);
    }
    return gotMeta;
}

static std::vector<int> requestLevelIDs() {
    std::vector<int> ids;
    std::unordered_map<int, bool> seen;
    ids.reserve(g_requestList.size());
    for (auto const& meta : g_requestList) {
        if (meta.levelID <= 0 || meta.event != "0" || seen.contains(meta.levelID)) continue;
        seen.emplace(meta.levelID, true);
        ids.push_back(meta.levelID);
    }
    return ids;
}

static std::size_t requestNativeBatchCount() {
    auto count = requestLevelIDs().size();
    return count == 0 ? 0 : (count + REQUEST_NATIVE_BATCH_SIZE - 1) / REQUEST_NATIVE_BATCH_SIZE;
}

static bool hasNextRequestNativeBatch() {
    auto count = requestNativeBatchCount();
    return count > 0 && g_requestNativeBatch + 1 < count;
}

static bool hasPrevRequestNativeBatch() {
    return g_requestNativeBatch > 0;
}

static std::string requestNativeBatchCSV(std::size_t batch) {
    auto ids = requestLevelIDs();
    if (ids.empty()) return "";
    auto begin = batch * REQUEST_NATIVE_BATCH_SIZE;
    if (begin >= ids.size()) return "";
    auto end = std::min(ids.size(), begin + REQUEST_NATIVE_BATCH_SIZE);

    std::string out;
    for (std::size_t i = begin; i < end; ++i) {
        if (!out.empty()) out += ",";
        out += std::to_string(ids[i]);
    }
    return out;
}

static GJSearchObject* makeRequestNativeBatchSearch(std::size_t batch) {
    auto ids = requestNativeBatchCSV(batch);
    if (ids.empty()) return nullptr;
    return GJSearchObject::create(static_cast<SearchType>(19), gd::string(ids.c_str()));
}

// Requests are loaded by RequestsHubPopup below. Keeping the HTTP request tied to a
// visible popup gives the user immediate Loading / Connected / Error feedback and avoids
// capturing a LevelSearchLayer pointer across an asynchronous request.

// A dedicated targeted-touch layer is used over the feedback field instead of a CCMenuItem.
// This matters on both desktop and mobile: the layer receives every click/tap (and drags),
// while the hidden TextInput remains the single owner of IME / keyboard editing.
class FeedbackTouchLayer final : public CCLayer {
protected:
    std::function<void(CCPoint const&)> m_onPoint;
    std::function<void(cocos2d::enumKeyCodes)> m_onKey;

    bool initFor(
        CCSize const& size,
        std::function<void(CCPoint const&)> onPoint,
        std::function<void(cocos2d::enumKeyCodes)> onKey
    ) {
        if (!CCLayer::init()) return false;
        m_onPoint = std::move(onPoint);
        m_onKey = std::move(onKey);
        setContentSize(size);
        setAnchorPoint({0.f, 0.f});
        setTouchEnabled(true);
        setKeyboardEnabled(true);
        return true;
    }

    bool pointInside(CCPoint const& local) const {
        auto size = getContentSize();
        return local.x >= 0.f && local.y >= 0.f && local.x <= size.width && local.y <= size.height;
    }

public:
    static FeedbackTouchLayer* create(
        CCSize const& size,
        std::function<void(CCPoint const&)> onPoint,
        std::function<void(cocos2d::enumKeyCodes)> onKey
    ) {
        auto* ret = new FeedbackTouchLayer();
        if (ret && ret->initFor(size, std::move(onPoint), std::move(onKey))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    void registerWithTouchDispatcher() override {
        // Cocos translates mouse clicks and mobile touches into the same targeted-touch path.
        // Only gestures beginning inside the editor are swallowed, so popup buttons stay normal.
        CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, -1000, true);
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        if (!touch || !isVisible()) return false;
        auto local = convertToNodeSpace(touch->getLocation());
        if (!pointInside(local)) return false;
        if (m_onPoint) m_onPoint(local);
        return true;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent*) override {
        if (!touch || !m_onPoint) return;
        auto local = convertToNodeSpace(touch->getLocation());
        auto size = getContentSize();
        local.x = std::clamp(local.x, 0.f, size.width);
        local.y = std::clamp(local.y, 0.f, size.height);
        m_onPoint(local);
    }

    void keyDown(cocos2d::enumKeyCodes key, double) override {
        if (m_onKey) m_onKey(key);
    }
};

class FeedbackPopup final : public geode::Popup {
protected:
    struct WrappedLine {
        std::string text;
        std::size_t startByte = 0;
        std::size_t endByte = 0;
    };

    RequestContext m_context;
    geode::TextInput* m_input = nullptr;
    CCLabelBMFont* m_measureLabel = nullptr;
    CCLabelBMFont* m_counter = nullptr;
    CCLabelBMFont* m_placeholder = nullptr;
    FeedbackTouchLayer* m_touchLayer = nullptr;
    CCLayerColor* m_caret = nullptr;
    std::vector<CCLabelBMFont*> m_lineLabels;
    std::string m_value;
    // The hidden TextInput is only an IME transport. m_value + m_cursorByte are authoritative.
    // Keeping editing state here avoids relying on CCTextInputNode's single-line cursor geometry
    // for a custom wrapped editor.
    std::string m_transportValue;
    bool m_resettingTransport = false;
    std::size_t m_cursorByte = 0;
    bool m_focused = false;
    bool m_cursorInitialized = false;
    std::size_t m_firstVisibleLine = 0;
    std::size_t m_visibleLineCount = 1;

    static constexpr float FIELD_W = 250.f;
    static constexpr float FIELD_H = 94.f;
    static constexpr float FIELD_X = 45.f;
    static constexpr float FIELD_Y = 66.f;
    static constexpr float TEXT_LEFT = FIELD_X + 10.f;
    static constexpr float TEXT_TOP = FIELD_Y + FIELD_H - 14.f;
    static constexpr float TEXT_SCALE = .58f;
    static constexpr float LINE_STEP = 11.2f;
    static constexpr std::size_t MAX_VISIBLE_LINES = 7;

    float measuredRawWidth(std::string const& text) {
        if (!m_measureLabel || text.empty()) return 0.f;
        m_measureLabel->setString(text.c_str());
        return m_measureLabel->getContentSize().width;
    }

    float measuredWidth(std::string const& text) {
        return measuredRawWidth(text) * TEXT_SCALE;
    }

    static std::size_t nextUtf8Boundary(std::string const& text, std::size_t index) {
        if (index >= text.size()) return text.size();
        auto lead = static_cast<unsigned char>(text[index]);
        std::size_t step = 1;
        if ((lead & 0xE0) == 0xC0) step = 2;
        else if ((lead & 0xF0) == 0xE0) step = 3;
        else if ((lead & 0xF8) == 0xF0) step = 4;
        return std::min(text.size(), index + step);
    }

    static std::vector<std::size_t> utf8Boundaries(std::string const& text) {
        std::vector<std::size_t> result;
        result.reserve(text.size() + 1);
        result.push_back(0);
        for (std::size_t i = 0; i < text.size();) {
            i = nextUtf8Boundary(text, i);
            result.push_back(i);
        }
        return result;
    }

    static std::size_t utf8CharCount(std::string const& text) {
        auto boundaries = utf8Boundaries(text);
        return boundaries.empty() ? 0 : boundaries.size() - 1;
    }

    static void truncateUtf8ToChars(std::string& text, std::size_t maxChars) {
        auto boundaries = utf8Boundaries(text);
        if (boundaries.size() <= maxChars + 1) return;
        text.resize(boundaries[maxChars]);
    }



    std::size_t nearestBoundaryInLine(std::string const& text, float wantedX) {
        wantedX = std::max(0.f, wantedX);
        auto boundaries = utf8Boundaries(text);
        if (boundaries.empty() || text.empty()) return 0;

        std::size_t bestByte = 0;
        float bestDistance = std::fabs(wantedX);
        for (auto boundary : boundaries) {
            auto width = measuredWidth(text.substr(0, boundary));
            auto distance = std::fabs(width - wantedX);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestByte = boundary;
            }
            if (width > wantedX && distance > bestDistance) break;
        }
        return bestByte;
    }

    std::vector<WrappedLine> wrapText(std::string const& text) {
        std::vector<WrappedLine> lines;
        std::string current;
        std::size_t lineStart = 0;
        constexpr float maxWidth = FIELD_W - 20.f;

        auto pushCurrent = [&](std::size_t endByte) {
            lines.push_back({current, lineStart, endByte});
            current.clear();
        };

        for (std::size_t i = 0; i < text.size();) {
            auto next = nextUtf8Boundary(text, i);
            auto token = text.substr(i, next - i);

            if (token == "\r") {
                i = next;
                continue;
            }
            if (token == "\n") {
                pushCurrent(i);
                lineStart = next;
                i = next;
                continue;
            }

            auto candidate = current + token;
            if (!current.empty() && measuredWidth(candidate) > maxWidth) {
                pushCurrent(i);
                lineStart = i;
                current = token;
            } else {
                current = std::move(candidate);
            }
            i = next;
        }

        // Always keep the final visual line, including an empty line after a trailing newline.
        lines.push_back({current, lineStart, text.size()});
        if (lines.empty()) lines.push_back({"", 0, 0});
        return lines;
    }

    void clearRenderedLines() {
        for (auto* label : m_lineLabels) {
            if (label) label->removeFromParentAndCleanup(true);
        }
        m_lineLabels.clear();
    }

    std::size_t cursorLineIndex(std::vector<WrappedLine> const& lines) const {
        if (lines.empty()) return 0;
        auto cursor = std::min(m_cursorByte, m_value.size());
        std::size_t result = 0;
        // Pick the last line whose start is <= cursor. This intentionally maps an exact
        // soft-wrap boundary to the beginning of the next line rather than the end of the old one.
        for (std::size_t i = 1; i < lines.size(); ++i) {
            if (lines[i].startByte <= cursor) result = i;
            else break;
        }
        return result;
    }

    void renderText() {
        if (!m_mainLayer) return;
        clearRenderedLines();

        auto lines = wrapText(m_value);
        auto visibleCount = std::min<std::size_t>(MAX_VISIBLE_LINES, lines.size());
        auto caretLine = cursorLineIndex(lines);

        std::size_t firstVisible = 0;
        if (lines.size() > visibleCount) {
            if (m_focused) {
                firstVisible = caretLine >= visibleCount ? caretLine - visibleCount + 1 : 0;
                firstVisible = std::min(firstVisible, lines.size() - visibleCount);
            } else {
                firstVisible = lines.size() - visibleCount;
            }
        }
        m_firstVisibleLine = firstVisible;
        m_visibleLineCount = std::max<std::size_t>(1, visibleCount);

        for (std::size_t row = 0; row < visibleCount; ++row) {
            auto const& line = lines[firstVisible + row];
            auto* label = CCLabelBMFont::create(line.text.c_str(), "chatFont.fnt");
            if (!label) continue;
            label->setScale(TEXT_SCALE);
            label->setAnchorPoint({0.f, 1.f});
            label->setPosition({TEXT_LEFT, TEXT_TOP - static_cast<float>(row) * LINE_STEP});
            label->setColor(ccc3(255, 255, 255));
            m_mainLayer->addChild(label, 5);
            m_lineLabels.push_back(label);
        }

        if (m_caret) {
            auto caretVisible = m_focused && caretLine >= firstVisible && caretLine < firstVisible + visibleCount;
            m_caret->setVisible(caretVisible);
            if (caretVisible) {
                auto const& line = lines[caretLine];
                auto cursor = std::min(m_cursorByte, m_value.size());
                auto prefixBytes = cursor > line.startByte ? cursor - line.startByte : 0;
                prefixBytes = std::min(prefixBytes, line.text.size());
                auto prefix = line.text.substr(0, prefixBytes);
                auto row = caretLine - firstVisible;
                // Keep the custom caret on the exact text advance. A tiny positive inset
                // avoids antialiasing overlap without creating the old 1.5 px visual gap.
                auto x = TEXT_LEFT + measuredWidth(prefix) - .15f;
                auto y = TEXT_TOP - static_cast<float>(row) * LINE_STEP - 8.9f;
                x = std::clamp(x, TEXT_LEFT, FIELD_X + FIELD_W - 7.f);
                y = std::clamp(y, FIELD_Y + 5.f, TEXT_TOP - 2.f);
                m_caret->setPosition({x, y});
            }
        }
    }

    void refreshVisuals() {
        if (m_counter) {
            auto counterText = std::to_string(utf8CharCount(m_value)) + "/" + std::to_string(FEEDBACK_LIMIT);
            m_counter->setString(counterText.c_str());
        }
        renderText();
        if (m_placeholder) m_placeholder->setVisible(m_value.empty());
    }

    std::size_t previousBoundary(std::size_t byteOffset) const {
        byteOffset = std::min(byteOffset, m_value.size());
        if (byteOffset == 0) return 0;
        auto i = byteOffset - 1;
        while (i > 0 && (static_cast<unsigned char>(m_value[i]) & 0xC0) == 0x80) --i;
        return i;
    }

    std::size_t nextBoundary(std::size_t byteOffset) const {
        return nextUtf8Boundary(m_value, std::min(byteOffset, m_value.size()));
    }

    void resetTransportToValue() {
        if (!m_input) return;
        m_resettingTransport = true;
        m_input->setString(gd::string(m_value.c_str()), false);
        m_transportValue = m_value;
        m_resettingTransport = false;
    }

    void consumeTransportMutation(std::string const& incomingValue) {
        if (m_resettingTransport) return;

        // Native TextInput is intentionally NOT our cursor model. It may keep its own
        // single-line insertion point (and different platforms/IME implementations move it
        // differently). We only extract WHAT changed, then apply that edit at our multiline
        // cursor. This also removes the repeated-character ambiguity from the old caret diff.
        auto const oldTransport = m_transportValue;
        auto const& nextTransport = incomingValue;

        std::size_t prefix = 0;
        while (
            prefix < oldTransport.size() &&
            prefix < nextTransport.size() &&
            oldTransport[prefix] == nextTransport[prefix]
        ) {
            ++prefix;
        }
        while (
            prefix > 0 &&
            ((prefix < oldTransport.size() && (static_cast<unsigned char>(oldTransport[prefix]) & 0xC0) == 0x80) ||
             (prefix < nextTransport.size() && (static_cast<unsigned char>(nextTransport[prefix]) & 0xC0) == 0x80))
        ) {
            --prefix;
        }

        std::size_t suffix = 0;
        while (
            suffix < oldTransport.size() - std::min(prefix, oldTransport.size()) &&
            suffix < nextTransport.size() - std::min(prefix, nextTransport.size()) &&
            oldTransport[oldTransport.size() - 1 - suffix] == nextTransport[nextTransport.size() - 1 - suffix]
        ) {
            ++suffix;
        }
        // Do not cut through a UTF-8 codepoint at the beginning of the preserved suffix.
        while (suffix > 0) {
            auto oldIndex = oldTransport.size() - suffix;
            auto newIndex = nextTransport.size() - suffix;
            bool oldContinuation = oldIndex < oldTransport.size() &&
                (static_cast<unsigned char>(oldTransport[oldIndex]) & 0xC0) == 0x80;
            bool newContinuation = newIndex < nextTransport.size() &&
                (static_cast<unsigned char>(nextTransport[newIndex]) & 0xC0) == 0x80;
            if (!oldContinuation && !newContinuation) break;
            --suffix;
        }

        auto removed = oldTransport.substr(prefix, oldTransport.size() - prefix - suffix);
        auto inserted = nextTransport.substr(prefix, nextTransport.size() - prefix - suffix);

        auto removeChars = utf8CharCount(removed);
        while (removeChars-- > 0 && m_cursorByte > 0) {
            auto begin = previousBoundary(m_cursorByte);
            m_value.erase(begin, m_cursorByte - begin);
            m_cursorByte = begin;
        }

        if (!inserted.empty()) {
            auto currentChars = utf8CharCount(m_value);
            auto room = currentChars >= FEEDBACK_LIMIT ? std::size_t(0) : FEEDBACK_LIMIT - currentChars;
            truncateUtf8ToChars(inserted, room);
            if (!inserted.empty()) {
                m_value.insert(m_cursorByte, inserted);
                m_cursorByte += inserted.size();
            }
        }

        resetTransportToValue();
        refreshVisuals();
    }

    void syncValueFromInput() {
        if (!m_input || m_resettingTransport) return;
        auto incoming = gdToStd(m_input->getString());
        if (incoming != m_transportValue) consumeTransportMutation(incoming);
    }

    void placeCursorFromFieldPoint(CCPoint const& local) {
        if (!m_input) return;
        focusInput();

        auto lines = wrapText(m_value);
        if (lines.empty()) return;

        auto first = std::min(m_firstVisibleLine, lines.size() - 1);
        auto count = std::min<std::size_t>(m_visibleLineCount, lines.size() - first);
        if (count == 0) count = 1;

        auto firstLineY = (TEXT_TOP - FIELD_Y) - 4.8f;
        auto rowFloat = (firstLineY - local.y) / LINE_STEP;
        auto rowSigned = static_cast<long>(std::lround(rowFloat));
        rowSigned = std::clamp<long>(rowSigned, 0, static_cast<long>(count - 1));
        auto lineIndex = first + static_cast<std::size_t>(rowSigned);
        auto const& line = lines[lineIndex];

        auto wantedX = local.x - (TEXT_LEFT - FIELD_X);
        auto inLineByte = nearestBoundaryInLine(line.text, wantedX);
        m_cursorByte = std::min(line.startByte + inLineByte, m_value.size());
        m_cursorInitialized = true;
        refreshVisuals();
    }

    void moveCursorVertical(int direction) {
        auto lines = wrapText(m_value);
        if (lines.empty()) return;
        auto currentLine = cursorLineIndex(lines);
        auto targetSigned = static_cast<long>(currentLine) + direction;
        targetSigned = std::clamp<long>(targetSigned, 0, static_cast<long>(lines.size() - 1));
        auto targetLine = static_cast<std::size_t>(targetSigned);
        if (targetLine == currentLine) return;

        auto const& current = lines[currentLine];
        auto currentInLine = m_cursorByte > current.startByte ? m_cursorByte - current.startByte : 0;
        currentInLine = std::min(currentInLine, current.text.size());
        auto wantedX = measuredWidth(current.text.substr(0, currentInLine));

        auto const& target = lines[targetLine];
        m_cursorByte = std::min(target.startByte + nearestBoundaryInLine(target.text, wantedX), m_value.size());
        renderText();
    }

    void handleEditorKey(cocos2d::enumKeyCodes key) {
        if (!m_focused) return;
        bool changed = false;
        if (key == cocos2d::KEY_Left) {
            auto next = previousBoundary(m_cursorByte);
            changed = next != m_cursorByte;
            m_cursorByte = next;
        } else if (key == cocos2d::KEY_Right) {
            auto next = nextBoundary(m_cursorByte);
            changed = next != m_cursorByte;
            m_cursorByte = next;
        } else if (key == cocos2d::KEY_Home) {
            auto lines = wrapText(m_value);
            if (!lines.empty()) {
                auto line = cursorLineIndex(lines);
                auto next = lines[line].startByte;
                changed = next != m_cursorByte;
                m_cursorByte = next;
            }
        } else if (key == cocos2d::KEY_End) {
            auto lines = wrapText(m_value);
            if (!lines.empty()) {
                auto line = cursorLineIndex(lines);
                auto next = lines[line].endByte;
                changed = next != m_cursorByte;
                m_cursorByte = next;
            }
        } else if (key == cocos2d::KEY_Up) {
            moveCursorVertical(-1);
            return;
        } else if (key == cocos2d::KEY_Down) {
            moveCursorVertical(1);
            return;
        }

        if (changed) renderText();
    }


    void focusInput() {
        if (!m_input) return;
        m_focused = true;
        m_input->focus();

        // The visible multiline cursor is authoritative. The first focus starts at the end
        // of a saved draft; later clicks/arrows keep their exact custom position.
        if (!m_cursorInitialized) {
            m_cursorByte = m_value.size();
            m_cursorInitialized = true;
        }
        refreshVisuals();
    }

    bool initFor(RequestContext const& context) {
        m_context = context;
        m_value = feedbackFor(context);
        truncateUtf8ToChars(m_value, FEEDBACK_LIMIT);
        m_cursorByte = m_value.size();
        if (!Popup::init(340.f, 205.f)) return false;
        setTitle("REQUEST FEEDBACK", "goldFont.fnt", .62f, 20.f);

        auto* box = CCLayerColor::create(ccc4(110, 61, 34, 255), FIELD_W, FIELD_H);
        box->setOpacity(165);
        box->setPosition({FIELD_X, FIELD_Y});
        m_mainLayer->addChild(box, 1);

        // Fixed-position BMFont rows are used for the visible multiline editor. The native
        // TextInput stays off-screen and owns only IME / editing / the real insertion index.
        m_measureLabel = CCLabelBMFont::create("", "chatFont.fnt");
        if (!m_measureLabel) return false;
        m_measureLabel->setVisible(false);
        m_mainLayer->addChild(m_measureLabel, 0);

        m_input = geode::TextInput::create(FIELD_W, "", "chatFont.fnt");
        if (!m_input) return false;
        m_input->setPosition({-1000.f, -1000.f});
        m_input->hideBG();
        m_input->setTextAlign(geode::TextInputAlign::Left);
        m_input->setCommonFilter(geode::CommonFilter::Any);
        m_input->setMaxCharCount(FEEDBACK_LIMIT);
        m_input->setString(gd::string(m_value.c_str()), false);
        m_transportValue = m_value;
        m_input->setCallback([this](std::string const& value) {
            this->consumeTransportMutation(value);
        });
        m_mainLayer->addChild(m_input, 0);

        m_placeholder = CCLabelBMFont::create("WRITE FEEDBACK...", "chatFont.fnt");
        m_placeholder->setScale(.48f);
        m_placeholder->setOpacity(145);
        m_placeholder->setAnchorPoint({0.f, .5f});
        m_placeholder->setPosition({TEXT_LEFT, TEXT_TOP - 4.f});
        m_mainLayer->addChild(m_placeholder, 6);

        m_caret = CCLayerColor::create(ccc4(255, 255, 255, 255), .46f, 9.2f);
        if (m_caret) {
            m_caret->setVisible(false);
            m_caret->runAction(CCRepeatForever::create(CCBlink::create(.9f, 1)));
            m_mainLayer->addChild(m_caret, 7);
        }

        m_touchLayer = FeedbackTouchLayer::create(
            CCSize(FIELD_W, FIELD_H),
            [this](CCPoint const& local) { this->placeCursorFromFieldPoint(local); },
            [this](cocos2d::enumKeyCodes key) { this->handleEditorKey(key); }
        );
        if (!m_touchLayer) return false;
        m_touchLayer->setPosition({FIELD_X, FIELD_Y});
        m_mainLayer->addChild(m_touchLayer, 20);

        m_counter = CCLabelBMFont::create("0/1500", "goldFont.fnt");
        m_counter->setScale(.27f);
        m_counter->setAnchorPoint({1.f, .5f});
        m_counter->setPosition({300.f, 58.f});
        m_mainLayer->addChild(m_counter);

        auto* cancelSpr = ButtonSprite::create("CANCEL", 80, true, "bigFont.fnt", "GJ_button_04.png", 30.f, .58f);
        auto* cancelBtn = CCMenuItemSpriteExtra::create(cancelSpr, this, menu_selector(FeedbackPopup::onCancel));
        cancelBtn->setPosition({118.f, 25.f});
        m_buttonMenu->addChild(cancelBtn);

        auto* saveSpr = ButtonSprite::create("SAVE", 80, true, "bigFont.fnt", "GJ_button_01.png", 30.f, .58f);
        auto* saveBtn = CCMenuItemSpriteExtra::create(saveSpr, this, menu_selector(FeedbackPopup::onSave));
        saveBtn->setPosition({222.f, 25.f});
        m_buttonMenu->addChild(saveBtn);

        // Text/IME events come from the native TextInput, while clicks and navigation use the
        // wrapped editor's authoritative cursor. The callback translates native mutations
        // into edits at that cursor, so the two cursor models can no longer diverge.
        refreshVisuals();
        return true;
    }

    void dismissEditor(CCObject* sender = nullptr) {
        if (m_input) m_input->defocus();
        m_focused = false;
        Popup::onClose(sender);
    }

    void onClose(CCObject* sender) override {
        dismissEditor(sender);
    }

    void onCancel(CCObject*) { dismissEditor(nullptr); }

    void onSave(CCObject*) {
        if (!m_context.active || m_context.request.requestID <= 0) {
            dismissEditor(nullptr);
            return;
        }
        syncValueFromInput();
        g_feedbackDrafts[m_context.request.requestID] = m_value;
        dismissEditor(nullptr);
    }

public:
    static FeedbackPopup* create(RequestContext const& context) {
        auto* ret = new FeedbackPopup();
        if (ret && ret->initFor(context)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

static void openFeedbackEditor(RequestContext const& context) {
    if (!context.active || context.request.requestID <= 0) return;
    if (auto* popup = FeedbackPopup::create(context)) popup->show();
}

static bool replaceFirstLabelContaining(CCNode* root, std::string const& needle, std::string const& replacement) {
    if (!root) return false;
    if (auto* label = typeinfo_cast<CCLabelBMFont*>(root)) {
        auto text = std::string(label->getString());
        auto haystack = text;
        auto search = needle;
        std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        std::transform(search.begin(), search.end(), search.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (haystack.find(search) != std::string::npos) {
            // Repeated late refreshes must not keep shrinking an already-correct helper title.
            if (text == replacement) return true;
            label->setString(replacement.c_str());
            // HELPER is wider than MOD; keep the vanilla title centered and inside the popup.
            label->setScale(label->getScale() * .84f);
            return true;
        }
    }

    for (CCNode* child : root->getChildrenExt()) {
        if (replaceFirstLabelContaining(child, needle, replacement)) return true;
    }
    return false;
}

static CCMenuItemSpriteExtra* findBottomRowButtonLeftOf(CCNode* parent, CCMenuItemSpriteExtra* submit) {
    if (!parent || !submit) return nullptr;
    CCMenuItemSpriteExtra* best = nullptr;
    for (CCNode* child : parent->getChildrenExt()) {
        auto* button = typeinfo_cast<CCMenuItemSpriteExtra*>(child);
        if (!button || button == submit || !button->isVisible()) continue;
        if (std::abs(button->getPositionY() - submit->getPositionY()) > 8.f) continue;
        if (button->getPositionX() >= submit->getPositionX()) continue;
        if (!best || button->getPositionX() > best->getPositionX()) best = button;
    }
    return best;
}

static std::vector<std::string> const DIFFICULTIES = {
    "all", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "demon-easy", "demon-medium", "demon-hard", "demon-insane", "demon-extreme"
};
static std::vector<std::string> const LEVEL_TYPES = {"all", "classic", "platformer"};
static std::vector<std::string> const STATUSES = {"unchecked", "sent", "rejected", "all"};
static std::vector<std::string> const MIN_SENDS = {"any", "star_rate", "featured", "epic", "legendary", "mythic"};
static std::vector<std::string> const RATED = {"all", "unrated", "rated"};
static std::vector<std::string> const SORTS = {"newest", "oldest", "random"};

static std::string prettyDifficulty(std::string const& v) {
    if (v == "all") return "Any";
    if (v == "1") return "1 star";
    if (v == "demon-easy") return "Easy Demon";
    if (v == "demon-medium") return "Medium Demon";
    if (v == "demon-hard") return "Hard Demon";
    if (v == "demon-insane") return "Insane Demon";
    if (v == "demon-extreme") return "Extreme Demon";
    return v + " stars";
}

static std::string prettyDifficultySelection(std::vector<std::string> const& values) {
    if (values.empty()) return "Any";
    if (values.size() == 1) return prettyDifficulty(values.front());
    if (values.size() == 2) return prettyDifficulty(values[0]) + ", " + prettyDifficulty(values[1]);
    return std::to_string(values.size()) + " selected";
}

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

static bool containsDifficulty(std::vector<std::string> const& values, std::string const& key) {
    return std::find(values.begin(), values.end(), key) != values.end();
}

static char const* difficultyFrameFor(std::string const& key) {
    // Use the same *_btn frames Geometry Dash uses in its search UI. These are present in
    // the base game and are texture-pack friendly. The face-only 07..10 names are NOT safe
    // to assume: on some packs/loaders they resolve to the magenta missing-texture atlas.
    if (key == "all") return "difficulty_00_btn_001.png";
    if (key == "1") return "difficulty_auto_btn_001.png";
    if (key == "2") return "difficulty_01_btn_001.png";
    if (key == "3") return "difficulty_02_btn_001.png";
    if (key == "4" || key == "5") return "difficulty_03_btn_001.png";
    if (key == "6" || key == "7") return "difficulty_04_btn_001.png";
    if (key == "8" || key == "9") return "difficulty_05_btn_001.png";
    if (key == "demon-easy") return "difficulty_07_btn_001.png";
    if (key == "demon-medium") return "difficulty_08_btn_001.png";
    if (key == "demon-hard") return "difficulty_06_btn_001.png";
    if (key == "demon-insane") return "difficulty_09_btn_001.png";
    if (key == "demon-extreme") return "difficulty_10_btn_001.png";
    return "difficulty_00_btn_001.png";
}

static std::string difficultyDisplayName(std::string const& key) {
    if (key == "all") return "NA";
    if (key == "1") return "AUTO";
    if (key == "2") return "EASY";
    if (key == "3") return "NORMAL";
    if (key == "4" || key == "5") return "HARD";
    if (key == "6" || key == "7") return "HARDER";
    if (key == "8" || key == "9") return "INSANE";
    if (key == "demon-easy") return "EASY DEMON";
    if (key == "demon-medium") return "MEDIUM DEMON";
    if (key == "demon-hard") return "HARD DEMON";
    if (key == "demon-insane") return "INSANE DEMON";
    if (key == "demon-extreme") return "EXTREME DEMON";
    return upperCopy(key);
}

static int difficultyStarCount(std::string const& key) {
    if (key.size() == 1 && key[0] >= '1' && key[0] <= '9') return key[0] - '0';
    if (key.rfind("demon-", 0) == 0) return 10;
    return 0;
}

static bool isDemonDifficulty(std::string const& key) {
    return key.rfind("demon-", 0) == 0;
}

static CCSprite* makeDifficultyTile(std::string const& key, bool selected) {
    auto* tile = CCSprite::create();
    tile->setContentSize({72.f, 61.f});
    tile->setAnchorPoint({.5f, .5f});

    auto* cache = CCSpriteFrameCache::sharedSpriteFrameCache();
    auto* frame = cache ? cache->spriteFrameByName(difficultyFrameFor(key)) : nullptr;
    auto* face = frame ? CCSprite::createWithSpriteFrame(frame) : nullptr;
    if (face) {
        // Keep stock *_btn assets only. Demons sit a touch higher and slightly smaller so their
        // custom caption can cover the baked-in DEMON text without eating the jaw / horns.
        face->setScale(isDemonDifficulty(key) ? .67f : .80f);
        face->setPosition({36.f, isDemonDifficulty(key) ? 45.8f : 43.8f});
        face->setOpacity(selected ? 255 : 240);
        tile->addChild(face, 2);
    }

    if (isDemonDifficulty(key) || key == "all") {
        // Stock demon / NA *_btn frames include their own caption. Hide that strip and draw our
        // own text so every tile shares the same baseline.
        auto* captionMask = CCLayerColor::create(ccc4(72, 40, 28, 255), 72.f, key == "all" ? 8.8f : 10.6f);
        if (captionMask) {
            captionMask->setPosition({0.f, key == "all" ? 24.8f : 24.2f});
            tile->addChild(captionMask, 3);
        }
    }

    if (isDemonDifficulty(key)) {
        auto name = difficultyDisplayName(key);
        auto* nameLabel = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
        if (nameLabel) {
            auto scale = name.size() >= 13 ? .198f : .214f;
            nameLabel->setScale(scale);
            auto width = nameLabel->getContentSize().width * scale;
            if (width > 69.f && width > 0.f) nameLabel->setScale(scale * 69.f / width);
            nameLabel->setPosition({36.f, 31.0f});
            tile->addChild(nameLabel, 4);
        }
    }

    auto stars = difficultyStarCount(key);
    if (stars > 0) {
        auto starText = std::to_string(stars);
        auto* starsLabel = CCLabelBMFont::create(starText.c_str(), "bigFont.fnt");
        if (starsLabel) {
            starsLabel->setScale(.40f);
            starsLabel->setAnchorPoint({1.f, .5f});
            starsLabel->setPosition({31.4f, 22.6f});
            tile->addChild(starsLabel, 4);
        }

        auto* star = CCSprite::createWithSpriteFrameName("GJ_starsIcon_001.png");
        if (star) {
            star->setScale(.54f);
            star->setPosition({42.8f, 22.3f});
            tile->addChild(star, 4);
        }
    } else if (key == "all") {
        auto* anyLabel = CCLabelBMFont::create("ANY", "bigFont.fnt");
        if (anyLabel) {
            anyLabel->setScale(.30f);
            anyLabel->setPosition({36.f, 21.8f});
            tile->addChild(anyLabel, 4);
        }
    }

    if (selected) {
        auto* check = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
        if (check) {
            check->setScale(.30f);
            check->setPosition({63.f, 55.f});
            tile->addChild(check, 5);
        }
    }
    return tile;
}

class DifficultyPickerPopup final : public geode::Popup {
protected:
    std::vector<std::string> m_selected;
    std::function<void(std::vector<std::string> const&)> m_onApply;
    std::vector<std::pair<std::string, CCMenuItemSpriteExtra*>> m_tiles;
    CCLabelBMFont* m_summary = nullptr;

    bool selected(std::string const& key) const {
        return key == "all" ? m_selected.empty() : containsDifficulty(m_selected, key);
    }

    void refreshTiles() {
        for (auto& [key, button] : m_tiles) {
            if (!button) continue;
            button->setSprite(makeDifficultyTile(key, selected(key)));
            button->setSizeMult(1.f);
        }
        if (m_summary) {
            auto summary = m_selected.empty()
                ? std::string("ANY DIFFICULTY")
                : std::to_string(m_selected.size()) + (m_selected.size() == 1 ? " DIFFICULTY" : " DIFFICULTIES");
            m_summary->setString(summary.c_str());
        }
    }

    bool initFor(
        std::vector<std::string> const& selectedValues,
        std::function<void(std::vector<std::string> const&)> onApply
    ) {
        m_selected = selectedValues;
        m_onApply = std::move(onApply);
        if (!Popup::init(430.f, 292.f)) return false;
        setTitle("REQUESTED DIFFICULTIES", "goldFont.fnt", .58f, 20.f);

        // One flat, slightly darker field instead of 15 textured scale9 tiles. This keeps
        // the layout readable even when the player uses a texture pack that replaces
        // square02b_001.png with a strongly patterned sprite.
        auto* panel = CCLayerColor::create(ccc4(72, 40, 28, 255), 402.f, 190.f);
        if (panel) {
            panel->setPosition({14.f, 61.f});
            m_mainLayer->addChild(panel, 0);
        }

        constexpr float xs[] = {55.f, 135.f, 215.f, 295.f, 375.f};
        constexpr float ys[] = {216.f, 156.f, 96.f};
        for (std::size_t i = 0; i < DIFFICULTIES.size(); ++i) {
            auto const& key = DIFFICULTIES[i];
            auto* btn = CCMenuItemSpriteExtra::create(
                makeDifficultyTile(key, selected(key)),
                this,
                menu_selector(DifficultyPickerPopup::onToggle)
            );
            btn->setUserObject(CCString::create(key.c_str()));
            btn->setPosition({xs[i % 5], ys[i / 5]});
            btn->setSizeMult(1.f);
            m_buttonMenu->addChild(btn);
            m_tiles.emplace_back(key, btn);
        }

        m_summary = CCLabelBMFont::create("ANY DIFFICULTY", "goldFont.fnt");
        if (m_summary) {
            m_summary->setScale(.28f);
            m_summary->setPosition({110.f, 30.f});
            m_mainLayer->addChild(m_summary);
        }

        auto* applySpr = ButtonSprite::create("APPLY", 78, true, "bigFont.fnt", "GJ_button_01.png", 30.f, .55f);
        auto* applyBtn = CCMenuItemSpriteExtra::create(applySpr, this, menu_selector(DifficultyPickerPopup::onApply));
        applyBtn->setPosition({320.f, 30.f});
        m_buttonMenu->addChild(applyBtn);

        refreshTiles();
        return true;
    }

    void onToggle(CCObject* sender) {
        auto* node = typeinfo_cast<CCNode*>(sender);
        if (!node) return;
        auto* value = typeinfo_cast<CCString*>(node->getUserObject());
        if (!value) return;
        auto key = std::string(value->getCString());

        if (key == "all") {
            m_selected.clear();
        } else {
            auto it = std::find(m_selected.begin(), m_selected.end(), key);
            if (it == m_selected.end()) m_selected.push_back(key);
            else m_selected.erase(it);
        }
        refreshTiles();
    }

    void onApply(CCObject*) {
        if (m_onApply) m_onApply(m_selected);
        onClose(nullptr);
    }

public:
    static DifficultyPickerPopup* create(
        std::vector<std::string> const& selected,
        std::function<void(std::vector<std::string> const&)> onApply
    ) {
        auto* ret = new DifficultyPickerPopup();
        if (ret && ret->initFor(selected, std::move(onApply))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class RequestFiltersPopup final : public geode::Popup {
protected:
    RequestFilters m_working;
    CCMenuItemSpriteExtra* m_difficultyButton = nullptr;
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

    void normalizeDependentFilters() {
        // A request cannot be both "Not checked"/"Rejected" by me and simultaneously
        // have one of my sent tiers. Keep that impossible combination out of the UI.
        if (m_working.status == "unchecked" || m_working.status == "rejected") {
            m_working.minSend = "any";
        }
    }

    void refresh() {
        normalizeDependentFilters();
        if (m_difficultyButton) {
            auto difficultyText = prettyDifficultySelection(m_working.difficulties);
            auto* spr = ButtonSprite::create(
                difficultyText.c_str(), 170, true, "bigFont.fnt", "GJ_button_04.png", 28.f,
                difficultyText.size() > 10 ? .40f : .48f
            );
            m_difficultyButton->setSprite(spr);
            m_difficultyButton->setSizeMult(1.f);
        }
        if (m_type) m_type->setString(prettyType(m_working.levelType).c_str());
        if (m_status) m_status->setString(prettyStatus(m_working.status).c_str());
        if (m_minSend) m_minSend->setString(prettyMinSend(m_working.minSend).c_str());
        if (m_rated) m_rated->setString(prettyRated(m_working.rated).c_str());
        if (m_sort) m_sort->setString(prettySort(m_working.sort).c_str());
    }

    bool initFor() {
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

        addText("Difficulty", {72.f, top}, .35f, "goldFont.fnt");
        auto* difficultySpr = ButtonSprite::create("Any", 170, true, "bigFont.fnt", "GJ_button_04.png", 28.f, .48f);
        m_difficultyButton = CCMenuItemSpriteExtra::create(
            difficultySpr, this, menu_selector(RequestFiltersPopup::onDifficultyPicker)
        );
        m_difficultyButton->setPosition({215.f, top});
        m_difficultyButton->setSizeMult(1.f);
        m_buttonMenu->addChild(m_difficultyButton);

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

    void onDifficultyPicker(CCObject*) {
        if (auto* popup = DifficultyPickerPopup::create(
            m_working.difficulties,
            [this](std::vector<std::string> const& values) {
                this->m_working.difficulties = values;
                this->refresh();
            }
        )) popup->show();
    }
    void typePrev(CCObject*) { cycleValue(m_working.levelType, LEVEL_TYPES, -1); refresh(); }
    void typeNext(CCObject*) { cycleValue(m_working.levelType, LEVEL_TYPES, 1); refresh(); }
    void statusPrev(CCObject*) { cycleValue(m_working.status, STATUSES, -1); refresh(); }
    void statusNext(CCObject*) { cycleValue(m_working.status, STATUSES, 1); refresh(); }
    void sendPrev(CCObject*) {
        if (m_working.status == "unchecked" || m_working.status == "rejected") { m_working.minSend = "any"; refresh(); return; }
        cycleValue(m_working.minSend, MIN_SENDS, -1); refresh();
    }
    void sendNext(CCObject*) {
        if (m_working.status == "unchecked" || m_working.status == "rejected") { m_working.minSend = "any"; refresh(); return; }
        cycleValue(m_working.minSend, MIN_SENDS, 1); refresh();
    }
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
        showAlert(MOD_NAME, "Filters saved. Tap Refresh in Server Requests to apply them.");
    }

public:
    static RequestFiltersPopup* create() {
        auto* ret = new RequestFiltersPopup();
        if (ret && ret->initFor()) {
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
    body["noPing"] = noPingFor(context);

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
            g_noPingDrafts.erase(requestID);
            showAlert(MOD_NAME, action == "send" ? "Request send result submitted." : "Request rejection submitted.");
        } else {
            showAlert(MOD_NAME, "Could not submit the request result.\n\nHTTP " + std::to_string(res.code()) + "\n" +
                (text.empty() ? "Empty response" : text));
        }
    });
}

class RejectPopup final : public geode::Popup {
protected:
    struct ReasonChoice {
        std::string label;
        std::string reason;
        CCMenuItemSpriteExtra* button = nullptr;
    };

    RequestContext m_context;
    std::string m_reason;
    std::vector<ReasonChoice> m_reasonButtons;
    CCMenuItemSpriteExtra* m_submitButton = nullptr;
    CCMenuItemToggler* m_noPingToggle = nullptr;
    CCLabelBMFont* m_noPingLabel = nullptr;

    static ButtonSprite* makeReasonSprite(std::string const& label, bool selected) {
        return ButtonSprite::create(
            label.c_str(),
            112,
            true,
            "bigFont.fnt",
            selected ? "GJ_button_01.png" : "GJ_button_04.png",
            28.f,
            .52f
        );
    }

    static ButtonSprite* makeBottomSprite(char const* label) {
        // Match the RateStars-style action buttons: green GJ background + gold font,
        // with a fixed action-button footprint. Disabled appearance is handled by the
        // menu item itself, just like the vanilla Submit button.
        return ButtonSprite::create(
            label,
            104,
            true,
            "goldFont.fnt",
            "GJ_button_01.png",
            32.f,
            .84f
        );
    }

    void updateButtons() {
        for (auto& choice : m_reasonButtons) {
            if (!choice.button) continue;
            if (auto* sprite = makeReasonSprite(choice.label, choice.reason == m_reason)) {
                choice.button->setSprite(sprite);
                choice.button->setSizeMult(1.f);
            }
        }

        if (m_submitButton) {
            bool enabled = !m_reason.empty();
            if (auto* sprite = makeBottomSprite("SUBMIT")) {
                m_submitButton->setSprite(sprite);
            }
            m_submitButton->setEnabled(enabled);
            m_submitButton->setSizeMult(1.f);
        }

        if (m_noPingToggle) m_noPingToggle->toggle(noPingFor(m_context));
    }

    CCMenuItemSpriteExtra* reasonButton(char const* label, std::string reason, CCPoint pos) {
        auto* sprite = makeReasonSprite(label, false);
        if (!sprite) return nullptr;
        auto* btn = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(RejectPopup::onReason));
        if (!btn) return nullptr;
        btn->setPosition(pos);
        btn->setSizeMult(1.f);
        btn->setUserObject(CCString::create(reason.c_str()));
        m_buttonMenu->addChild(btn);
        m_reasonButtons.push_back({label, reason, btn});
        return btn;
    }

    void addNoPingControl(CCPoint position) {
        m_noPingToggle = CCMenuItemToggler::createWithStandardSprites(
            this,
            menu_selector(RejectPopup::onNoPing),
            .48f
        );
        if (!m_noPingToggle) return;
        m_noPingToggle->setPosition(position);
        m_noPingToggle->setSizeMult(1.f);
        m_noPingToggle->toggle(noPingFor(m_context));
        m_buttonMenu->addChild(m_noPingToggle);

        m_noPingLabel = CCLabelBMFont::create("NO PING", "goldFont.fnt");
        m_noPingLabel->setScale(.20f);
        m_noPingLabel->setAnchorPoint({.5f, .5f});
        m_noPingLabel->setPosition({position.x, position.y - 13.f});
        m_buttonMenu->addChild(m_noPingLabel);
    }

    bool initFor(RequestContext const& context) {
        m_context = context;
        if (!Popup::init(400.f, 190.f)) return false;

        char const* title = context.mode == "helper" ? "HELPER: REJECT REASON" : "MOD: REJECT REASON";
        setTitle(title, "bigFont.fnt", context.mode == "helper" ? .72f : .80f, 20.f);

        reasonButton("NOT SENT", "not_sent", {112.f, 118.f});
        reasonButton("ALREADY SEEN", "already_seen", {288.f, 118.f});
        reasonButton("ALREADY RATED", "already_rated", {112.f, 78.f});
        reasonButton("REPORT", "report", {288.f, 78.f});

        auto* cancelSpr = makeBottomSprite("CANCEL");
        auto* cancelBtn = CCMenuItemSpriteExtra::create(cancelSpr, this, menu_selector(RejectPopup::onCancel));
        cancelBtn->setPosition({138.f, 30.f});
        cancelBtn->setSizeMult(1.f);
        m_buttonMenu->addChild(cancelBtn);

        auto* submitSpr = makeBottomSprite("SUBMIT");
        m_submitButton = CCMenuItemSpriteExtra::create(submitSpr, this, menu_selector(RejectPopup::onSubmit));
        m_submitButton->setPosition({262.f, 30.f});
        m_submitButton->setSizeMult(1.f);
        m_buttonMenu->addChild(m_submitButton);

        auto* feedbackSpr = CCSprite::createWithSpriteFrameName("GJ_editBtn_001.png");
        feedbackSpr->setScale(.32f);
        auto* feedbackBtn = CCMenuItemSpriteExtra::create(feedbackSpr, this, menu_selector(RejectPopup::onFeedback));
        feedbackBtn->setID("kolorbok.gd-send-logger/request-feedback-button");
        feedbackBtn->setSizeMult(1.f);
        feedbackBtn->setPosition({cancelBtn->getPositionX() - 78.f, cancelBtn->getPositionY()});
        m_buttonMenu->addChild(feedbackBtn);

        // Mirror the feedback control on the right side with GD's standard checkbox.
        addNoPingControl({m_submitButton->getPositionX() + 78.f, m_submitButton->getPositionY()});

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

    void syncNoPingState(float) {
        // CCMenuItemToggler changes its own state as part of activate(). Read that final
        // state on the next scheduler tick instead of toggling it a second time here.
        if (!m_noPingToggle) return;
        setNoPingFor(m_context, m_noPingToggle->isToggled());
    }

    void onNoPing(CCObject*) {
        this->scheduleOnce(schedule_selector(RejectPopup::syncNoPingState), 0.f);
    }

    void onFeedback(CCObject*) { openFeedbackEditor(m_context); }
    void onCancel(CCObject*) { onClose(nullptr); }
    void onSubmit(CCObject*) {
        if (m_reason.empty()) return;
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

class RequestsHubPopup final : public geode::Popup {
protected:
    CCLabelBMFont* m_statusLabel = nullptr;
    CCLabelBMFont* m_metaLabel = nullptr;
    CCMenuItemSpriteExtra* m_openButton = nullptr;
    CCMenuItemSpriteExtra* m_filterButton = nullptr;
    CCMenuItemSpriteExtra* m_refreshButton = nullptr;
    bool m_loading = false;

    static std::string modeLabel() {
        if (g_client.mode == "moderator") return "MODERATOR";
        if (g_client.mode == "helper") return "HELPER";
        if (g_client.mode == "reviewer") return "REVIEWER";
        return "ALL";
    }

    static std::string shortID(std::string const& value) {
        if (value.size() <= 12) return value;
        return value.substr(0, 6) + "..." + value.substr(value.size() - 4);
    }

    void setStatus(std::string const& text) {
        if (m_statusLabel) m_statusLabel->setString(text.c_str());
    }

    void refreshButtons() {
        bool ready = !m_loading && !g_requestList.empty();
        if (m_openButton) m_openButton->setVisible(ready);
        if (m_filterButton) m_filterButton->setVisible(!m_loading);
    }

    void applyLoadedState() {
        auto foundCount = static_cast<int>(requestLevelIDs().size());
        bool definitelyCapped = g_client.total > g_client.returned && g_client.returned > 0;
        bool likelyHundredCap = g_client.returned >= 100 && g_client.total <= g_client.returned;

        auto meta = "CONNECTED  |  " + modeLabel() + "  |  " + std::to_string(foundCount) + " SHOWN";
        setStatus(meta);
        if (m_metaLabel) {
            std::string info;
            if (definitelyCapped) {
                info = "SERVER LIMIT: " + std::to_string(g_client.returned) + " / " + std::to_string(g_client.total) + " MATCHES RETURNED";
            } else if (likelyHundredCap) {
                info = "ONLY 100 LOADED - SERVER MAY BE CAPPING THE RESPONSE";
            } else {
                info = "SERVER " + shortID(g_client.serverID) + "  -  USER " + shortID(g_client.userID);
            }
            m_metaLabel->setString(info.c_str());
        }
        refreshButtons();

        if (g_requestList.empty()) {
            if (g_filters.status == "unchecked") {
                setStatus("0 shown for NOT CHECKED - server filter returned no rows");
            } else {
                setStatus("No requests match these filters");
            }
        }
    }

    void loadRequests() {
        if (m_loading) return;
        auto key = connectionKey();
        if (key.empty()) {
            setStatus("Connection Key is empty - use /geode-link in Discord");
            return;
        }

        m_loading = true;
        g_client = ClientState{};
        g_context = RequestContext{};
        g_requestBrowserActive = false;
        g_requestBrowser = nullptr;
        g_requestList.clear();
        g_requestByLevel.clear();
        g_hasSelectedRequest = false;
        g_selectedRequest = RequestMeta{};
        setStatus("Loading server requests...");
        if (m_metaLabel) m_metaLabel->setString("Connecting to Discord bot...");
        refreshButtons();

        auto req = web::WebRequest();
        req.header("Authorization", "Bearer " + key);
        req.timeout(std::chrono::seconds(30));
        auto url = requestURL();

        this->retain();
        async::spawn(req.get(url), [self = this](web::WebResponse res) {
            auto text = res.string().unwrapOr("");
            self->m_loading = false;

            if (!self->getParent()) {
                self->release();
                return;
            }

            if (!res.ok()) {
                self->setStatus("Request server error - HTTP " + std::to_string(res.code()));
                if (self->m_metaLabel) {
                    self->m_metaLabel->setString(
                        limitPopupText(text.empty() ? "No response body" : text, 120).c_str()
                    );
                }
                self->refreshButtons();
                self->release();
                return;
            }

            if (!parseRequestsResponse(text)) {
                self->setStatus("Invalid response from request server");
                if (self->m_metaLabel) self->m_metaLabel->setString(limitPopupText(text, 120).c_str());
                self->refreshButtons();
                self->release();
                return;
            }

            self->applyLoadedState();
            self->release();
        });
    }

    bool init() {
        if (!Popup::init(440.f, 205.f)) return false;
        setTitle("SERVER REQUESTS");

        m_statusLabel = CCLabelBMFont::create("Loading server requests...", "bigFont.fnt");
        m_statusLabel->setScale(.42f);
        m_statusLabel->setPosition({220.f, 154.f});
        m_mainLayer->addChild(m_statusLabel);

        m_metaLabel = CCLabelBMFont::create("", "goldFont.fnt");
        m_metaLabel->setScale(.28f);
        m_metaLabel->setPosition({220.f, 134.f});
        m_mainLayer->addChild(m_metaLabel);


        auto* openSprite = ButtonSprite::create("OPEN LEVELS", 190, true, "bigFont.fnt", "GJ_button_01.png", 34.f, .64f);
        m_openButton = CCMenuItemSpriteExtra::create(openSprite, this, menu_selector(RequestsHubPopup::onOpenLevels));
        m_openButton->setPosition({220.f, 92.f});
        m_buttonMenu->addChild(m_openButton);
        m_openButton->setVisible(false);


        auto* filterSprite = ButtonSprite::create("FILTERS", 92, true, "bigFont.fnt", "GJ_button_04.png", 28.f, .54f);
        m_filterButton = CCMenuItemSpriteExtra::create(filterSprite, this, menu_selector(RequestsHubPopup::onFilters));
        m_filterButton->setPosition({145.f, 32.f});
        m_buttonMenu->addChild(m_filterButton);
        m_filterButton->setVisible(false);

        auto* refreshSprite = ButtonSprite::create("REFRESH", 92, true, "bigFont.fnt", "GJ_button_01.png", 28.f, .54f);
        m_refreshButton = CCMenuItemSpriteExtra::create(refreshSprite, this, menu_selector(RequestsHubPopup::onRefresh));
        m_refreshButton->setPosition({295.f, 32.f});
        m_buttonMenu->addChild(m_refreshButton);

        this->retain();
        geode::queueInMainThread([self = this]() {
            if (self->getParent()) self->loadRequests();
            self->release();
        });
        return true;
    }

    void onOpenLevels(CCObject*) {
        if (m_loading) return;
        if (requestLevelIDs().empty()) {
            showRequestError("There are no request levels to open.");
            return;
        }

        // Geometry Dash's comma-separated native search effectively tops out around one
        // 100-ID query. Keep native LevelCell rendering, but page the full request set in
        // 100-ID batches and bridge the native arrows between batches below.
        g_requestNativeBatch = 0;
        auto* search = makeRequestNativeBatchSearch(g_requestNativeBatch);
        if (!search) {
            showRequestError("Could not create the Geometry Dash request level list.");
            return;
        }

        g_hasSelectedRequest = false;
        g_selectedRequest = RequestMeta{};
        g_nextBrowserIsRequests = true;

        auto* scene = LevelBrowserLayer::scene(search);
        if (!scene) {
            g_nextBrowserIsRequests = false;
            showRequestError("Geometry Dash could not create the level browser scene.");
            return;
        }

        CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(.25f, scene));
    }

    void onFilters(CCObject*) {
        if (auto* popup = RequestFiltersPopup::create()) popup->show();
    }

    void onRefresh(CCObject*) { loadRequests(); }

public:
    static RequestsHubPopup* create() {
        auto* ret = new RequestsHubPopup();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};


class $modify(GDRequestsLevelSearchLayer, LevelSearchLayer) {
    void onRequests(CCObject*) {
        log::info("[REQUESTS UI] Requests button pressed");
        if (auto* popup = RequestsHubPopup::create()) {
            popup->show();
        } else {
            showRequestError("Could not create the Server Requests window.");
        }
    }

    void installRequestsButton() {
        NodeIDs::provideFor(this);
        auto* menu = typeinfo_cast<CCMenu*>(this->getChildByID("other-filter-menu"));
        if (!menu) menu = this->getChildByType<CCMenu>(0);
        if (!menu) {
            log::error("[REQUESTS UI] other-filter-menu was not found");
            return;
        }
        if (menu->getChildByID("kolorbok.gd-send-logger/requests-button")) return;

        auto* sprite = ButtonSprite::create(
            "REQ", 48, true, "bigFont.fnt", "GJ_button_01.png", 28.f, .58f
        );
        if (!sprite) return;
        auto* button = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(GDRequestsLevelSearchLayer::onRequests)
        );
        if (!button) return;
        button->setID("kolorbok.gd-send-logger/requests-button");
        menu->addChild(button);
        if (menu->getLayout()) menu->updateLayout();
        else button->setPosition({menu->getContentSize().width / 2.f, menu->getContentSize().height / 2.f});
    }

    bool init(int type) {
        if (!LevelSearchLayer::init(type)) return false;
        installRequestsButton();
        this->retain();
        geode::queueInMainThread([self = this]() {
            self->installRequestsButton();
            self->release();
        });
        return true;
    }
};

class $modify(GDRequestsLevelBrowserLayer, LevelBrowserLayer) {
    struct Fields {
        bool requestBrowser = false;
        bool nativeAtEnd = false;
        bool nativeAtStart = true;
    };

    bool isThisRequestBrowser() {
        return m_fields->requestBrowser && g_requestBrowserActive && g_requestBrowser == this;
    }

    void refreshRequestBatchArrows() {
        if (!isThisRequestBrowser()) return;

        // Capture vanilla page-boundary state before exposing an arrow for the adjacent
        // request batch. This lets normal GD pagination continue inside each 100-ID batch.
        m_fields->nativeAtEnd = !m_rightArrow || !m_rightArrow->isVisible();
        m_fields->nativeAtStart = !m_leftArrow || !m_leftArrow->isVisible();

        if (m_fields->nativeAtEnd && hasNextRequestNativeBatch() && m_rightArrow) {
            m_rightArrow->setVisible(true);
        }
        if (m_fields->nativeAtStart && hasPrevRequestNativeBatch() && m_leftArrow) {
            m_leftArrow->setVisible(true);
        }
    }

    void loadRequestNativeBatch(std::size_t batch) {
        auto count = requestNativeBatchCount();
        if (count == 0 || batch >= count) return;
        auto* search = makeRequestNativeBatchSearch(batch);
        if (!search) return;

        g_requestNativeBatch = batch;
        m_fields->nativeAtEnd = false;
        m_fields->nativeAtStart = true;
        setSearchObject(search);
        loadPage(search);
    }

    bool init(GJSearchObject* searchObj) {
        bool openingRequests = g_nextBrowserIsRequests;
        if (!LevelBrowserLayer::init(searchObj)) return false;
        NodeIDs::provideFor(this);

        if (openingRequests) {
            g_nextBrowserIsRequests = false;
            g_requestBrowserActive = true;
            g_requestBrowser = this;
            m_fields->requestBrowser = true;
            g_requestNativeBatch = 0;
        }
        return true;
    }

    void loadLevelsFinished(CCArray* levels, char const* key, int type) override {
        LevelBrowserLayer::loadLevelsFinished(levels, key, type);
        if (isThisRequestBrowser()) refreshRequestBatchArrows();
    }

    void onNextPage(CCObject* sender) {
        if (isThisRequestBrowser() && m_fields->nativeAtEnd && hasNextRequestNativeBatch()) {
            loadRequestNativeBatch(g_requestNativeBatch + 1);
            return;
        }
        LevelBrowserLayer::onNextPage(sender);
    }

    void onPrevPage(CCObject* sender) {
        if (isThisRequestBrowser() && m_fields->nativeAtStart && hasPrevRequestNativeBatch()) {
            loadRequestNativeBatch(g_requestNativeBatch - 1);
            return;
        }
        LevelBrowserLayer::onPrevPage(sender);
    }

    gd::string getSearchTitle() {
        if (isThisRequestBrowser()) {
            if (g_hasSelectedRequest && g_selectedRequest.requestID > 0) {
                auto title = "Request #" + std::to_string(g_selectedRequest.requestID);
                return gd::string(title.c_str());
            }
            auto title = std::string("Server Requests");
            auto batches = requestNativeBatchCount();
            if (batches > 1) {
                title += " " + std::to_string(g_requestNativeBatch + 1) + "/" + std::to_string(batches);
            }
            if (g_client.total > g_client.returned && g_client.returned > 0) {
                title += " (limited)";
            }
            return gd::string(title.c_str());
        }
        return LevelBrowserLayer::getSearchTitle();
    }

    void onBack(CCObject* sender) override {
        bool wasRequestBrowser = isThisRequestBrowser();
        if (wasRequestBrowser) {
            g_requestBrowserActive = false;
            g_requestBrowser = nullptr;
            g_requestNativeBatch = 0;
            g_hasSelectedRequest = false;
            g_selectedRequest = RequestMeta{};
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
            if (g_hasSelectedRequest && g_selectedRequest.levelID == levelID) {
                m_fields->requestContext.active = true;
                m_fields->requestContext.request = g_selectedRequest;
                m_fields->requestContext.mode = g_client.mode;
                g_context = m_fields->requestContext;
            } else {
                auto it = g_requestByLevel.find(levelID);
                if (it != g_requestByLevel.end()) {
                    m_fields->requestContext.active = true;
                    m_fields->requestContext.request = it->second;
                    m_fields->requestContext.mode = g_client.mode;
                    g_context = m_fields->requestContext;
                }
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
            fakeSprite->setScale(1.f);
            auto* fakeBtn = CCMenuItemSpriteExtra::create(fakeSprite, this, menu_selector(GDRequestsLevelInfoLayer::onHelperSend));
            fakeBtn->setID("kolorbok.gd-send-logger/helper-send-button");
            menu->addChild(fakeBtn);
        }

        auto* rejectSprite = CCSprite::createWithSpriteFrameName("GJ_dislikeBtn_001.png");
        rejectSprite->setScale(1.f);
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
        if (popup) {
            // Do not rely only on the hook-time context: rewrite the freshly-created layer
            // before it is shown as well. This makes the helper title deterministic even if
            // another mod mutates the vanilla MOD title during construction.
            if (!replaceFirstLabelContaining(popup, "MOD: SUGGEST STARS", "HELPER: SUGGEST STARS")) {
                replaceFirstLabelContaining(popup, "SUGGEST STARS", "HELPER: SUGGEST STARS");
            }
            popup->show();
        }
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
        CCMenuItemToggler* noPingToggle = nullptr;
        CCLabelBMFont* noPingLabel = nullptr;
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

    void refreshHelperRequestTitle() {
        if (!m_fields->helperRequestPopup) return;
        if (!replaceFirstLabelContaining(this, "MOD: SUGGEST STARS", "HELPER: SUGGEST STARS")) {
            replaceFirstLabelContaining(this, "SUGGEST STARS", "HELPER: SUGGEST STARS");
        }
    }

    void enforceHelperRequestTitle(float) {
        // Keep enforcing while this helper-only popup exists. The vanilla layer (or another
        // rate-related mod) may rewrite the title after init/show, which made the old two
        // queued refreshes lose the race and leave MOD: visible again.
        refreshHelperRequestTitle();
    }

    bool init(int levelID, bool platformer, bool moderator) {
        bool helperPopup = g_creatingHelperPopup;
        RequestContext captured;
        // g_creatingHelperPopup is the strongest signal: it is set immediately around the
        // helper-created RateStarsLayer::create call. Keep it in the decision instead of
        // calculating it and then accidentally discarding it.
        if (helperPopup && g_context.active && g_context.request.levelID == levelID) {
            captured = g_context;
            captured.mode = "helper";
        } else if (g_context.active && g_context.request.levelID == levelID &&
            (g_context.mode == "helper" || g_context.mode == "moderator")) {
            captured = g_context;
        }

        if (!RateStarsLayer::init(levelID, platformer, moderator)) return false;
        m_fields->helperRequestPopup = helperPopup || (captured.active && captured.mode == "helper");
        if (m_fields->helperRequestPopup && captured.active) captured.mode = "helper";
        m_fields->requestContext = captured;

        if (m_fields->helperRequestPopup) {
            refreshHelperRequestTitle();
            // Do not rely on a finite number of delayed rewrites. Keep the helper title
            // authoritative for the lifetime of this popup; scheduling is automatically
            // stopped when the layer leaves the scene.
            this->schedule(schedule_selector(GDRequestsRateStarsLayer::enforceHelperRequestTitle), .05f);
        }

        if (captured.active) {
            // Anchor both extra controls to the vanilla bottom row: feedback mirrors to the
            // left of Cancel, while the standard GD checkbox mirrors to the right of Submit.
            auto* sprite = CCSprite::createWithSpriteFrameName("GJ_editBtn_001.png");
            sprite->setScale(.34f);
            auto* button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(GDRequestsRateStarsLayer::onRequestFeedback));
            button->setID("kolorbok.gd-send-logger/request-feedback-button");
            button->setSizeMult(1.f);

            auto* noPing = CCMenuItemToggler::createWithStandardSprites(
                this,
                menu_selector(GDRequestsRateStarsLayer::onRequestNoPing),
                .48f
            );
            if (noPing) {
                noPing->setID("kolorbok.gd-send-logger/request-no-ping-toggle");
                noPing->setSizeMult(1.f);
                noPing->toggle(noPingFor(captured));
            }

            auto* noPingLabel = CCLabelBMFont::create("NO PING", "goldFont.fnt");
            if (noPingLabel) {
                noPingLabel->setScale(.20f);
                noPingLabel->setAnchorPoint({.5f, .5f});
            }

            if (m_submitButton && m_submitButton->getParent()) {
                auto* parent = m_submitButton->getParent();
                float feedbackX = m_submitButton->getPositionX() - 150.f;
                if (auto* cancel = findBottomRowButtonLeftOf(parent, m_submitButton)) {
                    feedbackX = cancel->getPositionX() - 78.f;
                }
                float y = m_submitButton->getPositionY();
                button->setPosition({feedbackX, y});
                parent->addChild(button);

                if (noPing) {
                    noPing->setPosition({m_submitButton->getPositionX() + 78.f, y + 4.f});
                    parent->addChild(noPing);
                    if (noPingLabel) {
                        noPingLabel->setPosition({noPing->getPositionX(), y - 10.f});
                        parent->addChild(noPingLabel);
                    }
                }
            } else if (m_buttonMenu) {
                button->setPosition({70.f, 35.f});
                m_buttonMenu->addChild(button);
                if (noPing) {
                    noPing->setPosition({m_buttonMenu->getContentSize().width - 70.f, 39.f});
                    m_buttonMenu->addChild(noPing);
                    if (noPingLabel) {
                        noPingLabel->setPosition({noPing->getPositionX(), 25.f});
                        m_buttonMenu->addChild(noPingLabel);
                    }
                }
            }
            m_fields->feedbackButton = button;
            m_fields->noPingToggle = noPing;
            m_fields->noPingLabel = noPingLabel;
        }
        return true;
    }

    void onRequestFeedback(CCObject*) {
        if (m_fields->requestContext.active) openFeedbackEditor(m_fields->requestContext);
    }

    void syncRequestNoPingState(float) {
        if (!m_fields->requestContext.active || !m_fields->noPingToggle) return;
        setNoPingFor(m_fields->requestContext, m_fields->noPingToggle->isToggled());
    }

    void onRequestNoPing(CCObject*) {
        if (!m_fields->requestContext.active) return;
        this->scheduleOnce(schedule_selector(GDRequestsRateStarsLayer::syncRequestNoPingState), 0.f);
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
