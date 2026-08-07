#include <Geode/Geode.hpp>
#include <Geode/modify/RateStarsLayer.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
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

static std::string featureStateToSendType(int featureState) {
    switch (featureState) {
        case 1: return "featured";
        case 2: return "epic";
        case 3: return "legendary";
        case 4: return "mythic";
        default: return "star_rate";
    }
}

static std::string makeEventID(SendSnapshot const& snapshot) {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return std::to_string(millis) + "-" + std::to_string(snapshot.levelID) + "-" +
        std::to_string(snapshot.stars) + "-" + std::to_string(snapshot.featureState);
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

static matjson::Value buildPayload(SendSnapshot const& snapshot) {
    auto body = matjson::Value();
    body["eventId"] = makeEventID(snapshot);
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

static void reportSuccessfulSend(SendSnapshot snapshot) {
    if (!Mod::get()->getSettingValue<bool>("enabled")) {
        return;
    }
    if (snapshot.levelID <= 0 || snapshot.stars <= 0 || snapshot.stars > 10) {
        if (debugLogging()) {
            log::warn(
                "Ignoring send with invalid values: levelID={}, stars={}, featureState={}",
                snapshot.levelID,
                snapshot.stars,
                snapshot.featureState
            );
        }
        return;
    }
    if (isRecentDuplicate(snapshot)) {
        if (debugLogging()) {
            log::info("Duplicate moderator send suppressed for level {}", snapshot.levelID);
        }
        return;
    }

    auto connectionKey = trim(Mod::get()->getSettingValue<std::string>("connection-key"));

    if (connectionKey.empty()) {
        log::warn("GD Send Logger is not configured. Fill your personal Connection Key in mod settings.");
        return;
    }

    auto body = buildPayload(snapshot);
    auto req = web::WebRequest();
    req.header("Content-Type", "application/json");
    req.header("Authorization", "Bearer " + connectionKey);
    req.bodyJSON(body);
    req.timeout(std::chrono::seconds(15));

    if (debugLogging()) {
        log::info(
            "Moderator send detected: levelID={}, name='{}', creator='{}', stars={}, featureState={}, sendType={}, platformer={}",
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
        [snapshot](web::WebResponse res) {
            auto responseText = res.string().unwrapOr("");
            if (res.ok()) {
                if (debugLogging()) {
                    log::info("Bot accepted send for level {}: {}", snapshot.levelID, responseText);
                }
            } else {
                log::warn(
                    "Bot bridge rejected send for level {} (HTTP {}): {}",
                    snapshot.levelID,
                    res.code(),
                    responseText.empty() ? std::string("empty response") : responseText
                );
            }
        }
    );
}

} // namespace

class $modify(GDSendLoggerRateStarsLayer, RateStarsLayer) {
    static void onModify(auto& self) {
        // Geode resolves these through its cross-platform bindings. If a future GD/Geode
        // update removes either binding on one target, fail loudly in the Geode log.
        if (!self.getHook("RateStarsLayer::uploadActionFinished")) {
            log::error("GD Send Logger: failed to register RateStarsLayer::uploadActionFinished hook");
        }
        if (!self.getHook("RateStarsLayer::uploadActionFailed")) {
            log::error("GD Send Logger: failed to register RateStarsLayer::uploadActionFailed hook");
        }
    }

    void uploadActionFinished(int id, int response) override {
        // RateStarsLayer implements UploadActionDelegate with distinct Finished/Failed
        // callbacks. Only the Finished path publishes a moderator send. Capture fields
        // before the original handler can mutate/close the popup.
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
            reportSuccessfulSend(snapshot);
        }

        RateStarsLayer::uploadActionFinished(id, response);
    }

    void uploadActionFailed(int id, int response) override {
        // Never publish from the failure callback. Keeping this hook gives us a clear
        // diagnostic on every supported platform when GD rejects the moderator action.
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
