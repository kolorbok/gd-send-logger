import asyncio
import os
import secrets
import string
import sqlite3
import time
from typing import Any

from aiohttp import web

from .core import (
    app_commands,
    bot,
    discord,
    fetch_gd_level_info,
)
from .command_access import (
    get_guild_member_from_config_role,
    has_admin_access,
    resolve_command_context,
)
from .modals_staff import (
    _build_staff_gd_rate_badge_file,
    _compose_send_title_parts,
    _normalize_send_type_value,
    _send_staff_components_message,
    _send_type_tier_kind,
    process_geode_moderator_request_send,
    process_geode_helper_request_send,
    process_geode_request_reject,
)


SEND_CONFIG_MODE_ALL = "all"
SEND_CONFIG_MODE_OUTSIDE_ONLY = "outside_only"
SEND_CONFIG_MODE_INSIDE_ONLY = "inside_only"
SEND_CONFIG_MODES = {SEND_CONFIG_MODE_ALL, SEND_CONFIG_MODE_OUTSIDE_ONLY, SEND_CONFIG_MODE_INSIDE_ONLY}

SEND_BRIDGE_HOST = os.getenv("SEND_BRIDGE_HOST", "0.0.0.0").strip() or "0.0.0.0"
try:
    SEND_BRIDGE_PORT = int(os.getenv("SEND_BRIDGE_PORT", "8765"))
except (TypeError, ValueError):
    SEND_BRIDGE_PORT = 8765
SEND_BRIDGE_PUBLIC_URL = os.getenv("SEND_BRIDGE_PUBLIC_URL", "").strip().rstrip("/")
SEND_BRIDGE_DATABASE = os.getenv("SEND_BRIDGE_DATABASE", "ultimate_bot_database.db").strip() or "ultimate_bot_database.db"

_bridge_runner: web.AppRunner | None = None
_bridge_site: web.TCPSite | None = None
_bridge_start_lock = asyncio.Lock()
_recent_event_ids: dict[tuple[str, str, str], float] = {}


def _db_connect():
    return sqlite3.connect(SEND_BRIDGE_DATABASE, timeout=10.0)


def _ensure_send_tables():
    """Create the personal send-config schema and preserve the old server-wide table once.

    v1 used ServerID as the primary key, so every moderator on a guild shared one key.
    v2 uses (ServerID, UserID), giving each moderator an independent key/config.
    """
    conn = _db_connect()
    try:
        cursor = conn.cursor()

        cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='SendConfig'")
        if cursor.fetchone() is not None:
            columns = [str(row[1]) for row in cursor.execute("PRAGMA table_info(SendConfig)").fetchall()]
            if "UserID" not in columns:
                # Keep the old shared config as a backup instead of silently assigning its
                # secret key to whichever moderator happens to run /send-config first.
                cursor.execute(
                    "SELECT name FROM sqlite_master WHERE type='table' AND name='SendConfigLegacyV1'"
                )
                legacy_exists = cursor.fetchone() is not None
                if legacy_exists:
                    cursor.execute(
                        """
                        INSERT OR IGNORE INTO SendConfigLegacyV1(
                            ServerID, Enabled, ChannelID, Mode, ApiKey, CreatedAt, UpdatedAt
                        )
                        SELECT ServerID, Enabled, ChannelID, Mode, ApiKey, CreatedAt, UpdatedAt
                        FROM SendConfig
                        """
                    )
                    cursor.execute("DROP TABLE SendConfig")
                else:
                    cursor.execute("ALTER TABLE SendConfig RENAME TO SendConfigLegacyV1")

        cursor.execute(
            """
            CREATE TABLE IF NOT EXISTS SendConfig (
                ServerID TEXT NOT NULL,
                UserID TEXT NOT NULL,
                Enabled INTEGER NOT NULL DEFAULT 0,
                ChannelID TEXT NOT NULL DEFAULT '0',
                Mode TEXT NOT NULL DEFAULT 'outside_only',
                RequestIntegration INTEGER NOT NULL DEFAULT 0,
                ApiKey TEXT NOT NULL UNIQUE,
                CreatedAt INTEGER NOT NULL,
                UpdatedAt INTEGER NOT NULL,
                PRIMARY KEY(ServerID, UserID)
            )
            """
        )
        send_config_columns = [
            str(row[1]) for row in cursor.execute("PRAGMA table_info(SendConfig)").fetchall()
        ]
        if "RequestIntegration" not in send_config_columns:
            cursor.execute(
                "ALTER TABLE SendConfig ADD COLUMN RequestIntegration INTEGER NOT NULL DEFAULT 0"
            )

        cursor.execute(
            "CREATE INDEX IF NOT EXISTS idx_send_config_server_user ON SendConfig(ServerID, UserID)"
        )
        cursor.execute(
            """
            CREATE TABLE IF NOT EXISTS GeodeSendIngress (
                IngressID INTEGER PRIMARY KEY AUTOINCREMENT,
                EventID TEXT,
                ServerID TEXT NOT NULL,
                ModeratorDiscordID TEXT NOT NULL,
                LevelID TEXT NOT NULL,
                LevelName TEXT NOT NULL DEFAULT '',
                LevelCreator TEXT NOT NULL DEFAULT '',
                Stars INTEGER NOT NULL DEFAULT 0,
                FeatureState INTEGER NOT NULL DEFAULT 0,
                SendType TEXT NOT NULL DEFAULT '',
                IsPlatformer INTEGER NOT NULL DEFAULT 0,
                ReceivedAt INTEGER NOT NULL,
                Published INTEGER NOT NULL DEFAULT 0,
                IgnoreReason TEXT NOT NULL DEFAULT ''
            )
            """
        )
        cursor.execute(
            "CREATE INDEX IF NOT EXISTS idx_geode_send_server_level ON GeodeSendIngress(ServerID, LevelID)"
        )
        conn.commit()
    finally:
        conn.close()


def _new_api_key() -> str:
    # Alphanumeric only: easy to paste/type in Geode settings on desktop and mobile.
    alphabet = string.ascii_uppercase + string.digits
    return "K" + "".join(secrets.choice(alphabet) for _ in range(31))


def _row_to_send_config(row) -> dict[str, Any]:
    return {
        "server_id": str(row[0]),
        "user_id": str(row[1]),
        "enabled": bool(row[2]),
        "channel_id": str(row[3] or "0"),
        "mode": str(row[4] or SEND_CONFIG_MODE_OUTSIDE_ONLY),
        "request_integration": bool(row[5]),
        "api_key": str(row[6] or ""),
        "created_at": int(row[7] or 0),
        "updated_at": int(row[8] or 0),
    }


def _get_or_create_send_config(server_id: int | str, user_id: int | str) -> dict[str, Any]:
    _ensure_send_tables()
    server_id_text = str(server_id)
    user_id_text = str(user_id)
    conn = _db_connect()
    try:
        cursor = conn.cursor()
        cursor.execute(
            """
            SELECT ServerID, UserID, Enabled, ChannelID, Mode, RequestIntegration, ApiKey, CreatedAt, UpdatedAt
            FROM SendConfig WHERE ServerID = ? AND UserID = ?
            """,
            (server_id_text, user_id_text),
        )
        row = cursor.fetchone()
        if row is None:
            now = int(time.time())
            api_key = _new_api_key()
            cursor.execute(
                """
                INSERT INTO SendConfig(
                    ServerID, UserID, Enabled, ChannelID, Mode, RequestIntegration, ApiKey, CreatedAt, UpdatedAt
                ) VALUES (?, ?, 0, '0', ?, 0, ?, ?, ?)
                """,
                (server_id_text, user_id_text, SEND_CONFIG_MODE_OUTSIDE_ONLY, api_key, now, now),
            )
            conn.commit()
            row = (
                server_id_text, user_id_text, 0, "0", SEND_CONFIG_MODE_OUTSIDE_ONLY, 0, api_key, now, now
            )
        return _row_to_send_config(row)
    finally:
        conn.close()


def _get_send_config_by_key(api_key: str) -> dict[str, Any] | None:
    if not api_key:
        return None
    _ensure_send_tables()
    conn = _db_connect()
    try:
        cursor = conn.cursor()
        cursor.execute(
            """
            SELECT ServerID, UserID, Enabled, ChannelID, Mode, RequestIntegration, ApiKey, CreatedAt, UpdatedAt
            FROM SendConfig WHERE ApiKey = ? LIMIT 1
            """,
            (str(api_key),),
        )
        row = cursor.fetchone()
        return None if row is None else _row_to_send_config(row)
    finally:
        conn.close()


def _update_send_config(server_id: int | str, user_id: int | str, field: str, value: Any):
    allowed_fields = {"Enabled", "ChannelID", "Mode", "RequestIntegration", "ApiKey"}
    if field not in allowed_fields:
        raise ValueError("Unsupported SendConfig field")
    _get_or_create_send_config(server_id, user_id)
    conn = _db_connect()
    try:
        cursor = conn.cursor()
        cursor.execute(
            f"UPDATE SendConfig SET {field} = ?, UpdatedAt = ? WHERE ServerID = ? AND UserID = ?",
            (value, int(time.time()), str(server_id), str(user_id)),
        )
        conn.commit()
    finally:
        conn.close()


def _has_send_config_access(guild: discord.Guild, server_id: int | str, member: discord.Member) -> bool:
    """Admins and configured ModeratorRole members can create/use personal send configs."""
    if has_admin_access(member):
        return True
    return bool(
        get_guild_member_from_config_role(
            guild, server_id, "ModeratorRole", member, event="0"
        )
    )


def _find_latest_server_request_level(server_id: int | str, level_id: int | str) -> dict[str, Any] | None:
    """Return the newest request row for this Level ID on this Discord server."""
    conn = _db_connect()
    try:
        cursor = conn.cursor()
        cursor.execute(
            """
            SELECT RequestID, COALESCE(Event, '0')
            FROM Sheet
            WHERE ServerID = ? AND LevelID = ?
            ORDER BY RequestID DESC
            LIMIT 1
            """,
            (str(server_id), str(level_id)),
        )
        row = cursor.fetchone()
        if row is None:
            return None
        return {"request_id": int(row[0]), "event": str(row[1] or "0")}
    except sqlite3.OperationalError as exc:
        if "no such table" in str(exc).lower():
            return None
        raise
    finally:
        conn.close()


def _server_has_request_level(server_id: int | str, level_id: int | str) -> bool:
    return _find_latest_server_request_level(server_id, level_id) is not None


def _write_ingress_log(
    *,
    event_id: str,
    server_id: str,
    moderator_discord_id: str,
    level_id: str,
    level_name: str,
    level_creator: str,
    stars: int,
    feature_state: int,
    send_type: str,
    platformer: bool,
    published: bool,
    ignore_reason: str = "",
):
    try:
        _ensure_send_tables()
        conn = _db_connect()
        try:
            conn.execute(
                """
                INSERT INTO GeodeSendIngress(
                    EventID, ServerID, ModeratorDiscordID, LevelID, LevelName, LevelCreator,
                    Stars, FeatureState, SendType, IsPlatformer, ReceivedAt, Published, IgnoreReason
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    str(event_id or ""),
                    str(server_id),
                    str(moderator_discord_id),
                    str(level_id),
                    str(level_name or ""),
                    str(level_creator or ""),
                    int(stars),
                    int(feature_state),
                    str(send_type or ""),
                    1 if platformer else 0,
                    int(time.time()),
                    1 if published else 0,
                    str(ignore_reason or ""),
                ),
            )
            conn.commit()
        finally:
            conn.close()
    except Exception as exc:
        print(f"[SEND BRIDGE] ingress log failed: {exc}")


def _extract_bearer_token(request: web.Request, payload: dict[str, Any] | None = None) -> str:
    authorization = str(request.headers.get("Authorization", "") or "").strip()
    if authorization.lower().startswith("bearer "):
        return authorization[7:].strip()
    payload = payload or {}
    return str(payload.get("connectionKey") or payload.get("apiKey") or "").strip()


def _safe_int(value, default: int = 0) -> int:
    try:
        return int(str(value).strip())
    except (TypeError, ValueError):
        return default


def _safe_bool(value) -> bool:
    if isinstance(value, bool):
        return value
    return str(value or "").strip().lower() in {"1", "true", "yes", "on", "platformer"}


def _feature_state_to_send_type(feature_state: int) -> str:
    # RateStarsLayer uses this monotonic feature state in current GD builds.
    # Raw featureState is also kept in ingress logs so the mapping is easy to inspect/change.
    return {
        0: "star_rate",
        1: "featured",
        2: "epic",
        3: "legendary",
        4: "mythic",
    }.get(int(feature_state), "star_rate")


def _normalize_external_send_type(raw_send_type: Any, feature_state: int, platformer: bool) -> str:
    normalized = _normalize_send_type_value(str(raw_send_type or ""))
    if normalized not in {"star_rate", "moon_rate", "featured", "epic", "legendary", "mythic"}:
        normalized = _feature_state_to_send_type(feature_state)
    if normalized == "star_rate" and platformer:
        return "moon_rate"
    return normalized


def _difficulty_base_kind_from_stars(stars: int) -> str:
    if stars >= 10:
        return "demon-hard"
    if stars <= 1:
        return "auto"
    if stars == 2:
        return "easy"
    if stars == 3:
        return "normal"
    if stars in (4, 5):
        return "hard"
    if stars in (6, 7):
        return "harder"
    if stars in (8, 9):
        return "insane"
    return "hard"


def _is_duplicate_event(server_id: str, user_id: str, event_id: str) -> bool:
    if not event_id:
        return False
    now = time.monotonic()
    expiry_seconds = 300.0
    stale = [key for key, seen_at in _recent_event_ids.items() if now - seen_at > expiry_seconds]
    for key in stale:
        _recent_event_ids.pop(key, None)
    key = (str(server_id), str(user_id), str(event_id))
    if key in _recent_event_ids:
        return True
    _recent_event_ids[key] = now
    return False




def _api_base_url() -> str:
    endpoint = _public_endpoint()
    if endpoint.endswith("/gd-send"):
        return endpoint[:-len("/gd-send")]
    return endpoint.rsplit("/", 1)[0]


def _member_role_ids(member: discord.Member | None) -> set[int]:
    if member is None:
        return set()
    result = set()
    for role in getattr(member, "roles", ()) or ():
        try:
            result.add(int(role.id))
        except Exception:
            pass
    return result


def _configured_role_ids_for_server(server_id: int | str, column_name: str) -> set[int]:
    if column_name not in {"HelperRole", "ReviewerRole", "ModeratorRole"}:
        return set()
    conn = _db_connect()
    try:
        cursor = conn.cursor()
        try:
            cursor.execute(
                f"SELECT {column_name} FROM ServerInfo WHERE ServerID = ?",
                (str(server_id),),
            )
        except sqlite3.OperationalError:
            return set()
        role_ids = set()
        for row in cursor.fetchall() or []:
            raw = str(row[0] or "").strip()
            for part in raw.replace(";", ",").split(","):
                part = part.strip()
                if not part or part == "0":
                    continue
                try:
                    role_ids.add(int(part))
                except (TypeError, ValueError):
                    continue
        return role_ids
    finally:
        conn.close()


def _request_client_capabilities(guild: discord.Guild, server_id: int | str, member: discord.Member) -> dict[str, bool]:
    member_roles = _member_role_ids(member)
    moderator_roles = _configured_role_ids_for_server(server_id, "ModeratorRole")
    helper_roles = _configured_role_ids_for_server(server_id, "HelperRole")
    reviewer_roles = _configured_role_ids_for_server(server_id, "ReviewerRole")
    return {
        "moderator": bool(member_roles & moderator_roles),
        "helper": bool(member_roles & helper_roles),
        "reviewer": bool(member_roles & reviewer_roles),
    }


def _default_request_client_mode(capabilities: dict[str, bool]) -> str:
    if capabilities.get("moderator"):
        return "moderator"
    if capabilities.get("helper"):
        return "helper"
    if capabilities.get("reviewer"):
        return "reviewer"
    return "all"


def _resolve_requested_client_mode(raw_mode: Any, capabilities: dict[str, bool]) -> str:
    requested = str(raw_mode or "auto").strip().lower()
    if requested in {"moderator", "helper", "reviewer"} and capabilities.get(requested):
        return requested
    if requested == "all":
        return "all"
    return _default_request_client_mode(capabilities)


def _actor_field_has_user(value: Any, user_id: int | str) -> bool:
    text = str(value or "")
    uid = str(user_id)
    if not text.strip() or not uid:
        return False
    tokens = {uid, f"<@{uid}>", f"<@!{uid}>"}
    for raw in text.replace(";", ",").split(","):
        part = raw.strip()
        if part in tokens or part.strip("<@!>") == uid:
            return True
    # Preserve compatibility with older rows that were stored without clean CSV separators.
    return f"<@{uid}>" in text or f"<@!{uid}>" in text


_REQUEST_SEND_FIELDS = {
    "helper": (
        "HelpersSend", "HelperStarRateSends", "HelperFeaturedSends", "HelperEpicSends",
        "HelperLegendarySends", "HelperMythicSends",
    ),
    "moderator": (
        "ModeratorsSend", "ModeratorStarRateSends", "ModeratorFeaturedSends", "ModeratorEpicSends",
        "ModeratorLegendarySends", "ModeratorMythicSends",
    ),
}
_REQUEST_REJECT_FIELDS = {
    "helper": (
        "HelpersNotSend", "HelperAlreadyRatedNotSends", "HelperAlreadySeenNotSends",
        "HelperWrongIDNotSends", "HelperReportNotSends",
    ),
    "moderator": (
        "ModeratorsNotSend", "ModeratorAlreadyRatedNotSends", "ModeratorAlreadySeenNotSends",
        "ModeratorWrongIDNotSends", "ModeratorReportNotSends",
    ),
}
_REQUEST_TIER_FIELDS = {
    "helper": {
        "star_rate": "HelperStarRateSends",
        "featured": "HelperFeaturedSends",
        "epic": "HelperEpicSends",
        "legendary": "HelperLegendarySends",
        "mythic": "HelperMythicSends",
    },
    "moderator": {
        "star_rate": "ModeratorStarRateSends",
        "featured": "ModeratorFeaturedSends",
        "epic": "ModeratorEpicSends",
        "legendary": "ModeratorLegendarySends",
        "mythic": "ModeratorMythicSends",
    },
}
_REQUEST_TIER_ORDER = ("star_rate", "featured", "epic", "legendary", "mythic")


def _row_user_status(row: sqlite3.Row | dict[str, Any], user_id: int | str, mode: str) -> str:
    if mode not in {"helper", "moderator"}:
        return "all"
    for field in _REQUEST_SEND_FIELDS[mode]:
        if _actor_field_has_user(row[field], user_id):
            return "sent"
    for field in _REQUEST_REJECT_FIELDS[mode]:
        if _actor_field_has_user(row[field], user_id):
            return "rejected"
    return "unchecked"


def _row_matches_minimum_user_send(row: sqlite3.Row | dict[str, Any], user_id: int | str, mode: str, minimum: str) -> bool:
    if mode not in {"helper", "moderator"}:
        return True
    minimum = str(minimum or "any").strip().lower()
    if minimum in {"", "any", "all", "none"}:
        return True
    if minimum not in _REQUEST_TIER_ORDER:
        return True
    start = _REQUEST_TIER_ORDER.index(minimum)
    for tier in _REQUEST_TIER_ORDER[start:]:
        field = _REQUEST_TIER_FIELDS[mode][tier]
        if _actor_field_has_user(row[field], user_id):
            return True
    return False


def _requested_difficulty_number(raw_value: Any) -> int:
    text = str(raw_value or "").strip()
    if not text:
        return 0
    first = text.split(maxsplit=1)[0]
    try:
        return int(first)
    except (TypeError, ValueError):
        return 0


def _query_request_rows(server_id: int | str) -> list[sqlite3.Row]:
    conn = _db_connect()
    conn.row_factory = sqlite3.Row
    try:
        cursor = conn.cursor()
        try:
            cursor.execute(
                """
                SELECT
                    RequestID, UserMention, LevelName, LevelID, LevelCreator, LevelDifficulty,
                    IsPlatformer, ReviewLanguage, ServerID, HelpersSend, HelpersNotSend, Reviewers,
                    SentTo, IsRated, ModeratorsSend, ModeratorsNotSend, Review, Event,
                    HelperStarRateSends, HelperFeaturedSends, HelperEpicSends, HelperLegendarySends, HelperMythicSends,
                    ModeratorStarRateSends, ModeratorFeaturedSends, ModeratorEpicSends, ModeratorLegendarySends, ModeratorMythicSends,
                    HelperAlreadyRatedNotSends, HelperAlreadySeenNotSends, HelperWrongIDNotSends, HelperReportNotSends,
                    ModeratorAlreadyRatedNotSends, ModeratorAlreadySeenNotSends, ModeratorWrongIDNotSends, ModeratorReportNotSends
                FROM Sheet
                WHERE ServerID = ?
                """,
                (str(server_id),),
            )
        except sqlite3.OperationalError:
            return []
        return list(cursor.fetchall() or [])
    finally:
        conn.close()


def _filter_request_rows(
    rows: list[sqlite3.Row],
    *,
    user_id: int | str,
    mode: str,
    difficulty: str,
    level_type: str,
    status: str,
    minimum_send: str,
    rated: str,
    sort_mode: str,
    query_text: str,
) -> list[sqlite3.Row]:
    difficulty = str(difficulty or "all").strip().lower()
    level_type = str(level_type or "all").strip().lower()
    status = str(status or "unchecked").strip().lower()
    minimum_send = str(minimum_send or "any").strip().lower()
    rated = str(rated or "all").strip().lower()
    sort_mode = str(sort_mode or "newest").strip().lower()
    query_text = str(query_text or "").strip().casefold()

    filtered: list[sqlite3.Row] = []
    for row in rows:
        level_id_text = str(row["LevelID"] or "").strip()
        try:
            if int(level_id_text) <= 0:
                continue
        except (TypeError, ValueError):
            continue

        # Reviewer mode mirrors the important /req-reviewer eligibility rules, but the
        # first in-game version intentionally does not expose Event or language filters.
        if mode == "reviewer":
            if str(row["Reviewers"] or "").strip():
                continue
            if str(row["IsRated"] or "").strip().lower() == "yes":
                continue
            if str(row["Review"] or "0").strip() not in {
                "0", "Yes", "Yes Review and Feedback", "Yes Review No Feedback", "Yes Review Yes Feedback"
            }:
                continue

        if difficulty not in {"", "all", "any"}:
            try:
                wanted_difficulty = int(difficulty)
            except (TypeError, ValueError):
                wanted_difficulty = 0
            if wanted_difficulty > 0 and _requested_difficulty_number(row["LevelDifficulty"]) != wanted_difficulty:
                continue

        is_platformer = str(row["IsPlatformer"] or "").strip().lower() in {"yes", "true", "1", "platformer"}
        if level_type == "platformer" and not is_platformer:
            continue
        if level_type == "classic" and is_platformer:
            continue

        is_rated = str(row["IsRated"] or "").strip().lower() == "yes"
        if rated == "rated" and not is_rated:
            continue
        if rated == "unrated" and is_rated:
            continue

        if query_text:
            haystack = " ".join(
                str(row[name] or "")
                for name in ("LevelID", "LevelName", "LevelCreator", "RequestID")
            ).casefold()
            if query_text not in haystack:
                continue

        if mode in {"helper", "moderator"}:
            user_status = _row_user_status(row, user_id, mode)
            if status in {"unchecked", "sent", "rejected"} and user_status != status:
                continue
            if not _row_matches_minimum_user_send(row, user_id, mode, minimum_send):
                continue

        filtered.append(row)

    if sort_mode == "oldest":
        filtered.sort(key=lambda row: int(row["RequestID"] or 0))
    elif sort_mode == "random":
        import random
        random.shuffle(filtered)
    else:
        filtered.sort(key=lambda row: int(row["RequestID"] or 0), reverse=True)

    # A vanilla GD ID search can display one cell per LevelID, not two copies of the
    # same online level. Keep the first row after sorting and bind that exact RequestID
    # to the in-game Request Context.
    deduped: list[sqlite3.Row] = []
    seen_level_ids: set[str] = set()
    for row in filtered:
        level_key = str(row["LevelID"] or "").strip()
        if level_key in seen_level_ids:
            continue
        seen_level_ids.add(level_key)
        deduped.append(row)
    return deduped


async def _resolve_connected_member(config: dict[str, Any]):
    server_id = str(config["server_id"])
    user_id = str(config["user_id"])
    guild = bot.get_guild(_safe_int(server_id, 0))
    if guild is None:
        return None, None
    try:
        member = guild.get_member(int(user_id))
        if member is None:
            member = await guild.fetch_member(int(user_id))
    except Exception:
        member = None
    return guild, member


async def _handle_client_me(request: web.Request):
    api_key = _extract_bearer_token(request)
    config = _get_send_config_by_key(api_key)
    if config is None:
        return web.json_response({"ok": False, "error": "invalid_connection_key"}, status=401)
    guild, member = await _resolve_connected_member(config)
    if guild is None or member is None:
        return web.json_response({"ok": False, "error": "discord_member_not_available"}, status=403)
    capabilities = _request_client_capabilities(guild, config["server_id"], member)
    return web.json_response({
        "ok": True,
        "serverId": str(config["server_id"]),
        "serverName": guild.name,
        "userId": str(config["user_id"]),
        "userName": str(getattr(member, "display_name", getattr(member, "name", config["user_id"]))),
        "moderator": bool(capabilities["moderator"]),
        "helper": bool(capabilities["helper"]),
        "reviewer": bool(capabilities["reviewer"]),
        "defaultMode": _default_request_client_mode(capabilities),
    })


async def _handle_requests(request: web.Request):
    api_key = _extract_bearer_token(request)
    config = _get_send_config_by_key(api_key)
    if config is None:
        return web.Response(text="ERR\tinvalid_connection_key\n", status=401, content_type="text/plain")
    guild, member = await _resolve_connected_member(config)
    if guild is None or member is None:
        return web.Response(text="ERR\tdiscord_member_not_available\n", status=403, content_type="text/plain")

    capabilities = _request_client_capabilities(guild, config["server_id"], member)
    mode = _resolve_requested_client_mode(request.query.get("mode"), capabilities)
    status_default = "unchecked" if mode in {"helper", "moderator"} else "all"
    rows = _filter_request_rows(
        _query_request_rows(config["server_id"]),
        user_id=config["user_id"],
        mode=mode,
        difficulty=request.query.get("difficulty", "all"),
        level_type=request.query.get("type", "all"),
        status=request.query.get("status", status_default),
        minimum_send=request.query.get("minSend", "any"),
        rated=request.query.get("rated", "all"),
        sort_mode=request.query.get("sort", "newest"),
        query_text=request.query.get("q", ""),
    )

    total = len(rows)
    try:
        limit = max(1, min(100, int(request.query.get("limit", "100"))))
    except (TypeError, ValueError):
        limit = 100
    rows = rows[:limit]

    lines = [
        "META\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}".format(
            mode,
            str(config["server_id"]),
            str(config["user_id"]),
            1 if capabilities["moderator"] else 0,
            1 if capabilities["helper"] else 0,
            1 if capabilities["reviewer"] else 0,
            total,
            len(rows),
        )
    ]
    for row in rows:
        lines.append(
            "REQ\t{}\t{}\t{}\t{}\t{}".format(
                int(row["RequestID"] or 0),
                str(row["LevelID"] or "0"),
                str(row["Event"] or "0").replace("\t", " ").replace("\n", " "),
                _requested_difficulty_number(row["LevelDifficulty"]),
                1 if str(row["IsRated"] or "").strip().lower() == "yes" else 0,
            )
        )
    return web.Response(text="\n".join(lines) + "\n", content_type="text/plain")


async def _handle_request_result(request: web.Request):
    try:
        payload = await request.json()
    except Exception:
        return web.json_response({"ok": False, "error": "invalid_json"}, status=400)
    if not isinstance(payload, dict):
        return web.json_response({"ok": False, "error": "invalid_payload"}, status=400)

    api_key = _extract_bearer_token(request, payload)
    config = _get_send_config_by_key(api_key)
    if config is None:
        return web.json_response({"ok": False, "error": "invalid_connection_key"}, status=401)
    guild, member = await _resolve_connected_member(config)
    if guild is None or member is None:
        return web.json_response({"ok": False, "error": "discord_member_not_available"}, status=403)

    request_id = _safe_int(payload.get("requestId", payload.get("request_id")), 0)
    if request_id <= 0:
        return web.json_response({"ok": False, "error": "invalid_request_id"}, status=400)
    event_id = str(payload.get("eventId") or payload.get("event_id") or "").strip()
    if _is_duplicate_event(str(config["server_id"]), str(config["user_id"]), event_id):
        return web.json_response({"ok": True, "published": False, "ignored": "duplicate_event"})

    action = str(payload.get("action") or "").strip().lower()
    requested_mode = str(payload.get("mode") or "auto").strip().lower()
    capabilities = _request_client_capabilities(guild, config["server_id"], member)
    mode = _resolve_requested_client_mode(requested_mode, capabilities)
    feedback = str(payload.get("feedback") or payload.get("note") or "").strip()
    if len(feedback) > 1500:
        return web.json_response({"ok": False, "error": "feedback_too_long", "max": 1500}, status=400)

    if action == "send":
        if mode != "helper" or not capabilities.get("helper"):
            return web.json_response({"ok": False, "error": "helper_access_required"}, status=403)
        stars = _safe_int(payload.get("stars"), 0)
        feature_state = _safe_int(payload.get("featureState", payload.get("feature_state")), 0)
        if stars < 1 or stars > 10:
            return web.json_response({"ok": False, "error": "invalid_stars"}, status=400)
        platformer = _safe_bool(payload.get("platformer", False))
        send_type = _normalize_external_send_type(payload.get("sendType"), feature_state, platformer)
        result = await process_geode_helper_request_send(
            server_id=config["server_id"],
            request_id=request_id,
            helper=member,
            send_type_raw=send_type,
            stars=stars,
            feedback=feedback,
        )
    elif action == "reject":
        if mode not in {"helper", "moderator"}:
            return web.json_response({"ok": False, "error": "staff_access_required"}, status=403)
        if not capabilities.get(mode):
            return web.json_response({"ok": False, "error": f"{mode}_access_required"}, status=403)
        reason = str(payload.get("reason") or "").strip().lower()
        if reason not in {"", "not_sent", "already_seen", "already_rated", "report"}:
            return web.json_response({"ok": False, "error": "invalid_reject_reason"}, status=400)
        if reason == "not_sent":
            reason = ""
        result = await process_geode_request_reject(
            server_id=config["server_id"],
            request_id=request_id,
            actor=member,
            staff_kind=mode,
            reason=reason,
            feedback=feedback,
        )
    else:
        return web.json_response({"ok": False, "error": "invalid_action"}, status=400)

    if not result.get("handled"):
        return web.json_response(
            {"ok": False, "error": str(result.get("reason") or "request_result_failed")},
            status=409,
        )
    message = result.get("message")
    return web.json_response({
        "ok": True,
        "published": True,
        "requestId": int(result.get("request_id") or request_id),
        "mode": mode,
        "action": action,
        "overwrittenRecords": int(result.get("overwritten_records") or 0),
        "messageId": str(getattr(message, "id", "") or ""),
    })

def _public_endpoint() -> str:
    if SEND_BRIDGE_PUBLIC_URL:
        return f"{SEND_BRIDGE_PUBLIC_URL}/api/v1/gd-send"
    host_for_display = SEND_BRIDGE_HOST
    return f"http://{host_for_display}:{SEND_BRIDGE_PORT}/api/v1/gd-send"


def _masked_key(api_key: str) -> str:
    if len(api_key) <= 12:
        return "••••••••"
    return f"{api_key[:6]}…{api_key[-6:]}"


def _send_config_mode_label(mode: str) -> str:
    if mode == SEND_CONFIG_MODE_ALL:
        return "All sends"
    if mode == SEND_CONFIG_MODE_INSIDE_ONLY:
        return "Only inside bot"
    return "Only outside bot"


def _send_config_text(
    server_id: int | str, user_id: int | str, guild: discord.Guild | None = None
) -> str:
    config = _get_or_create_send_config(server_id, user_id)
    status = "Enabled" if config["enabled"] else "Disabled"
    channel_text = "Not selected"
    channel_id = _safe_int(config["channel_id"], 0)
    if channel_id > 0:
        channel = guild.get_channel(channel_id) if guild is not None else None
        channel_text = channel.mention if channel is not None else f"<#{channel_id}>"
    return (
        "## Your GD Requests Send Logging\n"
        f"**Moderator:** <@{user_id}>\n"
        f"**Status:** {status}\n"
        f"**Channel:** {channel_text}\n"
        f"**Mode:** {_send_config_mode_label(config['mode'])}\n"
        f"**Request integration:** {'Enabled' if config['request_integration'] else 'Disabled'}\n"
        f"**Personal connection key:** `{_masked_key(config['api_key'])}`\n\n"
        "-# Ask kolorbok if you want to configure this mod. (GD Mods only)\n\n"
        "**Request integration** means: when this Level ID exists in this server's requests, "
        "the Geode send is handled like your normal moderator request result (including requester ping and overwrite). "
        "If that request has no usable Mod Send channel, the bot falls back to your Channel above as a normal external send.\n\n"
        "**Only outside bot** logs normal external sends only when the Level ID is not in this server's requests.\n"
        "**Only inside bot** logs only Level IDs that exist in this server's requests. "
        "If request integration handles the send, it becomes a normal moderator request result; otherwise it uses your Channel above."
    )


class _SendConfigModeSelect(discord.ui.Select):
    def __init__(self, owner_id: int, server_id: int | str):
        self.owner_id = int(owner_id)
        self.server_id = str(server_id)
        config = _get_or_create_send_config(server_id, owner_id)
        options = [
            discord.SelectOption(
                label="All sends",
                value=SEND_CONFIG_MODE_ALL,
                description="Post every send received from your Geode client.",
                default=config["mode"] == SEND_CONFIG_MODE_ALL,
            ),
            discord.SelectOption(
                label="Only outside bot",
                value=SEND_CONFIG_MODE_OUTSIDE_ONLY,
                description="Log only Level IDs not found in this server's requests.",
                default=config["mode"] == SEND_CONFIG_MODE_OUTSIDE_ONLY,
            ),
            discord.SelectOption(
                label="Only inside bot",
                value=SEND_CONFIG_MODE_INSIDE_ONLY,
                description="Log only Level IDs found in this server's requests.",
                default=config["mode"] == SEND_CONFIG_MODE_INSIDE_ONLY,
            ),
        ]
        super().__init__(placeholder="Select your send mode", min_values=1, max_values=1, options=options)

    async def callback(self, interaction: discord.Interaction):
        if interaction.user.id != self.owner_id:
            return await interaction.response.send_message("This personal config belongs to another moderator.", ephemeral=True)
        mode = self.values[0]
        if mode not in SEND_CONFIG_MODES:
            return await interaction.response.send_message("Invalid mode.", ephemeral=True)
        _update_send_config(self.server_id, self.owner_id, "Mode", mode)
        await interaction.response.edit_message(
            content=_send_config_text(self.server_id, self.owner_id, interaction.guild),
            view=_SendConfigView(owner_id=self.owner_id, server_id=self.server_id),
        )


class _SendConfigChannelSelect(discord.ui.ChannelSelect):
    def __init__(self, owner_id: int, server_id: int | str):
        self.owner_id = int(owner_id)
        self.server_id = str(server_id)
        super().__init__(
            placeholder="Select channel for your detected sends",
            min_values=1,
            max_values=1,
            channel_types=[discord.ChannelType.text, discord.ChannelType.news],
        )

    async def callback(self, interaction: discord.Interaction):
        if interaction.user.id != self.owner_id:
            return await interaction.response.send_message("This personal config belongs to another moderator.", ephemeral=True)
        channel = self.values[0]
        _update_send_config(self.server_id, self.owner_id, "ChannelID", str(channel.id))
        await interaction.response.edit_message(
            content=_send_config_text(self.server_id, self.owner_id, interaction.guild),
            view=_SendConfigView(owner_id=self.owner_id, server_id=self.server_id),
        )


class _SendConfigView(discord.ui.View):
    def __init__(self, *, owner_id: int, server_id: int | str):
        super().__init__(timeout=600)
        self.owner_id = int(owner_id)
        self.server_id = str(server_id)
        config = _get_or_create_send_config(server_id, owner_id)

        self.add_item(_SendConfigModeSelect(self.owner_id, self.server_id))
        self.add_item(_SendConfigChannelSelect(self.owner_id, self.server_id))

        toggle_button = discord.ui.Button(
            label="Disable" if config["enabled"] else "Enable",
            style=discord.ButtonStyle.danger if config["enabled"] else discord.ButtonStyle.success,
            row=2,
        )
        toggle_button.callback = self._toggle_callback
        self.add_item(toggle_button)

        integration_button = discord.ui.Button(
            label="Disable request integration" if config["request_integration"] else "Enable request integration",
            style=discord.ButtonStyle.danger if config["request_integration"] else discord.ButtonStyle.success,
            row=2,
        )
        integration_button.callback = self._request_integration_callback
        self.add_item(integration_button)

        info_button = discord.ui.Button(label="Connection info", style=discord.ButtonStyle.primary, row=2)
        info_button.callback = self._connection_info_callback
        self.add_item(info_button)

        regenerate_button = discord.ui.Button(label="Regenerate my key", style=discord.ButtonStyle.secondary, row=2)
        regenerate_button.callback = self._regenerate_key_callback
        self.add_item(regenerate_button)

    async def _guard(self, interaction: discord.Interaction) -> bool:
        if interaction.user.id == self.owner_id:
            return True
        await interaction.response.send_message("This personal config belongs to another moderator.", ephemeral=True)
        return False

    async def _toggle_callback(self, interaction: discord.Interaction):
        if not await self._guard(interaction):
            return
        config = _get_or_create_send_config(self.server_id, self.owner_id)
        _update_send_config(self.server_id, self.owner_id, "Enabled", 0 if config["enabled"] else 1)
        await interaction.response.edit_message(
            content=_send_config_text(self.server_id, self.owner_id, interaction.guild),
            view=_SendConfigView(owner_id=self.owner_id, server_id=self.server_id),
        )

    async def _request_integration_callback(self, interaction: discord.Interaction):
        if not await self._guard(interaction):
            return
        config = _get_or_create_send_config(self.server_id, self.owner_id)
        _update_send_config(
            self.server_id,
            self.owner_id,
            "RequestIntegration",
            0 if config["request_integration"] else 1,
        )
        await interaction.response.edit_message(
            content=_send_config_text(self.server_id, self.owner_id, interaction.guild),
            view=_SendConfigView(owner_id=self.owner_id, server_id=self.server_id),
        )

    async def _connection_info_callback(self, interaction: discord.Interaction):
        if not await self._guard(interaction):
            return
        config = _get_or_create_send_config(self.server_id, self.owner_id)
        await interaction.response.send_message(
            "### Your Geode connection\n"
            f"**Connection Key:** `{config['api_key']}`\n"
            "-# Paste only this key into GD Requests.",
            ephemeral=True,
        )

    async def _regenerate_key_callback(self, interaction: discord.Interaction):
        if not await self._guard(interaction):
            return
        new_key = _new_api_key()
        _update_send_config(self.server_id, self.owner_id, "ApiKey", new_key)
        await interaction.response.edit_message(
            content=_send_config_text(self.server_id, self.owner_id, interaction.guild),
            view=_SendConfigView(owner_id=self.owner_id, server_id=self.server_id),
        )
        await interaction.followup.send(
            "Your personal connection key was regenerated. Update the key in your Geode client.",
            ephemeral=True,
        )


@app_commands.guild_only()
@bot.tree.command(name="send-config", description="Configure your personal Geometry Dash moderator send connection")
async def sendconfig(interaction: discord.Interaction):
    command_context = await resolve_command_context(interaction, bot_instance=bot)
    if command_context is None:
        return
    server_id, guild, guild_member = command_context
    if not _has_send_config_access(guild, server_id, guild_member):
        return await interaction.response.send_message(
            "You are not a configured moderator or administrator on this server :(",
            ephemeral=True,
        )

    _get_or_create_send_config(server_id, interaction.user.id)
    await interaction.response.send_message(
        _send_config_text(server_id, interaction.user.id, guild),
        view=_SendConfigView(owner_id=interaction.user.id, server_id=server_id),
        ephemeral=True,
    )




class _GeodeLinkView(discord.ui.View):
    def __init__(self, *, owner_id: int, server_id: int | str):
        super().__init__(timeout=600)
        self.owner_id = int(owner_id)
        self.server_id = str(server_id)

    @discord.ui.button(label="Regenerate key", style=discord.ButtonStyle.secondary)
    async def regenerate(self, interaction: discord.Interaction, button: discord.ui.Button):
        if interaction.user.id != self.owner_id:
            return await interaction.response.send_message("This connection belongs to another user.", ephemeral=True)
        new_key = _new_api_key()
        _update_send_config(self.server_id, self.owner_id, "ApiKey", new_key)
        await interaction.response.edit_message(
            content=(
                "## Your GD Requests connection\n"
                f"**Connection Key:** `{new_key}`\n"
                "-# Paste this key into the Geode mod. It links the mod to this Discord user and server."
            ),
            view=_GeodeLinkView(owner_id=self.owner_id, server_id=self.server_id),
        )


@app_commands.guild_only()
@bot.tree.command(name="geode-link", description="Get the personal key used by the GD Requests Geode mod")
async def geodelink(interaction: discord.Interaction):
    command_context = await resolve_command_context(interaction, bot_instance=bot)
    if command_context is None:
        return
    server_id, guild, guild_member = command_context
    config = _get_or_create_send_config(server_id, interaction.user.id)
    capabilities = _request_client_capabilities(guild, server_id, guild_member)
    roles = [name.title() for name in ("moderator", "helper", "reviewer") if capabilities.get(name)]
    roles_text = ", ".join(roles) if roles else "Member"
    await interaction.response.send_message(
        "## Your GD Requests connection\n"
        f"**Server:** {guild.name}\n"
        f"**Linked Discord user:** {interaction.user.mention}\n"
        f"**Request access:** {roles_text}\n"
        f"**Connection Key:** `{config['api_key']}`\n\n"
        "-# Paste this key into the Geode mod. Roles are checked again by the bot when the mod requests levels or submits a result.",
        view=_GeodeLinkView(owner_id=interaction.user.id, server_id=server_id),
        ephemeral=True,
    )


async def _resolve_target_channel(guild: discord.Guild, channel_id: str):
    channel_int = _safe_int(channel_id, 0)
    if channel_int <= 0:
        return None
    channel = bot.get_channel(channel_int) or guild.get_channel(channel_int)
    if channel is not None:
        return channel
    try:
        return await bot.fetch_channel(channel_int)
    except Exception:
        return None


async def _resolve_level_metadata(payload: dict[str, Any], level_id: int) -> tuple[str, str, bool]:
    level_name = str(payload.get("levelName") or payload.get("name") or "").strip()
    creator = str(payload.get("creator") or payload.get("levelCreator") or "").strip()
    platformer_present = "platformer" in payload or "isPlatformer" in payload
    platformer = _safe_bool(payload.get("platformer", payload.get("isPlatformer", False)))

    if level_name and creator and platformer_present:
        return level_name, creator, platformer

    try:
        info = await asyncio.to_thread(fetch_gd_level_info, str(level_id), True, 8)
    except Exception as exc:
        print(f"[SEND BRIDGE] GD metadata fallback failed for {level_id}: {exc}")
        info = {}

    if not level_name:
        level_name = str(info.get("name") or f"Level {level_id}")
    if not creator:
        creator = str(info.get("author") or "Unknown")
    if not platformer_present:
        platformer = bool(info.get("platformer"))
    return level_name, creator, platformer


async def _handle_gd_send(request: web.Request):
    try:
        payload = await request.json()
    except Exception:
        return web.json_response({"ok": False, "error": "invalid_json"}, status=400)
    if not isinstance(payload, dict):
        return web.json_response({"ok": False, "error": "invalid_payload"}, status=400)

    api_key = _extract_bearer_token(request, payload)
    config = _get_send_config_by_key(api_key)
    if config is None:
        return web.json_response({"ok": False, "error": "invalid_connection_key"}, status=401)

    server_id = config["server_id"]
    moderator_discord_id = config["user_id"]
    level_id = _safe_int(payload.get("levelId", payload.get("level_id")), 0)
    stars = _safe_int(payload.get("stars"), 0)
    feature_state = _safe_int(payload.get("featureState", payload.get("feature_state")), 0)
    event_id = str(payload.get("eventId") or payload.get("event_id") or "").strip()

    if level_id <= 0:
        return web.json_response({"ok": False, "error": "invalid_level_id"}, status=400)
    if stars < 1 or stars > 10:
        return web.json_response({"ok": False, "error": "invalid_stars"}, status=400)
    if _is_duplicate_event(server_id, moderator_discord_id, event_id):
        return web.json_response({"ok": True, "published": False, "ignored": "duplicate_event"})

    level_name, creator, platformer = await _resolve_level_metadata(payload, level_id)
    send_type = _normalize_external_send_type(payload.get("sendType"), feature_state, platformer)

    guild = bot.get_guild(_safe_int(server_id, 0))
    if guild is None:
        return web.json_response({"ok": False, "error": "discord_server_not_available"}, status=503)

    try:
        moderator = guild.get_member(int(moderator_discord_id))
        if moderator is None:
            moderator = await guild.fetch_member(int(moderator_discord_id))
    except Exception:
        moderator = None
    if moderator is None:
        return web.json_response({"ok": False, "error": "discord_moderator_not_found"}, status=400)
    if not _has_send_config_access(guild, server_id, moderator):
        _write_ingress_log(
            event_id=event_id,
            server_id=server_id,
            moderator_discord_id=moderator_discord_id,
            level_id=str(level_id),
            level_name=level_name,
            level_creator=creator,
            stars=stars,
            feature_state=feature_state,
            send_type=send_type,
            platformer=platformer,
            published=False,
            ignore_reason="moderator_access_revoked",
        )
        return web.json_response({"ok": False, "error": "moderator_access_revoked"}, status=403)

    # A real RobTop send opened from the in-game Requests browser carries an exact
    # RequestID. Treat that as an explicit moderator request result even if the normal
    # external send logger is disabled; the user intentionally opened this request in
    # the request client. The old optional RequestIntegration behavior below is kept for
    # normal sends opened outside the request browser.
    explicit_request_id = _safe_int(payload.get("requestId", payload.get("request_id")), 0)
    explicit_request_mode = str(payload.get("requestMode") or payload.get("mode") or "").strip().lower()
    feedback = str(payload.get("feedback") or payload.get("note") or "").strip()
    if len(feedback) > 1500:
        return web.json_response({"ok": False, "error": "feedback_too_long", "max": 1500}, status=400)
    if explicit_request_id > 0 and explicit_request_mode == "moderator":
        capabilities = _request_client_capabilities(guild, server_id, moderator)
        if not capabilities.get("moderator"):
            return web.json_response({"ok": False, "error": "moderator_access_revoked"}, status=403)
        try:
            explicit_result = await process_geode_moderator_request_send(
                server_id=server_id,
                event_name=str(payload.get("requestEvent") or payload.get("event") or "0"),
                request_id=explicit_request_id,
                moderator=moderator,
                send_type_raw=send_type,
                stars=stars,
                feedback=feedback,
            )
        except Exception as exc:
            print(f"[SEND BRIDGE] explicit request result failed for request {explicit_request_id}: {exc}")
            explicit_result = {"handled": False, "reason": "request_integration_exception"}
        if explicit_result.get("handled"):
            result_message = explicit_result.get("message")
            _write_ingress_log(
                event_id=event_id,
                server_id=server_id,
                moderator_discord_id=moderator_discord_id,
                level_id=str(level_id),
                level_name=level_name,
                level_creator=creator,
                stars=stars,
                feature_state=feature_state,
                send_type=send_type,
                platformer=platformer,
                published=True,
                ignore_reason="explicit_request_result",
            )
            return web.json_response({
                "ok": True,
                "published": True,
                "requestIntegrated": True,
                "requestId": explicit_request_id,
                "overwrittenRecords": int(explicit_result.get("overwritten_records") or 0),
                "messageId": str(getattr(result_message, "id", "") or ""),
            })
        return web.json_response(
            {"ok": False, "error": str(explicit_result.get("reason") or "request_result_failed")},
            status=409,
        )

    if not config["enabled"]:
        _write_ingress_log(
            event_id=event_id,
            server_id=server_id,
            moderator_discord_id=moderator_discord_id,
            level_id=str(level_id),
            level_name=level_name,
            level_creator=creator,
            stars=stars,
            feature_state=feature_state,
            send_type=send_type,
            platformer=platformer,
            published=False,
            ignore_reason="disabled",
        )
        return web.json_response({"ok": True, "published": False, "ignored": "disabled"})

    request_match = _find_latest_server_request_level(server_id, level_id)
    force_external_fallback = False
    request_integration_fallback_reason = ""

    if config["request_integration"] and request_match is not None:
        try:
            request_result = await process_geode_moderator_request_send(
                server_id=server_id,
                event_name=request_match["event"],
                request_id=request_match["request_id"],
                moderator=moderator,
                send_type_raw=send_type,
                stars=stars,
            )
        except Exception as exc:
            print(f"[SEND BRIDGE] request integration failed for level {level_id}: {exc}")
            request_result = {"handled": False, "reason": "request_integration_exception"}

        if request_result.get("handled"):
            result_message = request_result.get("message")
            _write_ingress_log(
                event_id=event_id,
                server_id=server_id,
                moderator_discord_id=moderator_discord_id,
                level_id=str(level_id),
                level_name=level_name,
                level_creator=creator,
                stars=stars,
                feature_state=feature_state,
                send_type=send_type,
                platformer=platformer,
                published=True,
                ignore_reason="request_result",
            )
            return web.json_response(
                {
                    "ok": True,
                    "published": True,
                    "requestIntegrated": True,
                    "serverId": server_id,
                    "moderatorDiscordId": moderator_discord_id,
                    "levelId": level_id,
                    "sendType": send_type,
                    "requestId": request_result.get("request_id"),
                    "event": request_result.get("event"),
                    "overwrittenRecords": int(request_result.get("overwritten_records") or 0),
                    "messageId": str(getattr(result_message, "id", "") or ""),
                }
            )

        # A matching request exists, but it could not be handled as a request result.
        # Per send-config semantics, fall back to the personal external channel and do
        # not let Only outside bot suppress this safety fallback.
        force_external_fallback = True
        request_integration_fallback_reason = str(request_result.get("reason") or "request_integration_unavailable")

    if (
        not force_external_fallback
        and config["mode"] == SEND_CONFIG_MODE_OUTSIDE_ONLY
        and request_match is not None
    ):
        _write_ingress_log(
            event_id=event_id,
            server_id=server_id,
            moderator_discord_id=moderator_discord_id,
            level_id=str(level_id),
            level_name=level_name,
            level_creator=creator,
            stars=stars,
            feature_state=feature_state,
            send_type=send_type,
            platformer=platformer,
            published=False,
            ignore_reason="level_exists_in_server_requests",
        )
        return web.json_response(
            {"ok": True, "published": False, "ignored": "level_exists_in_server_requests"}
        )

    if (
        not force_external_fallback
        and config["mode"] == SEND_CONFIG_MODE_INSIDE_ONLY
        and request_match is None
    ):
        _write_ingress_log(
            event_id=event_id,
            server_id=server_id,
            moderator_discord_id=moderator_discord_id,
            level_id=str(level_id),
            level_name=level_name,
            level_creator=creator,
            stars=stars,
            feature_state=feature_state,
            send_type=send_type,
            platformer=platformer,
            published=False,
            ignore_reason="level_not_in_server_requests",
        )
        return web.json_response(
            {"ok": True, "published": False, "ignored": "level_not_in_server_requests"}
        )

    channel = await _resolve_target_channel(guild, config["channel_id"])
    if channel is None or getattr(getattr(channel, "guild", None), "id", None) != guild.id:
        return web.json_response({"ok": False, "error": "send_channel_not_configured"}, status=409)

    reward_kind = "moon" if platformer else "star"
    base_kind = _difficulty_base_kind_from_stars(stars)
    tier_kind = _send_type_tier_kind(send_type)
    title_emoji, title_text = _compose_send_title_parts(
        send_type,
        default_emoji="<:rate_3:1290321896361164842>",
    )

    badge_file = None
    try:
        badge_file = await _build_staff_gd_rate_badge_file(
            base_kind=base_kind,
            tier_kind=tier_kind,
            reward_kind=reward_kind,
            amount=str(stars),
            filename=f"geode_send_{level_id}_{base_kind}_{tier_kind}_{reward_kind}_{stars}.png",
        )
    except Exception as exc:
        print(f"[SEND BRIDGE] badge build failed for {level_id}: {exc}")

    try:
        result_message = await _send_staff_components_message(
            channel,
            counter="",
            title_emoji=title_emoji,
            title_text=title_text,
            headline_text="A level has been sent to RobTop!",
            level_name=level_name,
            level_creator=creator,
            level_id=str(level_id),
            checker_label="Moderator",
            checker_mention=moderator.mention,
            requester_mention=None,
            note_text="",
            badge_file=badge_file,
            interaction=None,
            allow_requester_ping=False,
        )
    except Exception as exc:
        print(f"[SEND BRIDGE] Discord publish failed: {exc}")
        return web.json_response({"ok": False, "error": "discord_publish_failed"}, status=500)

    if result_message is None:
        return web.json_response({"ok": False, "error": "discord_publish_rejected"}, status=500)

    _write_ingress_log(
        event_id=event_id,
        server_id=server_id,
        moderator_discord_id=moderator_discord_id,
        level_id=str(level_id),
        level_name=level_name,
        level_creator=creator,
        stars=stars,
        feature_state=feature_state,
        send_type=send_type,
        platformer=platformer,
        published=True,
        ignore_reason="",
    )
    return web.json_response(
        {
            "ok": True,
            "published": True,
            "serverId": server_id,
            "moderatorDiscordId": moderator_discord_id,
            "levelId": level_id,
            "sendType": send_type,
            "messageId": str(getattr(result_message, "id", "") or ""),
            "requestIntegrated": False,
            "requestIntegrationFallback": request_integration_fallback_reason or None,
        }
    )


async def _handle_health(_request: web.Request):
    return web.json_response({"ok": True, "service": "gd-send-bridge"})


async def start_send_bridge() -> bool:
    """Start the small HTTP listener once. Safe to call from every on_ready."""
    global _bridge_runner, _bridge_site
    async with _bridge_start_lock:
        if _bridge_runner is not None:
            return True

        _ensure_send_tables()
        app = web.Application(client_max_size=64 * 1024)
        app.router.add_get("/api/v1/health", _handle_health)
        app.router.add_get("/api/v1/client/me", _handle_client_me)
        app.router.add_get("/api/v1/requests", _handle_requests)
        app.router.add_post("/api/v1/request-result", _handle_request_result)
        app.router.add_post("/api/v1/gd-send", _handle_gd_send)

        runner = web.AppRunner(app)
        try:
            await runner.setup()
            site = web.TCPSite(runner, SEND_BRIDGE_HOST, SEND_BRIDGE_PORT)
            await site.start()
        except Exception as exc:
            print(f"[SEND BRIDGE] failed to start on {SEND_BRIDGE_HOST}:{SEND_BRIDGE_PORT}: {exc}")
            try:
                await runner.cleanup()
            except Exception:
                pass
            return False

        _bridge_runner = runner
        _bridge_site = site
        print(f"[SEND BRIDGE] listening on {SEND_BRIDGE_HOST}:{SEND_BRIDGE_PORT}")
        if SEND_BRIDGE_PUBLIC_URL:
            print(f"[SEND BRIDGE] public URL: {SEND_BRIDGE_PUBLIC_URL}/api/v1/gd-send")
        else:
            print("[SEND BRIDGE] SEND_BRIDGE_PUBLIC_URL is not set")
        return True


__all__ = [
    "sendconfig",
    "geodelink",
    "start_send_bridge",
]
