# GD Send Logger

Geode 5.6.1 / Geometry Dash 2.2081 mod that reports successful moderator sends to the Discord bot bridge added in the companion bot update.

## What it detects

The mod hooks `RateStarsLayer::uploadActionFinished`, and only reports when the rate popup belongs to a moderator (`m_moderator`). That means it reports the successful upload callback, not a mere button click.

Sent payload includes:

- Level ID
- stars (1-10; 10 is enough for Demon in the bot)
- raw feature state
- normalized send type
- level name, creator and platformer flag when the level is available locally
- the configured Discord User ID
- a unique event ID

If local level metadata is unavailable, the bot can fetch name/creator/platformer by Level ID.

## Settings

Open the mod settings in Geode:

- **Enabled** — turn reporting on/off
- **Connection Key** — copy from Discord `/send-config` -> `Connection info`
- **Discord User ID** — your own numeric Discord user ID
- **Debug Logging** — logs detected raw values and bridge responses

## Built-in API address

The API URL is **not** a Geode setting anymore. It is compiled into the `.geode` file.

For local builds the source fallback is:

```text
http://127.0.0.1:8765/api/v1/gd-send
```

For GitHub Actions builds, create a repository variable named `SEND_API_URL` with the production HTTPS endpoint, for example:

```text
https://send.example.com/api/v1/gd-send
```

The workflow refuses to produce distributable builds when this variable is missing or not HTTPS.

## Send type mapping

The current `RateStarsLayer::m_featureState` is sent raw and mapped as:

- 0 -> Star Rate (Moon Rate if platformer)
- 1 -> Featured
- 2 -> Epic
- 3 -> Legendary
- 4 -> Mythic

The raw value is also retained by the bot in `GeodeSendIngress` for debugging/migrations.

## Build

This is a normal Geode mod project. Set `GEODE_SDK` to a Geode SDK compatible with 5.6.1 and build with CMake as usual.

Example:

```bash
cmake -B build -G Ninja
cmake --build build
```

The source was based on the HTTP pattern used by the supplied level-folder-logger project (`web::WebRequest`, JSON body, async POST).
