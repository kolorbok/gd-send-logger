#include <Geode/Geode.hpp>
#include <Geode/modify/RateStarsLayer.hpp>
#include <Geode/modify/CCTextInputNode.hpp>
#include <Geode/modify/CCTextFieldTTF.hpp>
#include <Geode/modify/LevelSearchLayer.hpp>
#include <Geode/modify/LevelBrowserLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/LevelCell.hpp>
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
#include <Geode/utils/Keyboard.hpp>
#include <unordered_map>
#include <atomic>
#include <memory>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/binding/LoadingCircleSprite.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <Geode/binding/UploadPopupDelegate.hpp>
#include <Geode/cocos/extensions/GUI/CCControlExtension/CCScale9Sprite.h>

// Cross-platform clipboard helper. Geode 5.6.1 does not provide
// Geode/utils/clipboard.hpp, so use the native Windows clipboard where available.
// Other platforms keep the build portable and show the link instead.
#if defined(GEODE_IS_WINDOWS)
#    include <windows.h>
#endif

static std::string windowsKeyboardText(geode::KeyboardInputData const& data) {
#if defined(GEODE_IS_WINDOWS)
    if (data.action != geode::KeyboardInputData::Action::Press &&
        data.action != geode::KeyboardInputData::Action::Repeat) {
        return {};
    }
    if (data.modifiers & geode::KeyboardModifier::Control ||
        data.modifiers & geode::KeyboardModifier::Alt ||
        data.modifiers & geode::KeyboardModifier::Super) {
        return {};
    }

    auto vkey = static_cast<UINT>(data.native.code);
    auto scancode = static_cast<UINT>(data.native.extra);
    if (vkey == 0 || vkey == VK_BACK || vkey == VK_DELETE || vkey == VK_RETURN ||
        vkey == VK_TAB || vkey == VK_ESCAPE || vkey == VK_LEFT || vkey == VK_RIGHT ||
        vkey == VK_UP || vkey == VK_DOWN || vkey == VK_HOME || vkey == VK_END ||
        vkey == VK_PRIOR || vkey == VK_NEXT || vkey == VK_INSERT) {
        return {};
    }

    BYTE keyboardState[256]{};
    if (!GetKeyboardState(keyboardState)) return {};

    HKL layout = GetKeyboardLayout(0);
    WCHAR utf16[8]{};
    auto result = ToUnicodeEx(vkey, scancode, keyboardState, utf16, 8, 0, layout);
    if (result <= 0) return {};

    int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, utf16, result, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) return {};
    std::string out(static_cast<std::size_t>(bytes), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, utf16, result, out.data(), bytes, nullptr, nullptr) <= 0) {
        return {};
    }
    return out;
#else
    (void)data;
    return {};
#endif
}

static bool copyTextToClipboard(std::string const& text) {
#if defined(GEODE_IS_WINDOWS)
    if (!OpenClipboard(nullptr)) return false;
    EmptyClipboard();

    int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (length <= 0) {
        CloseClipboard();
        return false;
    }

    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, sizeof(wchar_t) * length);
    if (!memory) {
        CloseClipboard();
        return false;
    }

    auto buffer = static_cast<wchar_t*>(GlobalLock(memory));
    if (!buffer) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, buffer, length);
    GlobalUnlock(memory);

    if (!SetClipboardData(CF_UNICODETEXT, memory)) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
#else
    (void)text;
    return false;
#endif
}

static std::string getTextFromClipboard() {
#if defined(GEODE_IS_WINDOWS)
    if (!OpenClipboard(nullptr)) return {};
    auto* handle = GetClipboardData(CF_UNICODETEXT);
    if (!handle) { CloseClipboard(); return {}; }
    auto* wide = static_cast<wchar_t*>(GlobalLock(handle));
    if (!wide) { CloseClipboard(); return {}; }
    int length = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    std::string result;
    if (length > 1) {
        result.resize(static_cast<std::size_t>(length - 1));
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, result.data(), length, nullptr, nullptr);
    }
    GlobalUnlock(handle);
    CloseClipboard();
    return result;
#else
    return {};
#endif
}

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
#include <atomic>
#include <memory>
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
    std::string video = "any";
    std::string feedbackNeeded = "any";
    std::string sort = "newest";
};

struct RequestMeta {
    int requestID = 0;
    int levelID = 0;
    std::string event = "0";
    int difficulty = 0;
    std::string difficultyKey;
    bool rated = false;
    std::string videoURL;
    std::string description;
    std::string reviewLanguage;
    std::string reviewFlags;
    std::string reviewMode;
    bool hasPlatformer = false;
    bool platformer = false;
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
static std::size_t g_requestNativeSubPage = 0;
constexpr std::size_t REQUEST_NATIVE_BATCH_SIZE = 50;

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

static std::string unescapeRequestField(std::string const& value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        char c = value[i];
        if (c != '\\' || i + 1 >= value.size()) {
            out.push_back(c);
            continue;
        }

        char escaped = value[++i];
        if (escaped == 'n') out.push_back('\n');
        else if (escaped == 'r') out.push_back('\r');
        else if (escaped == 't') out.push_back('\t');
        else if (escaped == '\\') out.push_back('\\');
        else {
            out.push_back('\\');
            out.push_back(escaped);
        }
    }
    return out;
}

static bool hasRequestVideo(std::string const& raw) {
    auto value = trim(raw);
    if (value.empty() || value == "0" || value == "None" || value == "none") return false;
    return value.starts_with("http://") || value.starts_with("https://");
}

static bool isValidWebURL(std::string const& raw) {
    auto value = trim(raw);
    if (value.empty()) return false;

    auto lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    std::size_t schemeEnd = std::string::npos;
    if (lower.rfind("https://", 0) == 0) schemeEnd = 8;
    else if (lower.rfind("http://", 0) == 0) schemeEnd = 7;
    else return false;

    auto authorityEnd = lower.find_first_of("/?#", schemeEnd);
    auto authority = lower.substr(schemeEnd, authorityEnd == std::string::npos
        ? std::string::npos
        : authorityEnd - schemeEnd);
    if (authority.empty()) return false;
    if (authority.find('@') != std::string::npos) return false;

    auto colon = authority.rfind(':');
    if (colon != std::string::npos) {
        auto port = authority.substr(colon + 1);
        if (port.empty() || !std::all_of(port.begin(), port.end(), [](unsigned char c) {
            return std::isdigit(c) != 0;
        })) return false;
    }
    return authority.find('.') != std::string::npos || authority == "localhost";
}

static bool isYouTubeURL(std::string const& raw) {
    auto value = trim(raw);
    if (value.empty()) return false;

    // Validate the actual URL host instead of searching for "youtube.com"
    // anywhere in the string. This rejects things such as:
    //   https://evil.com/youtube.com/watch?v=...
    //   https://youtube.com.evil.com/watch?v=...
    //   https://youtube.com@evil.com/watch?v=...
    auto lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    std::size_t schemeEnd = std::string::npos;
    if (lower.rfind("https://", 0) == 0) schemeEnd = 8;
    else if (lower.rfind("http://", 0) == 0) schemeEnd = 7;
    else return false;

    auto authorityEnd = lower.find_first_of("/?#", schemeEnd);
    auto authority = lower.substr(schemeEnd, authorityEnd == std::string::npos
        ? std::string::npos
        : authorityEnd - schemeEnd);
    if (authority.empty() || authority.find('@') != std::string::npos) return false;

    // Split host and optional port. Only the real YouTube hostnames are allowed.
    std::string host = authority;
    std::string port;
    auto colon = authority.rfind(':');
    if (colon != std::string::npos) {
        host = authority.substr(0, colon);
        port = authority.substr(colon + 1);
        if (port.empty()) return false;
        for (char c : port) {
            if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        }
        if (port != "80" && port != "443") return false;
    }

    return host == "youtube.com" ||
           host == "www.youtube.com" ||
           host == "m.youtube.com" ||
           host == "music.youtube.com" ||
           host == "youtu.be" ||
           host == "www.youtu.be" ||
           host == "youtube-nocookie.com" ||
           host == "www.youtube-nocookie.com";
}

static std::string requestLanguageLabel(std::string raw) {
    raw = trim(raw);
    std::transform(raw.begin(), raw.end(), raw.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (raw == "us" || raw == "en" || raw == "english") return "English";
    if (raw == "ru" || raw == "russian") return "Russian";
    if (raw == "esp" || raw == "es" || raw == "spanish") return "Spanish";
    if (raw == "fr" || raw == "french") return "French";
    return raw.empty() ? "Not specified" : raw;
}

static std::string normalizedRequestReviewMode(RequestMeta const& meta) {
    auto mode = trim(meta.reviewMode);
    std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (mode == "enable review and feedback" || mode == "review_feedback" || mode == "review+feedback") {
        return "review_feedback";
    }
    if (mode == "enable only feedback" || mode == "feedback") return "feedback";
    if (mode == "enable only review" || mode == "enable" || mode == "review") return "review";
    if (mode == "disable" || mode == "disabled" || mode == "0" || mode == "none") return "disabled";

    // Backward compatibility with v2.0.32 bridge responses, which did not carry
    // RequestReviewStat. Infer the form mode from the stored Review value only when possible.
    auto flags = trim(meta.reviewFlags);
    std::transform(flags.begin(), flags.end(), flags.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (flags.empty() || flags == "0" || flags == "none") return "disabled";
    bool mentionsReview = flags.find("review") != std::string::npos;
    bool mentionsFeedback = flags.find("feedback") != std::string::npos;
    if (mentionsReview && mentionsFeedback) return "review_feedback";
    if (mentionsFeedback) return "feedback";
    return "review";
}

static bool requestReviewEnabled(RequestMeta const& meta) {
    auto mode = normalizedRequestReviewMode(meta);
    return mode == "review" || mode == "review_feedback";
}

static bool requestFeedbackEnabled(RequestMeta const& meta) {
    auto mode = normalizedRequestReviewMode(meta);
    return mode == "feedback" || mode == "review_feedback";
}

static bool requestWantsReview(RequestMeta const& meta) {
    if (!requestReviewEnabled(meta)) return false;
    auto flags = trim(meta.reviewFlags);
    std::transform(flags.begin(), flags.end(), flags.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (normalizedRequestReviewMode(meta) == "review") {
        return flags == "yes" || flags.starts_with("yes ") || flags.find("yes review") != std::string::npos;
    }
    return flags.find("yes review") != std::string::npos;
}

static bool requestWantsFeedback(RequestMeta const& meta) {
    if (!requestFeedbackEnabled(meta)) return false;
    auto flags = trim(meta.reviewFlags);
    std::transform(flags.begin(), flags.end(), flags.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (normalizedRequestReviewMode(meta) == "feedback") {
        return flags == "yes" || flags.starts_with("yes ") || flags.find("yes feedback") != std::string::npos;
    }
    return flags.find("yes feedback") != std::string::npos;
}

static bool hasRequestDescription(std::string const& raw) {
    auto value = trim(raw);
    return !value.empty() && value != "0" && value != "None" && value != "none";
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

static matjson::Value buildPayload(SendSnapshot const& snapshot, bool isTest, RequestContext const* context = nullptr, bool const* noPingOverride = nullptr) {
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
        body["noPing"] = noPingOverride ? *noPingOverride : noPingFor(*context);
    }
    return body;
}

static void reportSend(SendSnapshot snapshot, bool isTest = false, RequestContext const* context = nullptr, bool const* noPingOverride = nullptr) {
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

    auto body = buildPayload(snapshot, isTest, context, noPingOverride);
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

    bool hasVideo = hasRequestVideo(meta.videoURL);
    if (g_filters.video == "with" && !hasVideo) return false;
    if (g_filters.video == "without" && hasVideo) return false;

    bool wantsFeedback = g_client.mode == "reviewer"
        ? requestWantsReview(meta)
        : requestWantsFeedback(meta);
    if (g_filters.feedbackNeeded == "needed" && !wantsFeedback) return false;
    if (g_filters.feedbackNeeded == "not_needed" && wantsFeedback) return false;

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
    g_requestNativeSubPage = 0;
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
            if (parts.size() >= 8) meta.videoURL = unescapeRequestField(parts[7]);
            if (parts.size() >= 9) meta.description = unescapeRequestField(parts[8]);
            if (parts.size() >= 10) meta.reviewLanguage = unescapeRequestField(parts[9]);
            if (parts.size() >= 11) meta.reviewFlags = unescapeRequestField(parts[10]);
            if (parts.size() >= 12) meta.reviewMode = unescapeRequestField(parts[11]);
            if (parts.size() >= 13) {
                meta.hasPlatformer = true;
                meta.platformer = parseInt(parts[12]) != 0;
            }

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

    bool initFor(
        CCSize const& size,
        std::function<void(CCPoint const&)> onPoint
    ) {
        if (!CCLayer::init()) return false;
        m_onPoint = std::move(onPoint);
        setContentSize(size);
        setAnchorPoint({0.f, 0.f});
        setTouchEnabled(true);
        return true;
    }

    bool pointInside(CCPoint const& local) const {
        auto size = getContentSize();
        return local.x >= 0.f && local.y >= 0.f && local.x <= size.width && local.y <= size.height;
    }

public:
    static FeedbackTouchLayer* create(
        CCSize const& size,
        std::function<void(CCPoint const&)> onPoint
    ) {
        auto* ret = new FeedbackTouchLayer();
        if (ret && ret->initFor(size, std::move(onPoint))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    void registerWithTouchDispatcher() override {
        // Mouse clicks on desktop and finger taps on mobile both arrive through the Cocos
        // touch dispatcher. Swallow only gestures that start inside the feedback field.
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
};

class FeedbackPopup;
static CCTextInputNode* g_feedbackIMEInput = nullptr;
static std::function<void(std::string const&)> g_feedbackIMEInsert;
static std::function<void()> g_feedbackIMEBackspace;
static std::function<void()> g_feedbackIMEDelete;
static std::chrono::steady_clock::time_point g_feedbackLastDeleteEvent{};

static bool feedbackDeleteEventAlreadyHandled() {
    auto now = std::chrono::steady_clock::now();
    if (g_feedbackLastDeleteEvent.time_since_epoch().count() != 0 &&
        std::chrono::duration_cast<std::chrono::milliseconds>(now - g_feedbackLastDeleteEvent).count() < 1) {
        return true;
    }
    g_feedbackLastDeleteEvent = now;
    return false;
}

class FeedbackPopup final : public geode::Popup {
protected:
    struct WrappedLine {
        std::string text;
        std::size_t startByte = 0;
        std::size_t endByte = 0;
    };

    RequestContext m_context;
    geode::TextInput* m_input = nullptr;
    CCLabelTTF* m_measureLabel = nullptr;
    CCLabelBMFont* m_counter = nullptr;
    CCLabelBMFont* m_feedbackLanguage = nullptr;
    CCLabelBMFont* m_feedbackRequirement = nullptr;
    CCLabelBMFont* m_placeholder = nullptr;
    FeedbackTouchLayer* m_touchLayer = nullptr;
    CCLayerColor* m_caret = nullptr;
    std::vector<CCLabelTTF*> m_lineLabels;
    std::string m_value;
    std::size_t m_cursorByte = 0;
    // Android IME can occasionally deliver a UTF-8 code point in multiple callbacks.
    // Keep an incomplete trailing sequence out of the visible editor until it is complete.
    std::string m_pendingUtf8Insert;
    bool m_focused = false;
    bool m_cursorInitialized = false;
    bool m_nativeCursorSettled = false;
    std::size_t m_firstVisibleLine = 0;
    std::size_t m_visibleLineCount = 1;
    geode::ListenerHandle m_keyboardListener;
    geode::ListenerHandle m_scrollWheelListener;
    std::unordered_map<std::string, float> m_glyphWidthCache;
    bool m_manualScroll = false;

    static constexpr float FIELD_W = 250.f;
    static constexpr float FIELD_H = 94.f;
    static constexpr float FIELD_X = 45.f;
    static constexpr float FIELD_Y = 66.f;
    static constexpr float TEXT_LEFT = FIELD_X + 10.f;
    // Keep the visible text a little higher inside the unchanged field rectangle.
#if defined(GEODE_IS_ANDROID)
    static constexpr float TEXT_TOP = FIELD_Y + FIELD_H - 8.f;
#else
    static constexpr float TEXT_TOP = FIELD_Y + FIELD_H - 9.f;
#endif
    static constexpr float TEXT_SCALE = .58f;
    static constexpr float LINE_STEP = 11.2f;
    static constexpr std::size_t MAX_VISIBLE_LINES = 7;

    static unsigned int decodeUtf8CodePoint(std::string const& text, std::size_t index, std::size_t& length) {
        length = 0;
        if (index >= text.size()) return 0;
        auto b0 = static_cast<unsigned char>(text[index]);
        if (b0 < 0x80) { length = 1; return b0; }
        std::size_t need = 0;
        unsigned int cp = 0;
        if ((b0 & 0xE0) == 0xC0) { need = 2; cp = b0 & 0x1F; }
        else if ((b0 & 0xF0) == 0xE0) { need = 3; cp = b0 & 0x0F; }
        else if ((b0 & 0xF8) == 0xF0) { need = 4; cp = b0 & 0x07; }
        else return 0;
        if (index + need > text.size()) return 0;
        for (std::size_t i = 1; i < need; ++i) {
            auto b = static_cast<unsigned char>(text[index + i]);
            if ((b & 0xC0) != 0x80) return 0;
            cp = (cp << 6) | (b & 0x3F);
        }
        if ((need == 2 && cp < 0x80) ||
            (need == 3 && cp < 0x800) ||
            (need == 4 && cp < 0x10000) ||
            cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
        length = need;
        return cp;
    }

    static std::string cocosSafeDisplayText(std::string const& text) {
        std::string result;
        result.reserve(text.size());
        for (std::size_t i = 0; i < text.size();) {
            std::size_t length = 0;
            auto cp = decodeUtf8CodePoint(text, i, length);
            if (length == 0) break;
            if (!((cp >= 0x0300 && cp <= 0x036F) ||
                  (cp >= 0x1AB0 && cp <= 0x1AFF) ||
                  (cp >= 0x1DC0 && cp <= 0x1DFF) ||
                  (cp >= 0x20D0 && cp <= 0x20FF) ||
                  (cp >= 0xFE20 && cp <= 0xFE2F))) {
                result.append(text, i, length);
            }
            i += length;
        }
        return result;
    }

    float measuredRawWidth(std::string const& text) {
        if (!m_measureLabel || text.empty()) return 0.f;
        auto safeText = cocosSafeDisplayText(text);
        if (safeText.empty()) return 0.f;
        m_measureLabel->setString(safeText.c_str());
        return m_measureLabel->getContentSize().width;
    }

    static bool isCombiningMark(std::string const& glyph) {
        if (glyph.empty()) return false;
        auto lead = static_cast<unsigned char>(glyph[0]);
        unsigned int cp = 0;
        if ((lead & 0x80) == 0) {
            cp = lead;
        } else if ((lead & 0xE0) == 0xC0 && glyph.size() >= 2) {
            cp = ((lead & 0x1F) << 6) | (static_cast<unsigned char>(glyph[1]) & 0x3F);
        } else if ((lead & 0xF0) == 0xE0 && glyph.size() >= 3) {
            cp = ((lead & 0x0F) << 12) |
                 ((static_cast<unsigned char>(glyph[1]) & 0x3F) << 6) |
                 (static_cast<unsigned char>(glyph[2]) & 0x3F);
        } else if ((lead & 0xF8) == 0xF0 && glyph.size() >= 4) {
            cp = ((lead & 0x07) << 18) |
                 ((static_cast<unsigned char>(glyph[1]) & 0x3F) << 12) |
                 ((static_cast<unsigned char>(glyph[2]) & 0x3F) << 6) |
                 (static_cast<unsigned char>(glyph[3]) & 0x3F);
        } else {
            return false;
        }
        return (cp >= 0x0300 && cp <= 0x036F) ||
               (cp >= 0x1AB0 && cp <= 0x1AFF) ||
               (cp >= 0x1DC0 && cp <= 0x1DFF) ||
               (cp >= 0x20D0 && cp <= 0x20FF) ||
               (cp >= 0xFE20 && cp <= 0xFE2F);
    }

    float measuredGlyphWidth(std::string const& glyph) {
        if (glyph.empty() || isCombiningMark(glyph)) return 0.f;
        auto it = m_glyphWidthCache.find(glyph);
        if (it != m_glyphWidthCache.end()) return it->second;
        auto width = measuredRawWidth(glyph);
        m_glyphWidthCache.emplace(glyph, width);
        return width;
    }

    float measuredWidth(std::string const& text) {
        // Measuring every growing substring with CCLabelTTF is O(n^2) and becomes
        // very expensive near the 1500-character limit. Cache individual glyph
        // advances instead and sum them in O(n).
        float width = 0.f;
        for (std::size_t i = 0; i < text.size();) {
            auto next = nextUtf8Boundary(text, i);
            width += measuredGlyphWidth(text.substr(i, next - i));
            i = next;
        }
        return width * TEXT_SCALE;
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
        std::size_t count = 0;
        for (std::size_t i = 0; i < text.size();) {
            i = nextUtf8Boundary(text, i);
            ++count;
        }
        return count;
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
        constexpr float maxWidth = FIELD_W - 20.f;

        std::size_t paragraphStart = 0;
        while (paragraphStart <= text.size()) {
            auto paragraphEnd = text.find('\n', paragraphStart);
            if (paragraphEnd == std::string::npos) paragraphEnd = text.size();

            if (paragraphStart == paragraphEnd) {
                lines.push_back({"", paragraphStart, paragraphEnd});
            } else {
                std::size_t lineStart = paragraphStart;
                std::size_t cursor = paragraphStart;
                std::size_t lastBreak = std::string::npos;
                std::size_t lastBreakNext = std::string::npos;
                float lineWidth = 0.f;

                while (cursor < paragraphEnd) {
                    auto next = nextUtf8Boundary(text, cursor);
                    auto glyph = text.substr(cursor, next - cursor);
                    if (glyph == "\r") { cursor = next; continue; }
                    if (glyph == " " || glyph == "\t") {
                        lastBreak = cursor;
                        lastBreakNext = next;
                    }

                    auto glyphWidth = measuredGlyphWidth(glyph) * TEXT_SCALE;
                    if (cursor > lineStart && lineWidth + glyphWidth > maxWidth) {
                        if (lastBreak != std::string::npos && lastBreak >= lineStart) {
                            lines.push_back({text.substr(lineStart, lastBreak - lineStart), lineStart, lastBreak});
                            lineStart = lastBreakNext;
                            while (lineStart < paragraphEnd && (text[lineStart] == ' ' || text[lineStart] == '\t')) ++lineStart;
                            cursor = lineStart;
                        } else {
                            lines.push_back({text.substr(lineStart, cursor - lineStart), lineStart, cursor});
                            lineStart = cursor;
                        }
                        lastBreak = std::string::npos;
                        lastBreakNext = std::string::npos;
                        lineWidth = 0.f;
                        continue;
                    }

                    lineWidth += glyphWidth;
                    cursor = next;
                }
                lines.push_back({text.substr(lineStart, paragraphEnd - lineStart), lineStart, paragraphEnd});
            }

            if (paragraphEnd == text.size()) break;
            paragraphStart = paragraphEnd + 1;
            if (paragraphStart == text.size()) {
                lines.push_back({"", paragraphStart, paragraphStart});
                break;
            }
        }

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

    geode::ListenerResult handleScrollWheel(double, double dy) {
        if (!m_focused || !m_mainLayer) return geode::ListenerResult::Propagate;

        // Only react when the mouse is over the feedback field.
        auto mouse = m_mainLayer->convertToNodeSpace(geode::cocos::getMousePos());
        if (mouse.x < FIELD_X || mouse.x > FIELD_X + FIELD_W ||
            mouse.y < FIELD_Y || mouse.y > FIELD_Y + FIELD_H) {
            return geode::ListenerResult::Propagate;
        }

        auto lines = wrapText(m_value);
        if (lines.size() <= MAX_VISIBLE_LINES || std::abs(dy) < 0.001) {
            return geode::ListenerResult::Propagate;
        }

        auto maxFirst = lines.size() - MAX_VISIBLE_LINES;
        auto current = std::min(m_firstVisibleLine, maxFirst);
        // One wheel notch moves roughly three text rows, matching normal text-field scrolling.
        auto steps = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(std::abs(dy) * 3.0)));
        if (dy > 0.0) {
            current = current > steps ? current - steps : 0;
        } else {
            current = std::min(maxFirst, current + steps);
        }
        m_firstVisibleLine = current;
        m_manualScroll = true;
        renderText();
        return geode::ListenerResult::Stop;
    }

    void renderText() {
        if (!m_mainLayer) return;
        clearRenderedLines();

        auto lines = wrapText(m_value);
        auto visibleCount = std::min<std::size_t>(MAX_VISIBLE_LINES, lines.size());
        auto caretLine = cursorLineIndex(lines);

        std::size_t firstVisible = 0;
        if (lines.size() > visibleCount) {
            if (m_manualScroll) {
                firstVisible = std::min(m_firstVisibleLine, lines.size() - visibleCount);
                // If editing moves the caret outside the visible area, snap back to it.
                if (caretLine < firstVisible) {
                    firstVisible = caretLine;
                    m_manualScroll = false;
                } else if (caretLine >= firstVisible + visibleCount) {
                    firstVisible = caretLine - visibleCount + 1;
                    m_manualScroll = false;
                }
            } else if (m_focused) {
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
            auto safeDisplayText = cocosSafeDisplayText(line.text);
            auto* label = CCLabelTTF::create(safeDisplayText.c_str(), "Arial", 18.f);
            if (!label) continue;
            label->setScale(TEXT_SCALE);
            label->setAnchorPoint({0.f, 1.f});
            label->setPosition({TEXT_LEFT, TEXT_TOP + 1.5f - static_cast<float>(row) * LINE_STEP
#if defined(GEODE_IS_ANDROID)
                + 2.0f
#endif
            });
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
                auto x = TEXT_LEFT + measuredRawWidth(prefix) * TEXT_SCALE - 0.3f;
                auto y = TEXT_TOP + 1.5f - static_cast<float>(row) * LINE_STEP - 11.0f;
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
        if (m_feedbackLanguage) {
            auto language = requestLanguageLabel(m_context.request.reviewLanguage);
            if (!language.empty() && language != "Not specified") {
                m_feedbackLanguage->setString(("LANGUAGE: " + language).c_str());
                m_feedbackLanguage->setVisible(true);
            } else {
                m_feedbackLanguage->setString("");
                m_feedbackLanguage->setVisible(false);
            }
        }
        if (m_feedbackRequirement) {
            // Do not show a feedback requirement when the request has no feedback
            // metadata at all.
            if (requestFeedbackEnabled(m_context.request)) {
                auto text = std::string("FEEDBACK: ") +
                    (requestWantsFeedback(m_context.request) ? "REQUIRED" : "NOT REQUIRED");
                m_feedbackRequirement->setString(text.c_str());
                m_feedbackRequirement->setVisible(true);
            } else {
                m_feedbackRequirement->setString("");
                m_feedbackRequirement->setVisible(false);
            }
        }
        renderText();
        if (m_placeholder) m_placeholder->setVisible(m_value.empty());
    }

    static std::size_t clampUtf8Boundary(std::string const& text, std::size_t byteOffset) {
        byteOffset = std::min(byteOffset, text.size());
        while (
            byteOffset > 0 && byteOffset < text.size() &&
            (static_cast<unsigned char>(text[byteOffset]) & 0xC0) == 0x80
        ) {
            --byteOffset;
        }
        return byteOffset;
    }

    std::size_t nativeCursorByteOffset() const {
        return clampUtf8Boundary(m_value, m_cursorByte);
    }

    void setNativeCursorFromByte(std::size_t byteOffset) {
        m_cursorByte = clampUtf8Boundary(m_value, byteOffset);
#if defined(GEODE_IS_ANDROID)
        // Android's IME needs a real native buffer/cursor to perform Backspace reliably.
        // Keep that buffer synchronized only on Android; Windows keeps the existing
        // keyboard/UTF-8 path untouched.
        if (m_input) {
            if (auto* node = m_input->getInputNode()) {
                if (node->m_textField) {
                    // CCTextFieldTTF cursor position is a character index, not a UTF-8 byte
                    // offset. Using m_cursorByte breaks as soon as the text contains multibyte
                    // characters and becomes especially visible after reopening saved text.
                    auto cursor = std::min(m_cursorByte, m_value.size());
                    auto prefix = m_value.substr(0, cursor);
                    node->m_textField->m_uCursorPos = static_cast<int>(utf8CharCount(prefix));
                    node->updateBlinkLabel();
                }
            }
        }
#endif
    }

    void syncNativeCursor(float) {}

    void settleNativeCursor(float) {
        setNativeCursorFromByte(m_cursorByte);
        m_nativeCursorSettled = true;
        renderText();
    }

    void insertUtf8AtCursor(std::string const& text) {
        if (text.empty()) return;
        auto insertion = text;
        auto room = FEEDBACK_LIMIT - utf8CharCount(m_value);
        truncateUtf8ToChars(insertion, room);
        if (insertion.empty()) return;
        m_value.insert(m_cursorByte, insertion);
        m_cursorByte += insertion.size();
        refreshVisuals();
#if defined(GEODE_IS_ANDROID)
        setNativeCursorFromByte(m_cursorByte);
#endif
    }

    void eraseBackward() {
        m_cursorByte = std::min(m_cursorByte, m_value.size());
        m_cursorByte = clampUtf8Boundary(m_value, m_cursorByte);
        if (m_cursorByte == 0) return;
        auto previous = m_cursorByte - 1;
        while (previous > 0 && (static_cast<unsigned char>(m_value[previous]) & 0xC0) == 0x80) --previous;
        m_value.erase(previous, m_cursorByte - previous);
        m_cursorByte = previous;
        refreshVisuals();
#if defined(GEODE_IS_ANDROID)
        setNativeCursorFromByte(m_cursorByte);
#endif
    }

    void eraseForward() {
        m_cursorByte = std::min(m_cursorByte, m_value.size());
        m_cursorByte = clampUtf8Boundary(m_value, m_cursorByte);
        if (m_cursorByte >= m_value.size()) return;
        auto next = nextUtf8Boundary(m_value, m_cursorByte);
        m_value.erase(m_cursorByte, next - m_cursorByte);
        refreshVisuals();
#if defined(GEODE_IS_ANDROID)
        setNativeCursorFromByte(m_cursorByte);
#endif
    }

    void moveCursorHorizontal(int direction) {
        if (direction < 0) {
            if (m_cursorByte == 0) return;
            --m_cursorByte;
            while (m_cursorByte > 0 && (static_cast<unsigned char>(m_value[m_cursorByte]) & 0xC0) == 0x80) --m_cursorByte;
        } else {
            m_cursorByte = nextUtf8Boundary(m_value, m_cursorByte);
        }
        refreshVisuals();
    }

    void moveCursorVertical(int direction) {
        auto lines = wrapText(m_value);
        if (lines.empty()) return;
        auto current = cursorLineIndex(lines);
        if (direction < 0) {
            if (current == 0) return;
        } else if (current + 1 >= lines.size()) return;
        auto const& from = lines[current];
        auto prefixBytes = m_cursorByte > from.startByte ? m_cursorByte - from.startByte : 0;
        auto wantedX = measuredWidth(from.text.substr(0, std::min(prefixBytes, from.text.size())));
        auto targetIndex = direction < 0 ? current - 1 : current + 1;
        auto const& target = lines[targetIndex];
        m_cursorByte = target.startByte + nearestBoundaryInLine(target.text, wantedX);
        refreshVisuals();
    }

    void handleNativeInsert(std::string const& text) {
        if (text.empty()) return;

        // Do not let an incomplete/invalid UTF-8 fragment reach Cocos2d. Android IMEs
        // may split a multi-byte code point across callbacks while composing text.
        m_pendingUtf8Insert += text;

        std::string complete;
        complete.reserve(m_pendingUtf8Insert.size());
        std::size_t i = 0;
        while (i < m_pendingUtf8Insert.size()) {
            std::size_t length = 0;
            auto cp = decodeUtf8CodePoint(m_pendingUtf8Insert, i, length);
            if (length == 0) {
                // A leading byte without all continuation bytes is kept for the next
                // callback. An actually invalid byte is discarded so it cannot poison
                // the editor buffer indefinitely.
                auto b = static_cast<unsigned char>(m_pendingUtf8Insert[i]);
                bool incomplete = ((b & 0xE0) == 0xC0 && m_pendingUtf8Insert.size() - i < 2) ||
                                   ((b & 0xF0) == 0xE0 && m_pendingUtf8Insert.size() - i < 3) ||
                                   ((b & 0xF8) == 0xF0 && m_pendingUtf8Insert.size() - i < 4);
                if (incomplete) break;
                ++i;
                continue;
            }
            complete.append(m_pendingUtf8Insert, i, length);
            i += length;
        }

        if (i > 0) m_pendingUtf8Insert.erase(0, i);
        if (!complete.empty()) insertUtf8AtCursor(complete);
    }

    void setValueFromInput(std::string const&) {
        // Kept for compatibility with the TextInput callback; native text is not authoritative.
    }

    void syncValueFromInput() {
        // m_value is the authoritative UTF-8 buffer; the hidden native TextInput is only the IME host.
    }

    void placeCursorFromFieldPoint(CCPoint const& local) {
        if (!m_input) return;
        focusInput();

        auto lines = wrapText(m_value);
        if (lines.empty()) return;

        auto first = std::min(m_firstVisibleLine, lines.size() - 1);
        auto count = std::min<std::size_t>(m_visibleLineCount, lines.size() - first);
        if (count == 0) count = 1;

        // TEXT_TOP is expressed in popup coordinates; translate it into the field layer.
        auto firstLineY = (TEXT_TOP + 0.5f - FIELD_Y) - 4.8f;
        auto rowFloat = (firstLineY - local.y) / LINE_STEP;
        auto rowSigned = static_cast<long>(std::lround(rowFloat));
        rowSigned = std::clamp<long>(rowSigned, 0, static_cast<long>(count - 1));
        auto lineIndex = first + static_cast<std::size_t>(rowSigned);
        auto const& line = lines[lineIndex];

        auto wantedX = local.x - (TEXT_LEFT - FIELD_X);
        auto inLineByte = nearestBoundaryInLine(line.text, wantedX);
        auto byteOffset = std::min(line.startByte + inLineByte, m_value.size());
        setNativeCursorFromByte(byteOffset);
        m_cursorInitialized = true;
        m_nativeCursorSettled = true;
        refreshVisuals();
    }


    void focusInput() {
        if (!m_input) return;
        m_focused = true;
        m_nativeCursorSettled = false;
        if (m_caret) {
            m_caret->stopAllActions();
            m_caret->runAction(CCRepeatForever::create(CCBlink::create(.9f, 1)));
        }
#if defined(GEODE_IS_ANDROID)
        // Android IME needs the real native field to contain the current text before focus.
        // If it is empty when focus/re-focus happens, Backspace can stop working after a save
        // or after moving the cursor. The custom UTF-8 buffer remains authoritative.
        if (m_input) {
            auto native = gdToStd(m_input->getString());
            if (native != m_value) {
                m_input->setString(gd::string(m_value.c_str()), false);
            }
        }
#endif
        m_input->focus();

        // A hidden CCTextInputNode may asynchronously reset its insertion point to 0 directly
        // after focus(). Start at the end for the first focus, then restore it one frame later.
        if (!m_cursorInitialized) {
            m_cursorByte = m_value.size();
            m_cursorInitialized = true;
        }
        setNativeCursorFromByte(m_cursorByte);
        this->scheduleOnce(schedule_selector(FeedbackPopup::settleNativeCursor), 0.f);
        refreshVisuals();
    }

    geode::ListenerResult handleKeyboardEvent(geode::KeyboardInputData& data) {
        auto key = data.key;
        auto pressed = data.action == geode::KeyboardInputData::Action::Press ||
            data.action == geode::KeyboardInputData::Action::Repeat;
        if (!pressed) return geode::ListenerResult::Propagate;

        // Escape should close the feedback popup even before the text field itself
        // has been clicked/focused.
        if (key == KEY_Escape) {
            dismissEditor(nullptr);
            return geode::ListenerResult::Stop;
        }

        if (!m_focused) return geode::ListenerResult::Propagate;

        auto ctrl = static_cast<bool>(data.modifiers & geode::KeyboardModifier::Control);
        auto alt = static_cast<bool>(data.modifiers & geode::KeyboardModifier::Alt);
        auto super = static_cast<bool>(data.modifiers & geode::KeyboardModifier::Super);

        if (ctrl && key == KEY_V) {
            auto pasted = getTextFromClipboard();
            if (!pasted.empty()) insertUtf8AtCursor(pasted);
            return geode::ListenerResult::Stop;
        }
        if (ctrl || alt || super) return geode::ListenerResult::Propagate;

        switch (key) {
            case KEY_Enter:
                insertUtf8AtCursor("\n");
                return geode::ListenerResult::Stop;
            case KEY_Backspace: eraseBackward(); return geode::ListenerResult::Stop;
            case KEY_Delete: eraseForward(); return geode::ListenerResult::Stop;
            case KEY_Left: moveCursorHorizontal(-1); return geode::ListenerResult::Stop;
            case KEY_Right: moveCursorHorizontal(1); return geode::ListenerResult::Stop;
            case KEY_Up: moveCursorVertical(-1); return geode::ListenerResult::Stop;
            case KEY_Down: moveCursorVertical(1); return geode::ListenerResult::Stop;
            case KEY_Home: {
                auto lines = wrapText(m_value);
                if (!lines.empty()) m_cursorByte = lines[cursorLineIndex(lines)].startByte;
                refreshVisuals();
                return geode::ListenerResult::Stop;
            }
            case KEY_End: {
                auto lines = wrapText(m_value);
                if (!lines.empty()) m_cursorByte = lines[cursorLineIndex(lines)].endByte;
                refreshVisuals();
                return geode::ListenerResult::Stop;
            }
            default: break;
        }

        auto text = windowsKeyboardText(data);
        if (!text.empty()) {
            insertUtf8AtCursor(text);
            return geode::ListenerResult::Stop;
        }
        return geode::ListenerResult::Propagate;
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
        m_measureLabel = CCLabelTTF::create("", "Arial", 18.f);
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
        m_input->setString(gd::string(""), false);
        m_input->setCallback([this](std::string const&) {});
        m_mainLayer->addChild(m_input, 0);
        g_feedbackIMEInput = m_input->getInputNode();
        g_feedbackIMEInsert = [this](std::string const& text) { this->handleNativeInsert(text); };
        g_feedbackIMEBackspace = [this]() { this->eraseBackward(); };
        g_feedbackIMEDelete = [this]() { this->eraseForward(); };
        this->setKeyboardEnabled(true);
        m_keyboardListener = geode::KeyboardInputEvent().listen(
            [this](geode::KeyboardInputData& data) { return this->handleKeyboardEvent(data); },
            geode::Priority::VeryEarly
        );
        m_scrollWheelListener = geode::ScrollWheelEvent().listen(
            [this](double dx, double dy) { return this->handleScrollWheel(dx, dy); },
            geode::Priority::VeryEarly
        );

        m_placeholder = CCLabelBMFont::create("WRITE FEEDBACK...", "chatFont.fnt");
        m_placeholder->setScale(.48f);
        m_placeholder->setOpacity(145);
        m_placeholder->setAnchorPoint({0.f, .5f});
        m_placeholder->setPosition({TEXT_LEFT, TEXT_TOP - 4.9f});
        m_mainLayer->addChild(m_placeholder, 6);

        m_caret = CCLayerColor::create(ccc4(255, 255, 255, 255), .62f, 9.2f);
        if (m_caret) {
            // The editor is not focused when the popup first opens. Do not start the
            // blink action yet, otherwise the caret can flash at its default position
            // for a frame before the first real focus.
            m_caret->setVisible(false);
            m_caret->setPosition({TEXT_LEFT, TEXT_TOP - 14.f});
            m_mainLayer->addChild(m_caret, 7);
        }

        m_touchLayer = FeedbackTouchLayer::create(
            CCSize(FIELD_W, FIELD_H),
            [this](CCPoint const& local) { this->placeCursorFromFieldPoint(local); }
        );
        if (!m_touchLayer) return false;
        m_touchLayer->setPosition({FIELD_X, FIELD_Y});
        m_mainLayer->addChild(m_touchLayer, 20);

        m_counter = CCLabelBMFont::create("0/1500", "goldFont.fnt");
        m_counter->setScale(.27f);
        m_counter->setAnchorPoint({1.f, .5f});
        m_counter->setPosition({300.f, 58.f});
        m_mainLayer->addChild(m_counter);

        m_feedbackLanguage = CCLabelBMFont::create("", "goldFont.fnt");
        if (m_feedbackLanguage) {
            m_feedbackLanguage->setScale(.22f);
            m_feedbackLanguage->setAnchorPoint({0.f, .5f});
            m_feedbackLanguage->setPosition({40.f, 58.f});
            m_mainLayer->addChild(m_feedbackLanguage, 2);
        }

        m_feedbackRequirement = CCLabelBMFont::create("", "goldFont.fnt");
        if (m_feedbackRequirement) {
            m_feedbackRequirement->setScale(.22f);
            m_feedbackRequirement->setAnchorPoint({.5f, .5f});
            m_feedbackRequirement->setPosition({170.f, 58.f});
            m_mainLayer->addChild(m_feedbackRequirement, 2);
        }

        auto* cancelSpr = ButtonSprite::create("CANCEL", 80, true, "bigFont.fnt", "GJ_button_04.png", 30.f, .58f);
        auto* cancelBtn = CCMenuItemSpriteExtra::create(cancelSpr, this, menu_selector(FeedbackPopup::onCancel));
        cancelBtn->setPosition({118.f, 25.f});
        m_buttonMenu->addChild(cancelBtn);

        auto* saveSpr = ButtonSprite::create("SAVE", 80, true, "bigFont.fnt", "GJ_button_01.png", 30.f, .58f);
        auto* saveBtn = CCMenuItemSpriteExtra::create(saveSpr, this, menu_selector(FeedbackPopup::onSave));
        saveBtn->setPosition({222.f, 25.f});
        m_buttonMenu->addChild(saveBtn);

        // Keyboard/IME navigation is handled only by the native TextInput. We merely mirror
        // its actual cursor to the wrapped visual editor. This avoids double-handling arrows
        // on Windows and also works with Android/iOS/macOS IME implementations.
        refreshVisuals();
        return true;
    }

    void keyDown(cocos2d::enumKeyCodes, double) override {
        // KeyboardInputEvent handles the editor before the native CCTextInputNode receives
        // key events. Keep this delegate hook empty to avoid inserting/deleting twice.
    }

    void dismissEditor(CCObject* sender = nullptr) {
        m_pendingUtf8Insert.clear();
        if (g_feedbackIMEInput == (m_input ? m_input->getInputNode() : nullptr)) {
            g_feedbackIMEInput = nullptr;
            g_feedbackIMEInsert = {};
            g_feedbackIMEBackspace = {};
            g_feedbackIMEDelete = {};
            g_feedbackLastDeleteEvent = {};
        }
        if (m_input) m_input->defocus();
        m_focused = false;
        if (m_caret) {
            m_caret->stopAllActions();
            m_caret->setVisible(false);
        }
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

class $modify(GDRequestsFeedbackIMETextInputNode, CCTextInputNode) {
    void insertText(char const* text, int len, enumKeyCodes keyCodes) {
        if (this == g_feedbackIMEInput) {
#if !defined(GEODE_IS_WINDOWS)
            if (g_feedbackIMEInsert && text && len > 0) {
                g_feedbackIMEInsert(std::string(text, static_cast<std::size_t>(len)));
            }
#endif
            return;
        }
        CCTextInputNode::insertText(text, len, keyCodes);
    }

    void deleteBackward() {
        if (this == g_feedbackIMEInput && g_feedbackIMEBackspace) {
            if (!feedbackDeleteEventAlreadyHandled()) g_feedbackIMEBackspace();
            return;
        }
        CCTextInputNode::deleteBackward();
    }

    void deleteForward() {
        if (this == g_feedbackIMEInput && g_feedbackIMEDelete) {
            g_feedbackIMEDelete();
            return;
        }
        CCTextInputNode::deleteForward();
    }

    bool onTextFieldInsertText(CCTextFieldTTF* sender, char const* text, int nLen, enumKeyCodes keyCodes) {
        if (this == g_feedbackIMEInput) {
#if !defined(GEODE_IS_WINDOWS)
            if (g_feedbackIMEInsert && text && nLen > 0) {
                g_feedbackIMEInsert(std::string(text, static_cast<std::size_t>(nLen)));
            }
#endif
            return true;
        }
        return CCTextInputNode::onTextFieldInsertText(sender, text, nLen, keyCodes);
    }

    bool onTextFieldDeleteBackward(CCTextFieldTTF* sender, const char* delText, int nLen) {
        if (this == g_feedbackIMEInput) {
            if (g_feedbackIMEBackspace && !feedbackDeleteEventAlreadyHandled()) {
                g_feedbackIMEBackspace();
            }
            return true;
        }
        return CCTextInputNode::onTextFieldDeleteBackward(sender, delText, nLen);
    }
};

// Android can dispatch Backspace directly through CCTextFieldTTF's IME delegate
// path instead of reaching CCTextInputNode::deleteBackward(). Keep this hook
// limited to our hidden Feedback input so the existing text/UTF-8 system is untouched.
class $modify(GDRequestsFeedbackIMETextField, CCTextFieldTTF) {
    void deleteBackward() {
        if (g_feedbackIMEInput && g_feedbackIMEInput->m_textField == this) {
            if (g_feedbackIMEBackspace && !feedbackDeleteEventAlreadyHandled()) {
                g_feedbackIMEBackspace();
            }
            return;
        }
        CCTextFieldTTF::deleteBackward();
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
            auto oldWidth = std::max(1.f, label->getContentSize().width);
            auto oldScale = label->getScale();
            label->setString(replacement.c_str());
            auto newWidth = std::max(1.f, label->getContentSize().width);
            // The helper wording is longer than the vanilla MOD wording. Fit it into the same
            // title footprint instead of letting it grow past the popup frame.
            label->setScale(oldScale * std::min(1.f, oldWidth / newWidth));
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
static std::vector<std::string> const VIDEOS = {"any", "with", "without"};
static std::vector<std::string> const FEEDBACK_NEEDED = {"any", "needed", "not_needed"};
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
static std::string prettyVideo(std::string const& v) {
    if (v == "with") return "With Video";
    if (v == "without") return "No Video";
    return "Any";
}
static std::string prettyFeedbackNeeded(std::string const& v) {
    if (v == "needed") return "Needed";
    if (v == "not_needed") return "Not Needed";
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

static CCSprite* makeDifficultyTile(std::string const& key, bool selected, bool platformer = false) {
    auto* tile = CCSprite::create();
    tile->setContentSize({72.f, 61.f});
    tile->setAnchorPoint({.5f, .5f});

    auto* cache = CCSpriteFrameCache::sharedSpriteFrameCache();
    auto* frame = cache ? cache->spriteFrameByName(difficultyFrameFor(key)) : nullptr;
    auto* face = frame ? CCSprite::createWithSpriteFrame(frame) : nullptr;
    if (face) {
        // Use Geometry Dash's own difficulty-button frame as the complete visual.
        // In particular, Demon difficulties are no longer covered and redrawn with
        // our own captions: whatever caption/art GD provides stays intact.
        face->setScale(isDemonDifficulty(key) ? .74f : .80f);
        // Demon frames contain their caption inside the same native GD sprite and sit
        // visually lower than the normal difficulty frames. Raise the whole Demon
        // sprite so the caption -> star gap matches the normal difficulty rows.
        face->setPosition({36.f, isDemonDifficulty(key) ? 43.f : 43.8f});

        // Geometry Dash keeps these fully opaque and darkens inactive difficulties
        // through color modulation rather than transparency. 166 is the inactive
        // brightness observed from the native difficulty filter.
        face->setOpacity(255);
        face->setColor(selected ? ccc3(255, 255, 255) : ccc3(166, 166, 166));
        tile->addChild(face, 2);
    }

    // Keep the star counts for every rated difficulty, including all Demon tiers.
    // The whole tile uses the same selected/unselected visual state.
    auto stars = difficultyStarCount(key);
    if (stars > 0) {
        auto starText = std::to_string(stars);
        auto* starsLabel = CCLabelBMFont::create(starText.c_str(), "bigFont.fnt");
        if (starsLabel) {
            starsLabel->setScale(.40f);
            starsLabel->setAnchorPoint({1.f, .5f});
            // Restore the original Demon 10★ geometry; normal difficulties keep
            // their existing tested coordinates.
            starsLabel->setPosition(isDemonDifficulty(key) ? CCPoint{35.0f, 18.4f} : CCPoint{31.4f, 18.4f});
            starsLabel->setOpacity(255);
            starsLabel->setColor(selected ? ccc3(255, 255, 255) : ccc3(166, 166, 166));
            tile->addChild(starsLabel, 4);
        }

        auto* star = CCSprite::createWithSpriteFrameName(platformer ? "GJ_bigMoon_001.png" : "GJ_starsIcon_001.png");
        if (star) {
            // GJ_bigMoon_001 is slightly larger than GJ_starsIcon_001 in the UHD
            // sheet (168x176 vs the smaller star icon), so use a dedicated scale
            // to keep the moon visually the same size as the native star icon.
            // The moon texture is much larger than the star texture in the UHD
            // sheet. Scale it against the actual star sprite size so a platformer
            // moon occupies the same visual height as the normal star.
            float iconScale = isDemonDifficulty(key) ? .52f : .54f;
            if (platformer) {
                auto* referenceStar = CCSprite::createWithSpriteFrameName("GJ_starsIcon_001.png");
                if (referenceStar && referenceStar->getContentSize().height > 0.f && star->getContentSize().height > 0.f) {
                    iconScale = (referenceStar->getContentSize().height * .54f) / star->getContentSize().height;
                } else {
                    iconScale = .20f;
                }
            }
            star->setScale(iconScale);
            star->setPosition(isDemonDifficulty(key) ? CCPoint{47.2f, 18.1f} : CCPoint{42.8f, 18.1f});
            star->setOpacity(255);
            star->setColor(selected ? ccc3(255, 255, 255) : ccc3(166, 166, 166));
            tile->addChild(star, 4);
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
        if (!Popup::init(350.f, 250.f)) return false;
        setTitle("SELECT DIFFICULTIES", "goldFont.fnt", .58f, 20.f);

        // Keep the standard GD popup background visible. The choices themselves are
        // arranged as a native-style filter grid instead of sitting on a custom brown panel.
        constexpr float xs[] = {47.f, 109.f, 171.f, 233.f, 295.f};
        constexpr float ys[] = {182.f, 128.f, 74.f};
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
            m_summary->setScale(.48f);
            m_summary->setPosition({88.f, 25.f});
            m_mainLayer->addChild(m_summary);
        }

        auto* applySpr = ButtonSprite::create("APPLY", 78, true, "bigFont.fnt", "GJ_button_01.png", 30.f, .55f);
        auto* applyBtn = CCMenuItemSpriteExtra::create(applySpr, this, menu_selector(DifficultyPickerPopup::onApply));
        applyBtn->setPosition({256.f, 25.f});
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
    CCLabelBMFont* m_difficulty = nullptr;
    CCLabelBMFont* m_type = nullptr;
    CCLabelBMFont* m_status = nullptr;
    CCLabelBMFont* m_minSend = nullptr;
    CCLabelBMFont* m_rated = nullptr;
    CCLabelBMFont* m_video = nullptr;
    CCLabelBMFont* m_feedbackNeeded = nullptr;
    CCLabelBMFont* m_sort = nullptr;
    bool m_staff = false;
    bool m_reviewer = false;

    // Native GD-style rounded dark overlays. The popup itself keeps its normal
    // background; these shapes only darken it. The corner radius is deliberately
    // small (selector-style), not a modern pill/capsule.
    void addDarkRoundedRect(CCPoint position, CCSize size, float radius, GLubyte opacity, int z) {
        auto* shape = CCDrawNode::create();
        if (!shape) return;

        radius = std::max(0.f, std::min(radius, std::min(size.width, size.height) * .5f));
        constexpr int steps = 5;
        std::vector<CCPoint> points;
        points.reserve(steps * 4 + 4);

        const float hw = size.width * .5f;
        const float hh = size.height * .5f;
        const CCPoint centers[] = {
            { hw - radius,  hh - radius},
            {-hw + radius,  hh - radius},
            {-hw + radius, -hh + radius},
            { hw - radius, -hh + radius},
        };
        const float starts[] = {0.f, 90.f, 180.f, 270.f};
        constexpr float pi = 3.14159265358979323846f;
        for (int corner = 0; corner < 4; ++corner) {
            for (int i = 0; i <= steps; ++i) {
                float angle = (starts[corner] + i * 90.f / steps) * pi / 180.f;
                points.push_back({
                    centers[corner].x + std::cos(angle) * radius,
                    centers[corner].y + std::sin(angle) * radius
                });
            }
        }

        shape->drawPolygon(
            points.data(), static_cast<unsigned int>(points.size()),
            ccc4f(0.f, 0.f, 0.f, static_cast<float>(opacity) / 255.f),
            0.f, ccc4f(0.f, 0.f, 0.f, 0.f)
        );
        shape->setPosition(position);
        m_mainLayer->addChild(shape, z);
    }

    // Keep the popup itself in the native/default GD colour. This is only a
    // subtle translucent layer behind the controls.
    void addPanel(CCPoint position, CCSize size) {
        addDarkRoundedRect(position, size, 8.f, 40, -1);
    }

    void addText(char const* text, CCPoint position, float scale = .36f, char const* font = "goldFont.fnt") {
        auto* label = CCLabelBMFont::create(text, font);
        if (!label) return;
        label->setScale(scale);
        label->setPosition(position);
        m_mainLayer->addChild(label, 2);
    }

    void addField(CCPoint position, float width = 112.f, float height = 30.f) {
        // Match GD selector proportions: a modest rounded rectangle, no visible
        // border and no separate fill colour. It simply darkens the popup below.
        addDarkRoundedRect(position, {width, height}, 5.f, 82, 0);
    }

    CCLabelBMFont* addValue(CCPoint position) {
        auto* label = CCLabelBMFont::create("", "bigFont.fnt");
        if (!label) return nullptr;
        label->setScale(.40f);
        label->setPosition(position);
        m_mainLayer->addChild(label, 2);
        return label;
    }

    void addArrow(CCPoint position, bool right, SEL_MenuHandler handler) {
        auto* spr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        if (!spr) return;
        // Native arrow texture kept unchanged; scale it to approximately the
        // same visual height as a compact selector field.
        spr->setScale(.52f);
        spr->setFlipX(right);
        auto* btn = CCMenuItemSpriteExtra::create(spr, this, handler);
        btn->setPosition(position);
        btn->setSizeMult(1.f);
        m_buttonMenu->addChild(btn, 3);
    }

    void addCycleRow(
        char const* title,
        CCLabelBMFont*& value,
        float centerX,
        float y,
        SEL_MenuHandler left,
        SEL_MenuHandler right
    ) {
        constexpr float fieldW = 102.f;
        constexpr float fieldH = 28.f;
        // Keep the native arrow size unchanged. Place the menu-item centres using
        // the same field geometry on both sides so every arrow sits exactly on
        // the horizontal centreline of its selector.
        constexpr float gap = 14.f;
        addText(title, {centerX, y + 22.f}, .36f);
        addField({centerX, y}, fieldW, fieldH);
        value = addValue({centerX, y});
        // Arrows are tight to the field instead of floating in empty space.
        addArrow({centerX - fieldW / 2.f - gap, y}, false, left);
        addArrow({centerX + fieldW / 2.f + gap, y}, true, right);
    }

    void normalizeDependentFilters() {
        if (m_working.status == "unchecked" || m_working.status == "rejected") {
            m_working.minSend = "any";
        }
    }

    void refresh() {
        normalizeDependentFilters();
        if (m_difficulty) {
            auto text = m_working.difficulties.empty()
                ? std::string("ANY DIFFICULTY")
                : std::to_string(m_working.difficulties.size()) + " DIFFICULTIES";
            m_difficulty->setString(text.c_str());
            m_difficulty->setScale(text.size() > 13 ? .34f : .42f);
        }
        if (m_type) m_type->setString(prettyType(m_working.levelType).c_str());
        if (m_status) m_status->setString(prettyStatus(m_working.status).c_str());
        if (m_minSend) m_minSend->setString(prettyMinSend(m_working.minSend).c_str());
        if (m_rated) m_rated->setString(prettyRated(m_working.rated).c_str());
        if (m_video) m_video->setString(prettyVideo(m_working.video).c_str());
        if (m_feedbackNeeded) m_feedbackNeeded->setString(prettyFeedbackNeeded(m_working.feedbackNeeded).c_str());
        if (m_sort) m_sort->setString(prettySort(m_working.sort).c_str());
    }

    bool initFor() {
        m_working = g_filters;
        m_staff = g_client.mode == "helper" || g_client.mode == "moderator";
        m_reviewer = g_client.mode == "reviewer";

        // Compact two-column layout: smaller selectors, tighter buttons and the whole
        // filter grid lifted upward so the popup does not waste its upper area.
        float height = m_staff ? 270.f : 220.f;
        constexpr float width = 400.f;
        if (!Popup::init(width, height)) return false;
        setTitle("REQUEST FILTERS", "goldFont.fnt", .60f, 15.f);

        // Single semi-transparent dark rounded background behind the whole grid.
        addPanel({width / 2.f, height / 2.f - 4.f}, {356.f, m_staff ? 188.f : 144.f});

        constexpr float leftX = 115.f;
        constexpr float rightX = 285.f;
        // The previous pass lifted the grid too high; bring it down slightly
        // while keeping the compact spacing and the popup size unchanged.
        // Lift the complete filter grid a little without changing popup size.
        float topY = m_staff ? 214.f : 162.f;

        // Difficulty is a clickable selector itself. No extra arrow is drawn.
        addText("DIFFICULTY", {leftX, topY + 22.f}, .36f);
        addField({leftX, topY}, 102.f, 28.f);
        m_difficulty = CCLabelBMFont::create("ANY DIFFICULTY", "bigFont.fnt");
        if (m_difficulty) {
            m_difficulty->setScale(.40f);
            m_difficultyButton = CCMenuItemSpriteExtra::create(
                m_difficulty, this, menu_selector(RequestFiltersPopup::onDifficultyPicker)
            );
        }
        if (m_difficultyButton) {
            m_difficultyButton->setPosition({leftX, topY});
            m_difficultyButton->setSizeMult(1.f);
            m_buttonMenu->addChild(m_difficultyButton, 3);
        }

        addCycleRow("TYPE", m_type, rightX, topY,
            menu_selector(RequestFiltersPopup::typePrev), menu_selector(RequestFiltersPopup::typeNext));

        float y = topY - 46.f;
        if (m_staff) {
            addCycleRow("MY STATUS", m_status, leftX, y,
                menu_selector(RequestFiltersPopup::statusPrev), menu_selector(RequestFiltersPopup::statusNext));
            addCycleRow("MY SEND", m_minSend, rightX, y,
                menu_selector(RequestFiltersPopup::sendPrev), menu_selector(RequestFiltersPopup::sendNext));
            y -= 46.f;
        }

        addCycleRow("RATED", m_rated, leftX, y,
            menu_selector(RequestFiltersPopup::ratedPrev), menu_selector(RequestFiltersPopup::ratedNext));
        addCycleRow("HAS VIDEO", m_video, rightX, y,
            menu_selector(RequestFiltersPopup::videoPrev), menu_selector(RequestFiltersPopup::videoNext));
        y -= 46.f;

        addCycleRow(m_reviewer ? "REVIEW NEEDED" : "FEEDBACK NEEDED", m_feedbackNeeded, leftX, y,
            menu_selector(RequestFiltersPopup::feedbackPrev), menu_selector(RequestFiltersPopup::feedbackNext));
        addCycleRow("SORT", m_sort, rightX, y,
            menu_selector(RequestFiltersPopup::sortPrev), menu_selector(RequestFiltersPopup::sortNext));

        auto* resetSpr = ButtonSprite::create("RESET", 104, true, "bigFont.fnt", "GJ_button_04.png", 30.f, .56f);
        auto* resetBtn = CCMenuItemSpriteExtra::create(resetSpr, this, menu_selector(RequestFiltersPopup::onReset));
        resetBtn->setPosition({112.f, 31.f});
        resetBtn->setSizeMult(1.f);
        m_buttonMenu->addChild(resetBtn, 3);

        auto* applySpr = ButtonSprite::create("APPLY", 104, true, "bigFont.fnt", "GJ_button_01.png", 30.f, .56f);
        auto* applyBtn = CCMenuItemSpriteExtra::create(applySpr, this, menu_selector(RequestFiltersPopup::onApply));
        applyBtn->setPosition({288.f, 31.f});
        applyBtn->setSizeMult(1.f);
        m_buttonMenu->addChild(applyBtn, 3);

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
    void videoPrev(CCObject*) { cycleValue(m_working.video, VIDEOS, -1); refresh(); }
    void videoNext(CCObject*) { cycleValue(m_working.video, VIDEOS, 1); refresh(); }
    void feedbackPrev(CCObject*) { cycleValue(m_working.feedbackNeeded, FEEDBACK_NEEDED, -1); refresh(); }
    void feedbackNext(CCObject*) { cycleValue(m_working.feedbackNeeded, FEEDBACK_NEEDED, 1); refresh(); }
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

static void hideSubmitLoading();

struct SubmitRequestState {
    std::atomic_bool active{true};
    std::function<void()> restoreHostClose;
    std::function<void()> resetNoPingVisual;
    bool submittedNoPing = false;
    int requestID = 0;
};

class SubmitUploadDelegate final : public UploadPopupDelegate {
public:
    void onClosePopup(UploadActionPopup* popup) override;
};

static SubmitUploadDelegate g_submitUploadDelegate;
static UploadActionPopup* g_submitLoadingPopup = nullptr;
static CCNode* g_submitLoadingHost = nullptr;
static std::shared_ptr<SubmitRequestState> g_submitRequestState;

static std::shared_ptr<SubmitRequestState> showSubmitLoading(
    CCNode* host,
    char const* title,
    std::function<void()> restoreHostClose = {},
    std::function<void()> resetNoPingVisual = {},
    bool submittedNoPing = false,
    int requestID = 0
) {
    if (!host || g_submitLoadingPopup) return nullptr;

    // Use Geometry Dash's actual native UploadActionPopup. It owns the exact
    // RobTop loading/result UI, including the native close button and success state.
    auto* popup = UploadActionPopup::create(&g_submitUploadDelegate, gd::string(title));
    if (!popup) return nullptr;

    // The parent request popup must not have its own X visible while the native
    // upload popup is on screen. The native popup itself keeps its normal X.
    popup->setTouchEnabled(true);

    g_submitRequestState = std::make_shared<SubmitRequestState>();
    g_submitRequestState->restoreHostClose = std::move(restoreHostClose);
    g_submitRequestState->resetNoPingVisual = std::move(resetNoPingVisual);
    g_submitRequestState->submittedNoPing = submittedNoPing;
    g_submitRequestState->requestID = requestID;
    g_submitLoadingPopup = popup;
    g_submitLoadingHost = host;

    popup->show();
    popup->setTouchEnabled(true);
    return g_submitRequestState;
}

static void hideSubmitLoading() {
    auto state = g_submitRequestState;
    if (state) state->active.store(false);

    auto* popup = g_submitLoadingPopup;
    g_submitLoadingPopup = nullptr;
    g_submitLoadingHost = nullptr;
    g_submitRequestState.reset();

    if (state && state->restoreHostClose) {
        auto restore = std::move(state->restoreHostClose);
        restore();
    }

    if (popup) popup->closePopup();
}

void SubmitUploadDelegate::onClosePopup(UploadActionPopup* popup) {
    if (g_submitLoadingPopup != popup) return;

    auto state = g_submitRequestState;
    if (state) state->active.store(false);

    g_submitLoadingPopup = nullptr;
    g_submitLoadingHost = nullptr;
    g_submitRequestState.reset();

    // The delegate is called by the native popup's X callback. Explicitly close
    // the native popup here; globals are already cleared, so a possible second
    // delegate callback is harmless and will return immediately.
    if (popup) popup->closePopup();

    // Restore the parent popup's X so the user can continue working and submit again.
    if (state && state->restoreHostClose) {
        auto restore = std::move(state->restoreHostClose);
        restore();
    }
}

static void invalidateSubmitLoading(CCNode* host) {
    if (g_submitLoadingHost != host) return;

    if (g_submitRequestState) g_submitRequestState->active.store(false);

    auto* popup = g_submitLoadingPopup;
    g_submitLoadingPopup = nullptr;
    g_submitLoadingHost = nullptr;
    // Do not restore the parent's X here: this path runs while the parent is
    // leaving the scene and its controls may already be in destruction.
    // Also do NOT close UploadActionPopup synchronously from the parent's
    // onExit(): doing so re-enters FLAlertLayer destruction and can crash GD.
    g_submitRequestState.reset();

    if (popup) {
        // Keep the native popup alive until the parent has finished leaving the
        // scene, then close it on the main thread. The globals are already clear,
        // so the delegate cannot try to touch the destroyed parent popup.
        popup->retain();
        geode::queueInMainThread([popup]() {
            popup->closePopup();
            popup->release();
        });
    }
}

static void postRequestAction(
    RequestContext context,
    std::string action,
    std::string reason,
    SendSnapshot snapshot,
    CCNode* loadingHost,
    std::function<void()> restoreHostClose = {},
    std::function<void()> resetNoPingVisual = {},
    bool submittedNoPing = false,
    int submittedRequestID = 0
) {
    auto key = connectionKey();
    if (key.empty() || !context.active || context.request.requestID <= 0) return;

    auto state = showSubmitLoading(loadingHost, "Submitting...", std::move(restoreHostClose), std::move(resetNoPingVisual), submittedNoPing, submittedRequestID);
    if (!state) return;

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
    async::spawn(req.post(apiBase() + "/request-result"), [requestID, action, state](web::WebResponse res) {
        auto text = res.string().unwrapOr("");
        geode::queueInMainThread([requestID, action, state, res, text = std::move(text)]() mutable {
            if (!state->active.load()) return;

            if (res.ok()) {
                g_feedbackDrafts.erase(requestID);
                auto np = g_noPingDrafts.find(requestID);
                if (np != g_noPingDrafts.end() && np->second == state->submittedNoPing) {
                    g_noPingDrafts.erase(np);
                }
                if (state->resetNoPingVisual) state->resetNoPingVisual();

                if (g_submitRequestState != state || !g_submitLoadingPopup) return;
                auto* popup = g_submitLoadingPopup;
                if (action == "send") {
                    popup->showSuccessMessage(gd::string("Rating submitted!"));
                } else {
                    popup->showSuccessMessage(gd::string("Request rejected!"));
                }
            } else {
                if (g_submitRequestState != state) return;
                hideSubmitLoading();
                showAlert(MOD_NAME, "Could not submit the request result.\n\nHTTP " + std::to_string(res.code()) + "\n" +
                    (text.empty() ? "Empty response" : text));
            }
        });
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

    static ButtonSprite* makeBottomSprite(char const* label, bool ready = true) {
        // Keep the exact green GD action-button background in both states.
        // When no reason is selected Submit is only visually darkened, never changed
        // into the gray button style and never made non-clickable.
        auto* sprite = ButtonSprite::create(
            label,
            92,
            true,
            "goldFont.fnt",
            "GJ_button_01.png",
            30.f,
            1.26f
        );
        if (sprite && !ready) {
            sprite->setColor({160, 160, 160});
        }
        return sprite;
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
            bool ready = !m_reason.empty();
            if (auto* sprite = makeBottomSprite("Submit", ready)) {
                m_submitButton->setSprite(sprite);
            }
            // Submit stays clickable at all times. The dark appearance only communicates
            // that no reason is selected yet; onSubmit handles that case itself.
            m_submitButton->setEnabled(true);
            m_submitButton->setSizeMult(1.f);
        }

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

        // This popup is intentionally closed only with its Cancel button. Its
        // native frame X must never be visible; the temporary UploadActionPopup
        // has its own native X while a request is being submitted.
        if (m_closeBtn) m_closeBtn->setVisible(false);

        char const* title = context.mode == "helper" ? "Helper: Reject Reason" : "Mod: Reject Reason";
        // Keep the native GD popup-title style and fit the longer Helper title inside the frame.
        setTitle(title, "bigFont.fnt", context.mode == "helper" ? 0.92f : .94f, 25.f);

        reasonButton("NOT SENT", "not_sent", {112.f, 118.f});
        reasonButton("ALREADY SEEN", "already_seen", {288.f, 118.f});
        reasonButton("ALREADY RATED", "already_rated", {112.f, 78.f});
        reasonButton("REPORT", "report", {288.f, 78.f});

        auto* cancelSpr = makeBottomSprite("Cancel");
        auto* cancelBtn = CCMenuItemSpriteExtra::create(cancelSpr, this, menu_selector(RejectPopup::onCancel));
        cancelBtn->setPosition({138.f, 30.f});
        cancelBtn->setSizeMult(1.f);
        m_buttonMenu->addChild(cancelBtn);

        auto* submitSpr = makeBottomSprite("Submit");
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
        // CCMenuItemToggler updates its state during activate(). Read it only after
        // that activation has completed; this makes the stored value exactly match
        // the visible checkbox and keeps it when the popup is reopened.
        this->scheduleOnce(schedule_selector(RejectPopup::syncNoPingState), 0.f);
    }

    void onFeedback(CCObject*) { openFeedbackEditor(m_context); }
    void onCancel(CCObject*) { onClose(nullptr); }
    void onSubmit(CCObject*) {
        if (m_reason.empty()) {
            showAlert(MOD_NAME, "Choose a reject reason before submitting.");
            return;
        }
        // Read the toggler directly at submit time. This avoids a race where the user
        // enables NO PING and immediately presses Submit before the deferred sync runs.
        if (m_noPingToggle) setNoPingFor(m_context, m_noPingToggle->isToggled());
        auto context = m_context;
        auto reason = m_reason;
        // The Reject Reason popup never shows its own frame X. Closing the
        // native UploadActionPopup simply returns here and Submit can be used again.
        bool submittedNoPing = m_noPingToggle && m_noPingToggle->isToggled();
        int submittedRequestID = context.request.requestID;
        postRequestAction(context, "reject", reason, {}, this, {}, [this, submittedNoPing, submittedRequestID]() {
            // Only reset the checkbox if the user has not changed it while the request was loading.
            // Otherwise an old request finishing could overwrite the choice for the next request.
            if (m_context.request.requestID == submittedRequestID && m_noPingToggle &&
                m_noPingToggle->isToggled() == submittedNoPing) {
                m_noPingToggle->toggle(false);
            }
        }, submittedNoPing, submittedRequestID);
    }

    void onExit() override {
        invalidateSubmitLoading(this);
        Popup::onExit();
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

        // Keep Geometry Dash's native LevelCell rendering, but use a GDDL-style
        // outer pagination layer: the full request list stays in our mod, while GD only
        // receives a 50-ID chunk at a time. Native GD pages (10 levels each) remain
        // available inside that chunk, and the custom page controls jump between chunks.
        g_requestNativeBatch = 0;
        g_requestNativeSubPage = 0;
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

        // Load the Requests icon directly from this mod's extracted resources directory.
        // This deliberately avoids the sprite-frame cache: if a custom frame is not registered,
        // Geode displays its magenta/black missing-texture fallback instead of our PNG.
        auto iconPath = (Mod::get()->getResourcesDir() / "request-star.png").string();
        auto* sprite = CCSprite::create(iconPath.c_str());
        if (!sprite) {
            log::error("[REQUESTS UI] Could not load custom Requests icon from {}", iconPath);
            return;
        }
        // Do not use a fixed scale here. A PNG loaded directly from the mod resources has a
        // different logical content size when GD switches between High / Medium / Low texture
        // quality. That made the same .77 scale appear roughly twice as large on lower quality.
        // Instead, mirror the actual vanilla button geometry that is already on this menu.
        CCMenuItem* lowestButton = nullptr;
        CCMenuItem* secondLowestButton = nullptr;
        if (auto* children = menu->getChildren()) {
            for (unsigned int i = 0; i < children->count(); ++i) {
                auto* item = typeinfo_cast<CCMenuItem*>(children->objectAtIndex(i));
                if (!item || !item->isVisible()) continue;

                if (!lowestButton || item->getPositionY() < lowestButton->getPositionY()) {
                    secondLowestButton = lowestButton;
                    lowestButton = item;
                } else if (!secondLowestButton || item->getPositionY() < secondLowestButton->getPositionY()) {
                    secondLowestButton = item;
                }
            }
        }

        float targetSize = 48.f;
        CCSize referenceSize(targetSize, targetSize);
        if (lowestButton) {
            referenceSize = lowestButton->getContentSize();
            targetSize = std::max(referenceSize.width, referenceSize.height);
            if (targetSize < 1.f) {
                targetSize = 48.f;
                referenceSize = CCSize(targetSize, targetSize);
            }
        }

        auto sourceSize = sprite->getContentSize();
        auto sourceMax = std::max(sourceSize.width, sourceSize.height);
        if (sourceMax > 0.f) sprite->setScale(targetSize / sourceMax);

        auto* button = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(GDRequestsLevelSearchLayer::onRequests)
        );
        if (!button) return;
        button->setID("kolorbok.gd-send-logger/requests-button");

        // Give the custom item exactly the same layout / hitbox footprint as a vanilla button.
        // More importantly, do NOT call updateLayout after inserting it: the vanilla buttons
        // keep their original positions instead of being shifted by our extra item.
        button->setContentSize(referenceSize);
        sprite->setPosition({referenceSize.width / 2.f, referenceSize.height / 2.f});
        menu->addChild(button);

        if (lowestButton) {
            float spacing = targetSize + 6.f;
            if (secondLowestButton) {
                auto measured = std::abs(secondLowestButton->getPositionY() - lowestButton->getPositionY());
                if (measured > 1.f) spacing = measured;
            }
            button->setPosition({lowestButton->getPositionX(), lowestButton->getPositionY() - spacing});
        } else {
            button->setPosition({menu->getContentSize().width / 2.f, menu->getContentSize().height / 2.f});
        }
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

        // Server Requests owns the arrows completely: one arrow step is one
        // 50-level request page. The native 10-level pagination is intentionally bypassed.
        m_fields->nativeAtEnd = true;
        m_fields->nativeAtStart = true;

        if (m_rightArrow) m_rightArrow->setVisible(hasNextRequestNativeBatch());
        if (m_leftArrow) m_leftArrow->setVisible(hasPrevRequestNativeBatch());
    }

    void loadRequestNativeBatch(std::size_t batch) {
        auto count = requestNativeBatchCount();
        if (count == 0 || batch >= count) return;
        auto* search = makeRequestNativeBatchSearch(batch);
        if (!search) return;

        g_requestNativeBatch = batch;
        g_requestNativeSubPage = 0;
        m_fields->nativeAtEnd = false;
        m_fields->nativeAtStart = true;
        setSearchObject(search);
        loadPage(search);
    }

    void forceRequestPageSize() {
        if (!isThisRequestBrowser() || !m_list) return;

        auto count = m_levels ? static_cast<std::size_t>(m_levels->count()) : 0;
        auto visibleCount = std::min<std::size_t>(REQUEST_NATIVE_BATCH_SIZE, count);

        // Geometry Dash normally treats one LevelBrowser page as 10 items.
        // For Server Requests we deliberately widen that page to the whole
        // 50-ID request batch, so the list itself contains 50 levels and can
        // scroll through all of them without creating five native sub-pages.
        m_itemCount = static_cast<int>(visibleCount);
        m_pageStartIdx = 0;
        m_pageEndIdx = visibleCount == 0 ? -1 : static_cast<int>(visibleCount - 1);

        if (m_list->m_listView) {
            m_list->m_listView->reloadData();
        }
    }

    void refreshRequestPageLabels() {
        if (!isThisRequestBrowser()) return;

        auto ids = requestLevelIDs();
        auto total = ids.size();
        if (total == 0) return;

        auto first = g_requestNativeBatch * REQUEST_NATIVE_BATCH_SIZE + 1;
        auto last = std::min(first + REQUEST_NATIVE_BATCH_SIZE - 1, total);

        if (m_countText) {
            auto text = std::to_string(first) + " to " + std::to_string(last) +
                " of " + std::to_string(total);
            m_countText->setString(text.c_str());
        }

        // Server Requests uses only the left/right arrows for pagination.
        // Hide GD's numeric page selector so there is no extra page counter
        // beside the Server Requests title.
        if (m_pageBtn) m_pageBtn->setVisible(false);
        if (m_pageText) m_pageText->setVisible(false);
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
            g_requestNativeSubPage = 0;

            // LevelBrowserLayer::init() can create/load the first LevelCell objects
            // before the request-browser flag above is set. In that case our LevelCell
            // hook has no request context yet, so the + / video decorations are skipped
            // on the very first visit. Reload the same first page once after activation
            // so every already-created cell gets loadFromLevel() again with the request
            // context available. This is intentionally queued to the next main-thread
            // tick instead of calling loadPage() while the base init is still running.
            this->retain();
            geode::queueInMainThread([self = this, searchObj]() {
                if (self->getParent() && self->isThisRequestBrowser()) {
                    self->loadPage(searchObj);
                }
                self->release();
            });
        }
        return true;
    }

    void loadLevelsFinished(CCArray* levels, char const* key, int type) override {
        LevelBrowserLayer::loadLevelsFinished(levels, key, type);
        if (isThisRequestBrowser()) {
            forceRequestPageSize();
            refreshRequestBatchArrows();
            refreshRequestPageLabels();
        }
    }

    void onNextPage(CCObject* sender) {
        if (isThisRequestBrowser()) {
            if (hasNextRequestNativeBatch()) {
                loadRequestNativeBatch(g_requestNativeBatch + 1);
            }
            return;
        }
        LevelBrowserLayer::onNextPage(sender);
    }

    void onPrevPage(CCObject* sender) {
        if (isThisRequestBrowser()) {
            if (hasPrevRequestNativeBatch()) {
                loadRequestNativeBatch(g_requestNativeBatch - 1);
            }
            return;
        }
        LevelBrowserLayer::onPrevPage(sender);
    }

    void setIDPopupClosed(SetIDPopup* popup, int value) {
        if (isThisRequestBrowser()) {
            auto count = requestNativeBatchCount();
            if (count == 0 || value <= 0) return;
            auto batch = static_cast<std::size_t>(value - 1);
            if (batch >= count) batch = count - 1;
            loadRequestNativeBatch(batch);
            return;
        }
        LevelBrowserLayer::setIDPopupClosed(popup, value);
    }

    gd::string getSearchTitle() {
        if (isThisRequestBrowser()) {
            if (g_hasSelectedRequest && g_selectedRequest.requestID > 0) {
                auto title = "Request #" + std::to_string(g_selectedRequest.requestID);
                return gd::string(title.c_str());
            }
            auto title = std::string("Server Requests");
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
            g_requestNativeSubPage = 0;
            g_hasSelectedRequest = false;
            g_selectedRequest = RequestMeta{};
            g_context = RequestContext{};
        }
        LevelBrowserLayer::onBack(sender);
    }
};

class RequestInfoPopup final : public geode::Popup {
protected:
    RequestMeta m_meta;
    CCLabelTTF* m_measureLabel = nullptr;
    std::vector<std::string> m_descriptionLinks;

    static constexpr float POPUP_W = 330.f;
    static constexpr float POPUP_H = 220.f;
    static constexpr float SCROLL_X = 22.f;
    static constexpr float SCROLL_Y = 27.f;
    static constexpr float SCROLL_W = 286.f;
    static constexpr float SCROLL_H = 154.f;
    static constexpr float TEXT_W = SCROLL_W - 20.f;
    static constexpr float TTF_SIZE = 11.f;
    static constexpr float TTF_LINE_STEP = 15.f;
    static constexpr float INFO_LINE_STEP = 30.f;
    static constexpr float INFO_VALUE_OFFSET = 15.f;
    static constexpr float DESCRIPTION_GAP = 4.f;

    float measureUnicode(std::string const& value) {
        if (!m_measureLabel || value.empty()) return 0.f;
        m_measureLabel->setString(value.c_str());
        return m_measureLabel->getContentSize().width;
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

    std::vector<std::string> breakLongWord(std::string const& word) {
        std::vector<std::string> pieces;
        std::string current;
        for (std::size_t i = 0; i < word.size();) {
            auto next = nextUtf8Boundary(word, i);
            auto glyph = word.substr(i, next - i);
            auto candidate = current + glyph;
            if (!current.empty() && measureUnicode(candidate) > TEXT_W) {
                pieces.push_back(current);
                current = glyph;
            } else {
                current = candidate;
            }
            i = next;
        }
        if (!current.empty()) pieces.push_back(current);
        if (pieces.empty()) pieces.push_back("");
        return pieces;
    }

    struct DescriptionLinkRange {
        std::size_t start = 0;
        std::size_t end = 0;
        std::string url;
    };

    struct DescriptionDisplayLine {
        std::string text;
        std::vector<DescriptionLinkRange> links;
    };

    std::vector<DescriptionDisplayLine> wrapDescription(std::string text) {
        text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
        std::vector<DescriptionDisplayLine> lines;
        std::istringstream paragraphs(text);
        std::string paragraph;
        bool readAny = false;

        while (std::getline(paragraphs, paragraph, '\n')) {
            readAny = true;
            if (paragraph.empty()) {
                lines.push_back({"", {}});
                continue;
            }

            std::istringstream words(paragraph);
            std::string word;
            DescriptionDisplayLine current;

            auto pushCurrent = [&]() {
                if (!current.text.empty()) {
                    lines.push_back(std::move(current));
                    current = DescriptionDisplayLine{};
                }
            };

            while (words >> word) {
                auto candidate = current.text.empty() ? word : current.text + " " + word;
                auto links = findDescriptionLinks(word);
                bool isWholeURL = links.size() == 1 && links[0].start == 0 && links[0].end == word.size();

                if (measureUnicode(candidate) <= TEXT_W) {
                    auto offset = current.text.empty() ? 0u : current.text.size() + 1u;
                    if (!current.text.empty()) current.text += " ";
                    current.text += word;
                    if (isWholeURL) {
                        current.links.push_back({
                            offset,
                            offset + word.size(),
                            links[0].url
                        });
                    }
                    continue;
                }

                pushCurrent();

                if (measureUnicode(word) <= TEXT_W) {
                    current.text = word;
                    if (isWholeURL) {
                        current.links.push_back({0, word.size(), links[0].url});
                    }
                    continue;
                }

                auto pieces = breakLongWord(word);
                for (std::size_t i = 0; i < pieces.size(); ++i) {
                    if (i + 1 < pieces.size()) {
                        DescriptionDisplayLine line;
                        line.text = pieces[i];
                        if (isWholeURL) {
                            line.links.push_back({0, pieces[i].size(), links[0].url});
                        }
                        lines.push_back(std::move(line));
                    } else {
                        current.text = pieces[i];
                        if (isWholeURL) {
                            current.links.push_back({0, pieces[i].size(), links[0].url});
                        }
                    }
                }
            }
            pushCurrent();
        }

        if (!readAny || lines.empty()) lines.push_back({"", {}});
        return lines;
    }

    static std::vector<DescriptionLinkRange> findDescriptionLinks(std::string const& line) {
        std::vector<DescriptionLinkRange> links;
        std::size_t searchFrom = 0;

        while (searchFrom < line.size()) {
            auto http = line.find("http://", searchFrom);
            auto https = line.find("https://", searchFrom);
            std::size_t start = std::string::npos;
            if (http != std::string::npos && https != std::string::npos) start = std::min(http, https);
            else if (http != std::string::npos) start = http;
            else start = https;
            if (start == std::string::npos) break;

            auto end = line.find_first_of(" \t\r\n", start);
            if (end == std::string::npos) end = line.size();

            auto url = line.substr(start, end - start);
            while (!url.empty() && std::string(".,!?;:)]}>'\"").find(url.back()) != std::string::npos) {
                url.pop_back();
                --end;
            }

            if (end > start && isValidWebURL(url)) {
                links.push_back({start, end, url});
            }
            searchFrom = std::max(end, start + 1);
        }
        return links;
    }

    void onDescriptionLink(CCObject* sender) {
        auto* item = static_cast<CCMenuItemSpriteExtra*>(sender);
        if (!item) return;
        auto index = item->getTag();
        if (index < 0 || static_cast<std::size_t>(index) >= m_descriptionLinks.size()) return;
        geode::utils::web::openLinkInBrowser(m_descriptionLinks[index]);
    }

    // Same layout logic as DESCRIPTION: yellow GD heading first, then the
    // readable Arial value on its own line underneath.
    static void addInfoLine(CCNode* parent, std::string const& name, std::string const& value, float y) {
        if (!parent) return;

        auto* nameLabel = CCLabelBMFont::create(name.c_str(), "goldFont.fnt");
        if (nameLabel) {
            nameLabel->setScale(.38f);
            nameLabel->setAnchorPoint({0.f, 1.f});
            nameLabel->setPosition({10.f, y});
            parent->addChild(nameLabel, 2);
        }

        auto* valueLabel = CCLabelTTF::create(value.c_str(), "Arial", TTF_SIZE);
        if (!valueLabel) return;
        valueLabel->setAnchorPoint({0.f, 1.f});
        valueLabel->setColor(ccc3(255, 255, 255));
        valueLabel->setPosition({10.f, y - 15.f});
        parent->addChild(valueLabel, 2);
    }

    // Keep request metadata rows on the same two-line heading/value layout.
    // The vertical spacing is calculated only from fields that actually exist,
    // so missing REVIEW/FEEDBACK fields never reserve empty space.
    bool initFor(RequestMeta const& meta) {
        m_meta = meta;
        if (!Popup::init(POPUP_W, POPUP_H)) return false;
        auto title = "REQUEST #" + std::to_string(meta.requestID);
        setTitle(title.c_str(), "goldFont.fnt", .62f, 20.f);

        auto* panel = CCLayerColor::create(ccc4(78, 42, 25, 205), SCROLL_W, SCROLL_H);
        if (panel) {
            panel->setPosition({SCROLL_X, SCROLL_Y});
            m_mainLayer->addChild(panel, 1);
        }

        m_measureLabel = CCLabelTTF::create("", "Arial", TTF_SIZE);
        if (!m_measureLabel) return false;
        m_measureLabel->setVisible(false);
        m_mainLayer->addChild(m_measureLabel, 0);

        std::vector<std::pair<std::string, std::string>> infoLines;

        if (requestReviewEnabled(meta)) {
            infoLines.emplace_back("REVIEW", requestWantsReview(meta) ? "Yes" : "No");
        }
        if (requestFeedbackEnabled(meta)) {
            infoLines.emplace_back("FEEDBACK", requestWantsFeedback(meta) ? "Yes" : "No");
        }
        // LANGUAGE is part of the request metadata and must be shown even when
        // the server does not have Review/Feedback fields enabled.
        auto language = requestLanguageLabel(meta.reviewLanguage);
        if (!language.empty() && language != "Not specified") {
            infoLines.emplace_back("LANGUAGE", language);
        }

        auto description = hasRequestDescription(meta.description) ? trim(meta.description) : std::string();
        auto descriptionLines = description.empty() ? std::vector<DescriptionDisplayLine>{} : wrapDescription(description);

        float required = 13.f;
        required += static_cast<float>(infoLines.size()) * INFO_LINE_STEP;
        if (!descriptionLines.empty()) {
            if (!infoLines.empty()) required += DESCRIPTION_GAP;
            required += 16.f;
            required += static_cast<float>(descriptionLines.size()) * TTF_LINE_STEP;
        }
        if (infoLines.empty() && descriptionLines.empty()) required += 24.f;
        required += 10.f;
        float contentH = std::max(SCROLL_H, required);

        auto* scroll = geode::ScrollLayer::create(CCSize(SCROLL_W, SCROLL_H), true, true);
        if (!scroll) return false;
        scroll->setID("kolorbok.gd-send-logger/request-info-scroll");
        scroll->setPosition({SCROLL_X, SCROLL_Y});
        scroll->setStealingTouches(false);
        scroll->m_contentLayer->setContentSize({SCROLL_W, contentH});
        m_mainLayer->addChild(scroll, 2);

        // Use the exact same difficulty tile visual as the difficulty filter,
        // including the star count. It belongs to the scroll content, so it moves
        // together with the rest of the request information.
        if (!meta.difficultyKey.empty() || meta.difficulty > 0) {
            auto key = meta.difficultyKey.empty()
                ? normalizeRequestDifficultyKey("", meta.difficulty)
                : meta.difficultyKey;
            auto* difficultyTile = makeDifficultyTile(key, true, meta.hasPlatformer && meta.platformer);
            if (difficultyTile) {
                difficultyTile->setScale(.70f);
                difficultyTile->setAnchorPoint({1.f, 1.f});
                difficultyTile->setPosition({SCROLL_W - 7.f, contentH - 11.f});
                difficultyTile->setID("kolorbok.gd-send-logger/request-info-difficulty");
                scroll->m_contentLayer->addChild(difficultyTile, 3);
            }
        }

        // Metadata rows are laid out from the rows that are actually present.
        // Do not reserve a row for a disabled/missing REVIEW or FEEDBACK field.
        float y = contentH - 10.f;
        for (std::size_t i = 0; i < infoLines.size(); ++i) {
            addInfoLine(scroll->m_contentLayer, infoLines[i].first, infoLines[i].second, y);
            y -= INFO_LINE_STEP;
        }

        if (!descriptionLines.empty()) {
            // After the loop, y is already one full row below the last heading.
            // The last value is at (y + INFO_LINE_STEP - INFO_VALUE_OFFSET).
            // Place DESCRIPTION below that value, leaving only the requested gap.
            if (!infoLines.empty()) {
                y -= DESCRIPTION_GAP;
            }
            auto* heading = CCLabelBMFont::create("DESCRIPTION", "goldFont.fnt");
            if (heading) {
                heading->setScale(.38f);
                heading->setAnchorPoint({0.f, 1.f});
                heading->setPosition({10.f, y});
                scroll->m_contentLayer->addChild(heading, 2);
            }
            y -= 16.f;

            auto* linkMenu = CCMenu::create();
            if (linkMenu) {
                linkMenu->setPosition({0.f, 0.f});
                scroll->m_contentLayer->addChild(linkMenu, 4);
            }

            for (auto const& displayLine : descriptionLines) {
                auto const& line = displayLine.text;
                auto const& links = displayLine.links;
                float x = 10.f;
                std::size_t cursor = 0;

                auto addTextPart = [&](std::string const& part, bool link, std::string const& url) {
                    if (part.empty()) return;
                    auto* text = CCLabelTTF::create(part.c_str(), "Arial", TTF_SIZE);
                    if (!text) return;
                    text->setAnchorPoint({0.f, 1.f});
                    text->setColor(link ? ccc3(85, 190, 255) : ccc3(255, 255, 255));

                    auto width = text->getContentSize().width;
                    if (link && linkMenu) {
                        text->setAnchorPoint({.5f, .5f});
                        auto* button = CCMenuItemSpriteExtra::create(
                            text, this, menu_selector(RequestInfoPopup::onDescriptionLink)
                        );
                        if (button) {
                            // Description links are plain clickable text: disable the
                            // CCMenuItemSpriteExtra hover/select animation completely.
                            button->m_animationEnabled = false;
                            button->setSizeMult(1.f);
                            button->setTag(static_cast<int>(m_descriptionLinks.size()));
                            m_descriptionLinks.push_back(url);
                            button->setPosition({x + width * .5f, y - TTF_SIZE * .5f});
                            linkMenu->addChild(button);
                        }
                    } else {
                        text->setPosition({x, y});
                        scroll->m_contentLayer->addChild(text, 2);
                    }
                    x += width;
                };

                for (auto const& link : links) {
                    if (link.start > cursor) {
                        addTextPart(line.substr(cursor, link.start - cursor), false, {});
                    }
                    addTextPart(line.substr(link.start, link.end - link.start), true, link.url);
                    cursor = link.end;
                }
                if (cursor < line.size()) {
                    addTextPart(line.substr(cursor), false, {});
                }
                if (line.empty()) {
                    auto* empty = CCLabelTTF::create(" ", "Arial", TTF_SIZE);
                    if (empty) {
                        empty->setAnchorPoint({0.f, 1.f});
                        empty->setPosition({10.f, y});
                        scroll->m_contentLayer->addChild(empty, 2);
                    }
                }
                y -= TTF_LINE_STEP;
            }
        } else if (infoLines.empty()) {
            auto* empty = CCLabelTTF::create("No additional request info.", "Arial", TTF_SIZE);
            if (empty) {
                empty->setAnchorPoint({0.f, 1.f});
                empty->setColor(ccc3(255, 255, 255));
                empty->setPosition({10.f, y});
                scroll->m_contentLayer->addChild(empty, 2);
            }
        }

        scroll->scrollToTop();

        if (contentH > SCROLL_H + 1.f) {
            auto* hint = CCLabelBMFont::create("SCROLL", "goldFont.fnt");
            if (hint) {
                hint->setScale(.24f);
                hint->setOpacity(155);
                hint->setAnchorPoint({1.f, .5f});
                hint->setPosition({POPUP_W - 24.f, 18.f});
                m_mainLayer->addChild(hint, 3);
            }
        }
        return true;
    }

public:
    static RequestInfoPopup* create(RequestMeta const& meta) {
        auto* ret = new RequestInfoPopup();
        if (ret && ret->initFor(meta)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class $modify(GDRequestsLevelCell, LevelCell) {
    struct Fields {
        RequestMeta request;
        bool hasRequest = false;
    };

    void clearRequestDecorations() {
        if (m_mainMenu) {
            for (auto const* id : {
                "kolorbok.gd-send-logger/request-info-button",
                "kolorbok.gd-send-logger/request-video-button",
                "kolorbok.gd-send-logger/request-youtube-button",
                "kolorbok.gd-send-logger/request-copy-link-button"
            }) {
                if (auto* node = m_mainMenu->getChildByID(id)) node->removeFromParentAndCleanup(true);
            }
        }
        if (auto* node = this->getChildByID("kolorbok.gd-send-logger/request-id-label")) {
            node->removeFromParentAndCleanup(true);
        }
        m_fields->request = RequestMeta{};
        m_fields->hasRequest = false;
    }

    static CCNode* requestIconOrFallback(
        char const* frameName,
        char const* fallbackText,
        float targetHeight
    ) {
        if (auto* sprite = CCSprite::createWithSpriteFrameName(frameName)) {
            auto size = sprite->getContentSize();
            if (size.height > 0.f) sprite->setScale(targetHeight / size.height);
            return sprite;
        }
        auto* fallback = ButtonSprite::create(
            fallbackText, 34, true, "bigFont.fnt", "GJ_button_01.png", 28.f, .76f
        );
        auto size = fallback->getContentSize();
        if (size.height > 0.f) fallback->setScale(targetHeight / size.height);
        return fallback;
    }

    static float displayedWidth(CCNode* node) {
        if (!node) return 0.f;
        return node->getContentSize().width * std::fabs(node->getScaleX());
    }

    static float displayedHeight(CCNode* node) {
        if (!node) return 0.f;
        return node->getContentSize().height * std::fabs(node->getScaleY());
    }

    void addRequestDecorations(RequestMeta const& meta) {
        auto size = this->getContentSize();
        float width = size.width > 0.f ? size.width : 356.f;

        auto* label = CCLabelBMFont::create(
            ("REQ #" + std::to_string(meta.requestID)).c_str(),
            "goldFont.fnt"
        );
        if (label) {
            label->setID("kolorbok.gd-send-logger/request-id-label");
            label->setScale(.28f);
            label->setOpacity(205);
            label->setAnchorPoint({1.f, .5f});
            label->setPosition({width - 6.f, 8.f});
            this->addChild(label, 30);
        }

        if (!m_mainMenu) return;
        NodeIDs::provideFor(this);
        auto* viewButton = typeinfo_cast<CCMenuItemSpriteExtra*>(m_mainMenu->getChildByID("view-button"));

        // Use the real vanilla VIEW button as the anchor instead of hard-coded cell offsets.
        // This keeps request controls aligned when GD / NodeIDs / texture packs resize VIEW.
        float y = viewButton ? viewButton->getPositionY() : 45.f;
        float viewX = viewButton ? viewButton->getPositionX() : width - 40.f;
        CCNode* viewVisual = viewButton ? viewButton->getNormalImage() : nullptr;
        float viewScaleX = viewButton ? std::fabs(viewButton->getScaleX()) : 1.f;
        float viewScaleY = viewButton ? std::fabs(viewButton->getScaleY()) : 1.f;
        float viewW = viewVisual ? displayedWidth(viewVisual) * viewScaleX : (viewButton ? displayedWidth(viewButton) : 72.f);
        float viewH = viewVisual ? displayedHeight(viewVisual) * viewScaleY : (viewButton ? displayedHeight(viewButton) : 38.f);
        if (viewH < 20.f || viewH > 60.f) viewH = 38.f;
        if (viewW < 35.f || viewW > 120.f) viewW = 72.f;

        // Request buttons use the same visual height as VIEW and a shared tiny gap.
        float buttonSize = viewH;
        float gap = 2.f;
        float viewLeft = viewX - viewW * .5f;
        bool hasVideo = hasRequestVideo(meta.videoURL);

        // Layout requested for Server Requests:
        //   with video:  [+] [YouTube] [VIEW]
        //   no video:                [+] [VIEW]
        // Keep the control nearest VIEW anchored to VIEW itself, so texture packs / GET IT
        // variants keep the whole group aligned.
        float nearestX = viewLeft - gap - buttonSize * .5f;
        float youtubeX = nearestX;
        float infoX = hasVideo ? (youtubeX - buttonSize - gap) : nearestX;

        // Use the vanilla green info icon shown by Geometry Dash. Keep the
        // visual icon smaller while preserving the normal button hit area.
        // Texture quality can change the source frame's logical dimensions. Cap the
        // displayed info icon to the normal GD button height so low-quality textures
        // cannot make the icon grow disproportionately.
        float infoTargetHeight = buttonSize * .74f;
        auto* infoSprite = requestIconOrFallback("GJ_infoIcon_001.png", "i", infoTargetHeight);
        auto* infoButton = CCMenuItemSpriteExtra::create(
            infoSprite, this, menu_selector(GDRequestsLevelCell::onRequestInfo)
        );
        if (infoButton) {
            infoButton->setID("kolorbok.gd-send-logger/request-info-button");
            infoButton->setSizeMult(1.f);
            infoButton->setPosition({infoX, y});
            m_mainMenu->addChild(infoButton);
        }

        if (hasVideo) {
            bool youtube = isYouTubeURL(meta.videoURL);
            auto* videoSprite = youtube
                ? requestIconOrFallback("gj_ytIcon_001.png", "YT", buttonSize)
                : requestIconOrFallback("GJ_copyBtn_001.png", "COPY", buttonSize);
            auto* youtubeButton = CCMenuItemSpriteExtra::create(
                videoSprite, this, menu_selector(GDRequestsLevelCell::onRequestVideo)
            );
            if (youtubeButton) {
                youtubeButton->setID(youtube
                    ? "kolorbok.gd-send-logger/request-youtube-button"
                    : "kolorbok.gd-send-logger/request-copy-link-button");
                youtubeButton->setSizeMult(1.f);
                youtubeButton->setPosition({youtubeX, y});
                m_mainMenu->addChild(youtubeButton);
            }
        }
    }

    void loadFromLevel(GJGameLevel* level) {
        LevelCell::loadFromLevel(level);
        NodeIDs::provideFor(this);
        clearRequestDecorations();
        if (!g_requestBrowserActive || !level) return;

        auto it = g_requestByLevel.find(level->m_levelID);
        if (it == g_requestByLevel.end()) return;

        m_fields->request = it->second;
        m_fields->hasRequest = true;
        addRequestDecorations(m_fields->request);
    }

    void onRequestInfo(CCObject*) {
        if (!m_fields->hasRequest) return;
        if (auto* popup = RequestInfoPopup::create(m_fields->request)) popup->show();
    }

    void onRequestVideo(CCObject*) {
        if (!m_fields->hasRequest || !hasRequestVideo(m_fields->request.videoURL)) return;
        auto url = trim(m_fields->request.videoURL);
        if (isYouTubeURL(url)) {
            geode::utils::web::openLinkInBrowser(url);
        } else {
            if (copyTextToClipboard(url)) {
                showAlert(MOD_NAME, "The link was copied to your clipboard. Non-YouTube links are not opened directly for safety.");
            } else {
                showAlert(MOD_NAME, fmt::format("For safety, this non-YouTube link was not opened automatically:<br>{}", url));
            }
        }
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

        // Native GD cyan cancel button: orange X on a blue/cyan circular background.
        auto* rejectSprite = CCSprite::createWithSpriteFrameName("GJ_cancelDownloadBtn_001.png");
        rejectSprite->setScale(1.25f);
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
            auto replacement = m_level->isPlatformer() ? "Helper: Suggest Moons" : "Helper: Suggest Stars";
            if (!replaceFirstLabelContaining(popup, "MOD: SUGGEST", replacement)) {
                replaceFirstLabelContaining(popup, "SUGGEST", replacement);
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
        bool platformerPopup = false;
        RequestContext requestContext;
        CCMenuItemSpriteExtra* feedbackButton = nullptr;
        CCMenuItemToggler* noPingToggle = nullptr;
        CCLabelBMFont* noPingLabel = nullptr;
        bool pendingModeratorNoPing = false;
        bool hasPendingModeratorNoPing = false;
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
        auto replacement = m_fields->platformerPopup
            ? "Helper: Suggest Moons"
            : "Helper: Suggest Stars";
        if (!replaceFirstLabelContaining(this, "MOD: SUGGEST", replacement)) {
            replaceFirstLabelContaining(this, "SUGGEST", replacement);
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
        } else if (moderator && g_context.active && g_context.request.levelID == levelID &&
            (g_context.mode == "helper" || g_context.mode == "moderator")) {
            // Only the actual moderator-send RateStarsLayer may inherit request context.
            // The ordinary player "Rate Stars" popup also uses RateStarsLayer, but is
            // created with moderator=false and must remain completely unrelated to Discord.
            captured = g_context;
        }

        if (!RateStarsLayer::init(levelID, platformer, moderator)) return false;
        m_fields->platformerPopup = platformer;
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
        if (!m_fields->requestContext.active || !m_fields->noPingToggle) return;
        // The toggler changes its state as part of activate(), so store the final
        // state on the next tick instead of sampling the pre-click state.
        this->scheduleOnce(schedule_selector(GDRequestsRateStarsLayer::syncRequestNoPingState), 0.f);
    }

    void onExit() override {
        invalidateSubmitLoading(this);
        RateStarsLayer::onExit();
    }

    void onRate(CCObject* sender) {
        if (m_fields->helperRequestPopup) {
            auto snapshot = captureSend(this);
            if (snapshot.levelID <= 0 || snapshot.stars <= 0 || snapshot.stars > 10) {
                showAlert(MOD_NAME, "Choose a difficulty before submitting the helper result.");
                return;
            }
            auto context = m_fields->requestContext;
            // Read the current checkbox state immediately before building the request.
            // The click callback can be deferred, so relying only on onRequestNoPing()
            // causes a fast NO PING -> Submit sequence to send noPing=false.
            if (m_fields->noPingToggle) {
                setNoPingFor(context, m_fields->noPingToggle->isToggled());
            }
            bool submittedNoPing = m_fields->noPingToggle && m_fields->noPingToggle->isToggled();
            int submittedRequestID = context.request.requestID;
            postRequestAction(context, "send", "", snapshot, this, {}, [this, submittedNoPing, submittedRequestID]() {
                // Do not let completion of an older submission reset a checkbox that the user
                // already changed for the next submission.
                if (m_fields->requestContext.request.requestID == submittedRequestID &&
                    m_fields->noPingToggle && m_fields->noPingToggle->isToggled() == submittedNoPing) {
                    m_fields->noPingToggle->toggle(false);
                }
            }, submittedNoPing, submittedRequestID);
            return;
        }

        // FakeGDMod compatibility is only for genuine moderator sends. Never let the
        // ordinary player star-rating popup reach the Discord send bridge.
        bool fakeSend = m_moderator && !m_fields->helperRequestPopup && fakeGDModWillSimulateSend(this);
        SendSnapshot fakeSnapshot;
        RequestContext captured = m_fields->requestContext;
        if (fakeSend) fakeSnapshot = captureSend(this);

        // For the real RobTop moderator upload, freeze NO PING at the exact moment
        // the user presses Submit. uploadActionFinished() can run later, after the
        // checkbox has already been changed for another request.
        if (m_moderator && !m_fields->helperRequestPopup && !fakeSend) {
            m_fields->pendingModeratorNoPing = m_fields->noPingToggle && m_fields->noPingToggle->isToggled();
            m_fields->hasPendingModeratorNoPing = true;
        }

        RateStarsLayer::onRate(sender);

        if (fakeSend) {
            reportSend(fakeSnapshot, false, captured.active && captured.mode == "moderator" ? &captured : nullptr);
        }
    }

    void uploadActionFinished(int id, int response) override {
        bool wasModerator = m_moderator;
        auto snapshot = captureSend(this);
        auto captured = m_fields->requestContext;
        bool submittedNoPing = m_fields->hasPendingModeratorNoPing
            ? m_fields->pendingModeratorNoPing
            : (m_fields->noPingToggle && m_fields->noPingToggle->isToggled());
        bool hasSubmittedNoPing = m_fields->hasPendingModeratorNoPing;
        m_fields->hasPendingModeratorNoPing = false;

        if (debugLogging()) {
            log::info("RateStarsLayer::uploadActionFinished id={}, response={}, moderator={}", id, response, wasModerator);
        }

        if (wasModerator && !m_fields->helperRequestPopup) {
            // Use only the immutable NO PING snapshot captured at the exact Submit click.
            // Never read or modify the checkbox here: the user may already be preparing
            // another request while Geometry Dash finishes the previous upload.
            auto* noPingOverride = hasSubmittedNoPing ? &submittedNoPing : nullptr;
            reportSend(snapshot, false, captured.active && captured.mode == "moderator" ? &captured : nullptr, noPingOverride);
        }
        RateStarsLayer::uploadActionFinished(id, response);
    }

    void uploadActionFailed(int id, int response) override {
        if (m_moderator && !m_fields->helperRequestPopup) {
            m_fields->hasPendingModeratorNoPing = false;
        }
        if (m_moderator && debugLogging()) {
            log::warn("Moderator send failed in GD: id={}, response={}, levelID={}, stars={}, featureState={}",
                id, response, m_levelID, m_starsRate, m_featureState);
        }
        RateStarsLayer::uploadActionFailed(id, response);
    }
};
