#include <Geode/Geode.hpp>
#include <Geode/modify/RateStarsLayer.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/loader/Loader.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include "api_url.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

using namespace geode::prelude;

namespace {

struct SendSnapshot {
    int levelID = 0;
    int stars = 0;
    int featureState = 0;
    bool hasPlatformer = false;
    bool platformer = false;
    std::string levelName;
    std::string creator;
};

static std::unordered_map<std::string, std::chrono::steady_clock::time_point> g_recentSends;

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

static bool debugLogging() {
    return Mod::get()->getSettingValue<bool>("debug-logging");
}

static std::string limitPopupText(std::string value, std::size_t limit = 700) {
    if (value.size() <= limit) {
        return value;
    }
    value.resize(limit);
    value += "...";
    return value;
}

static void showTestPopup(std::string const& message) {
    FLAlertLayer::create(
        "GD Send Logger",
        gd::string(limitPopupText(message).c_str()),
        "OK"
    )->show();
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

static bool isRecentDuplicate(SendSnapshot const& snapshot) {
    auto now = std::chrono::steady_clock::now();
    std::string key = std::to_string(snapshot.levelID) + ":" + std::to_string(snapshot.stars) + ":" +
        std::to_string(snapshot.featureState);

    for (auto it = g_recentSends.begin(); it != g_recentSends.end();) {
        if (now - it->second > std::chrono::seconds(15)) {
            it = g_recentSends.erase(it);
        } else {
            ++it;
        }
    }

    auto found = g_recentSends.find(key);
    if (found != g_recentSends.end() && now - found->second < std::chrono::seconds(5)) {
        return true;
    }
    g_recentSends[key] = now;
    return false;
}

static SendSnapshot captureSend(RateStarsLayer* layer) {
    SendSnapshot snapshot;
    if (!layer) {
        return snapshot;
    }

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

// FakeGDMod v1.0.x currently identifies its fake moderator send path by looking
// at the first child layer of RateStarsLayer and intercepting onRate when that
// child has exactly three children. Mirror that predicate safely so we only
// report when FakeGDMod itself is going to simulate "Rating submitted!".
static bool fakeGDModWillSimulateSend(RateStarsLayer* layer) {
    if (!layer || !fakeGDModLoaded()) {
        return false;
    }

    auto* children = layer->getChildren();
    if (!children || children->count() < 1) {
        return false;
    }

    auto* firstChild = children->objectAtIndex(0);
    auto* innerLayer = dynamic_cast<cocos2d::CCLayer*>(firstChild);
    return innerLayer && innerLayer->getChildrenCount() == 3;
}

static matjson::Value buildPayload(SendSnapshot const& snapshot, bool isTest) {
    auto body = matjson::Value();
    body["eventId"] = makeEventID(snapshot, isTest);
    body["levelId"] = snapshot.levelID;
    body["stars"] = snapshot.stars;
    body["featureState"] = snapshot.featureState;
    body["sendType"] = featureStateToSendType(snapshot.featureState);

    if (!snapshot.levelName.empty()) {
        body["levelName"] = snapshot.levelName;
    }
    if (!snapshot.creator.empty()) {
        body["creator"] = snapshot.creator;
    }
    if (snapshot.hasPlatformer) {
        body["platformer"] = snapshot.platformer;
    }
    return body;
}

static void reportSend(SendSnapshot snapshot, bool isTest = false) {
    // A manual test is allowed even when normal detection is disabled so the bridge can
    // be checked without risking an accidental real moderator-send report.
    if (!isTest && !Mod::get()->getSettingValue<bool>("enabled")) {
        return;
    }
    if (snapshot.levelID <= 0 || snapshot.stars <= 0 || snapshot.stars > 10) {
        if (debugLogging() || isTest) {
            log::warn(
                "Ignoring {}send with invalid values: levelID={}, stars={}, featureState={}",
                isTest ? "test " : "",
                snapshot.levelID,
                snapshot.stars,
                snapshot.featureState
            );
        }
        return;
    }
    if (!isTest && isRecentDuplicate(snapshot)) {
        if (debugLogging()) {
            log::info("Duplicate moderator send suppressed for level {}", snapshot.levelID);
        }
        return;
    }

    auto connectionKey = trim(Mod::get()->getSettingValue<std::string>("connection-key"));
    if (connectionKey.empty()) {
        log::warn("GD Send Logger is not configured. Fill Connection Key in mod settings.");
        if (isTest) {
            showTestPopup("Test was not sent: Connection Key is empty.");
        }
        return;
    }

    auto body = buildPayload(snapshot, isTest);
    auto req = web::WebRequest();
    req.header("Content-Type", "application/json");
    req.header("Authorization", "Bearer " + connectionKey);
    req.bodyJSON(body);
    req.timeout(std::chrono::seconds(15));

    if (debugLogging() || isTest) {
        log::info(
            "{}send: levelID={}, name='{}', creator='{}', stars={}, featureState={}, sendType={}, platformer={}",
            isTest ? "Test " : "Moderator ",
            snapshot.levelID,
            snapshot.levelName,
            snapshot.creator,
            snapshot.stars,
            snapshot.featureState,
            featureStateToSendType(snapshot.featureState),
            snapshot.hasPlatformer ? (snapshot.platformer ? "true" : "false") : "unknown"
        );
    }

    async::spawn(
        req.post(SEND_API_URL),
        [snapshot, isTest](web::WebResponse res) {
            auto responseText = res.string().unwrapOr("");
            if (res.ok()) {
                if (debugLogging() || isTest) {
                    log::info(
                        "Bot accepted {}send for level {}: {}",
                        isTest ? "test " : "",
                        snapshot.levelID,
                        responseText
                    );
                }
                if (isTest) {
                    bool published =
                        responseText.find("\"published\": true") != std::string::npos ||
                        responseText.find("\"published\":true") != std::string::npos;
                    if (published) {
                        showTestPopup("Success: the cloud bot published the test send to Discord.");
                    } else {
                        showTestPopup(
                            "Server accepted the request, but it was not published.\n\nHTTP " +
                            std::to_string(res.code()) + "\n" +
                            (responseText.empty() ? std::string("Empty response") : responseText)
                        );
                    }
                }
            } else {
                log::warn(
                    "Bot bridge rejected {}send for level {} (HTTP {}): {}",
                    isTest ? "test " : "",
                    snapshot.levelID,
                    res.code(),
                    responseText.empty() ? std::string("empty response") : responseText
                );
                if (isTest) {
                    std::string details = responseText.empty()
                        ? std::string("No response body. Check the embedded API URL, HTTPS, Caddy, and internet connection.")
                        : responseText;
                    showTestPopup(
                        "Test failed.\n\nHTTP " + std::to_string(res.code()) + "\n" + details
                    );
                }
            }
        }
    );
}

static void sendTestRequest() {
    SendSnapshot snapshot;
    // Deliberately use a non-GD test ID and provide all metadata locally. This prevents
    // the bot from needing a GD lookup and makes an "Only outside bot" collision extremely unlikely.
    snapshot.levelID = 2147483001;
    snapshot.stars = 6;
    snapshot.featureState = 1;
    snapshot.hasPlatformer = true;
    snapshot.platformer = false;
    snapshot.levelName = "GD Send Logger Test";
    snapshot.creator = "Local Test";
    reportSend(snapshot, true);
}

} // namespace

$execute {
    listenForSettingChanges<bool>("test-send", [](bool value) {
        if (!value) {
            return;
        }
        sendTestRequest();
        // Make this act like a one-shot button: after the ON change triggers the test,
        // reset it so the next tap can send another independent test request.
        Mod::get()->setSettingValue<bool>("test-send", false);
    });
}

class $modify(GDSendLoggerRateStarsLayer, RateStarsLayer) {
    static void onModify(auto& self) {
        if (!self.getHook("RateStarsLayer::onRate")) {
            log::error("GD Send Logger: failed to register RateStarsLayer::onRate hook");
        }
        if (!self.getHook("RateStarsLayer::uploadActionFinished")) {
            log::error("GD Send Logger: failed to register RateStarsLayer::uploadActionFinished hook");
        }
        if (!self.getHook("RateStarsLayer::uploadActionFailed")) {
            log::error("GD Send Logger: failed to register RateStarsLayer::uploadActionFailed hook");
        }

        // FakeGDMod's fake-send branch does not call the original onRate at all.
        // Therefore our hook must be earlier in the chain or FakeGDMod would swallow
        // the call before GD Send Logger ever sees it. Geode's relative hook priority
        // gives deterministic ordering when FakeGDMod is installed.
        if (Loader::get()->isModInstalled("bitz.fakegdmod")) {
            auto priorityResult = self.setHookPriorityBeforePre(
                "RateStarsLayer::onRate",
                "bitz.fakegdmod"
            );
            if (!priorityResult) {
                log::error(
                    "GD Send Logger: FakeGDMod detected, but failed to order RateStarsLayer::onRate before it"
                );
            } else {
                log::info("GD Send Logger: FakeGDMod compatibility armed");
            }
        }
    }

    void onRate(CCObject* sender) {
        // Capture BEFORE calling the next hook. FakeGDMod may consume the call and
        // never invoke Geometry Dash's original onRate.
        bool fakeSend = fakeGDModWillSimulateSend(this);
        SendSnapshot fakeSnapshot;
        if (fakeSend) {
            fakeSnapshot = captureSend(this);
            if (debugLogging()) {
                log::info(
                    "FakeGDMod send button captured: levelID={}, stars={}, featureState={}",
                    fakeSnapshot.levelID,
                    fakeSnapshot.stars,
                    fakeSnapshot.featureState
                );
            }
        }

        // Continue the hook chain. With current FakeGDMod this displays its fake
        // upload popup / "Rating submitted!" message instead of contacting GD.
        RateStarsLayer::onRate(sender);

        // Only publish when the exact FakeGDMod fake-send path was active. If its
        // onRate passed through to real GD, uploadActionFinished remains the source
        // of truth and this branch does nothing.
        if (fakeSend) {
            if (debugLogging()) {
                log::info("FakeGDMod simulated send completed; forwarding snapshot to bot bridge");
            }
            reportSend(fakeSnapshot, false);
        }
    }

    void uploadActionFinished(int id, int response) override {
        bool wasModerator = m_moderator;
        auto snapshot = captureSend(this);

        if (debugLogging()) {
            log::info(
                "RateStarsLayer::uploadActionFinished id={}, response={}, moderator={}",
                id,
                response,
                wasModerator
            );
        }

        if (wasModerator) {
            reportSend(snapshot, false);
        }

        RateStarsLayer::uploadActionFinished(id, response);
    }

    void uploadActionFailed(int id, int response) override {
        if (m_moderator && debugLogging()) {
            log::warn(
                "Moderator send failed in GD: id={}, response={}, levelID={}, stars={}, featureState={}",
                id,
                response,
                m_levelID,
                m_starsRate,
                m_featureState
            );
        }
        RateStarsLayer::uploadActionFailed(id, response);
    }
};
