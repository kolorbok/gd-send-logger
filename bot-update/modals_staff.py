# Imported explicit globals from previous modules (monolith compatibility)
from .core import bot
from .core import (
    discord, fileinput, time, app_commands, commands, DMChannel, os, asyncio, urllib, json, logging, random, sqlite3, psutil, tracemalloc, gc, threading, queue, log_memory_usage, get_required_permissions, check_bot_permissions, check_configs, report_configs, count_specific_values, checkvalueexists, checkvalueexists2, checkvalueexists3, getsheetparameterfromreqid, getsheetparameterfromother, getsheetparameterfromother2, getsheetlinefromreqid, writesheetline, editsheetparameterfromreqid, getbuttonparameterfrommessageid, writebuttonline, writedmuserline, getdmusersparameterfromuserid, editdmusersparameterfromuserid, deletebuttonlinefrommessageid
)
from .core import (
    editbuttonparameterfrommessageid, getreactionhelperparameterfromuserid, writereactionhelperline, editreactionhelperparameterfromuserid, getqueueparameterfromuserid, getqueueparameterfromrequesterid, writequeueline, editqueueparameterfromuserid, editcooldownparameterfromuserid, getcooldownparameterfromuserid, writecooldownline, getserverparameterfromserverid, writeserverline, editserverparameterfromserverid, getrowcount, get_max_request_id, memory_cleanup_task, auto_close_requests, AddReaction, LocateID, append, write, read, exists, stats_embed, request_embed, request_embed_2, can_dm_user, safe_send, queue_send, safe_channel_send, safe_channel_edit, safe_dm_send, can_send_in_channel, ensure_send_permissions, warn_send_permission_problem, payload_uses_external_custom_emojis, can_add_reactions, ensure_reaction_permissions, can_create_threads, ensure_thread_permissions, can_send_embeds, ensure_embed_permissions, normalize_event_name, appendcandidaterequestvote, closecandidaterequest, candidate_request_embed, write_sheet_from_candidate, editcandidaterequestparameterfromid, getcandidaterequestlinefromid, getcandidaterequestparameterfromid, NOT_SEND_REASON_EMOJI_MAP, member_has_role_id, resolve_guild_member
)
from .modals_requests import (
    RequestModal
)


# Lazy shim to avoid circular import (ui_views imports modals_*)
# and to keep IDE/runtime references to ReactionView valid in this module.
from typing import TYPE_CHECKING
from pathlib import Path
import tempfile
import re
from PIL import Image
if TYPE_CHECKING:
    from .ui_views import ReactionView as ReactionView
else:
    def ReactionView(*args, **kwargs):
        from .ui_views import ReactionView as _ReactionView
        return _ReactionView(*args, **kwargs)


# Sheet table column indexes (see core.py CREATE TABLE Sheet)
_SHEET_IDX_LEVEL_DIFFICULTY = 5
_SHEET_IDX_IS_PLATFORMER = 10
_SHEET_IDX_REVIEW_LANGUAGE = 11
_SHEET_IDX_SERVER_ID = 15
_SHEET_IDX_HELPERS_SEND = 16
_SHEET_IDX_HELPERS_NOT_SEND = 17
_SHEET_IDX_REVIEWERS = 18
_SHEET_IDX_SENT_TO = 19
_SHEET_IDX_IS_RATED = 20
_SHEET_IDX_MODERATORS_SEND = 21
_SHEET_IDX_MODERATORS_NOT_SEND = 22
_SHEET_IDX_TOTAL_SENDS = 24
_SHEET_IDX_REVIEW_REQUESTED = 25
_SHEET_IDX_EVENT = 26
_SHEET_IDX_HELPER_STAR_RATE_SENDS = 27
_SHEET_IDX_HELPER_FEATURED_SENDS = 28
_SHEET_IDX_HELPER_EPIC_SENDS = 29
_SHEET_IDX_HELPER_LEGENDARY_SENDS = 30
_SHEET_IDX_HELPER_MYTHIC_SENDS = 31
_SHEET_IDX_MODERATOR_STAR_RATE_SENDS = 32
_SHEET_IDX_MODERATOR_FEATURED_SENDS = 33
_SHEET_IDX_MODERATOR_EPIC_SENDS = 34
_SHEET_IDX_MODERATOR_LEGENDARY_SENDS = 35
_SHEET_IDX_MODERATOR_MYTHIC_SENDS = 36
_SHEET_IDX_HELPER_ALREADY_RATED_NOT_SENDS = 37
_SHEET_IDX_HELPER_ALREADY_SEEN_NOT_SENDS = 38
_SHEET_IDX_HELPER_WRONG_ID_NOT_SENDS = 39
_SHEET_IDX_HELPER_REPORT_NOT_SENDS = 40
_SHEET_IDX_MODERATOR_ALREADY_RATED_NOT_SENDS = 41
_SHEET_IDX_MODERATOR_ALREADY_SEEN_NOT_SENDS = 42
_SHEET_IDX_MODERATOR_WRONG_ID_NOT_SENDS = 43
_SHEET_IDX_MODERATOR_REPORT_NOT_SENDS = 44

_SEND_TYPE_COLUMNS_BY_GROUP = {
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

_SEND_TYPE_INDEXES_BY_GROUP = {
    "helper": {
        "star_rate": _SHEET_IDX_HELPER_STAR_RATE_SENDS,
        "featured": _SHEET_IDX_HELPER_FEATURED_SENDS,
        "epic": _SHEET_IDX_HELPER_EPIC_SENDS,
        "legendary": _SHEET_IDX_HELPER_LEGENDARY_SENDS,
        "mythic": _SHEET_IDX_HELPER_MYTHIC_SENDS,
    },
    "moderator": {
        "star_rate": _SHEET_IDX_MODERATOR_STAR_RATE_SENDS,
        "featured": _SHEET_IDX_MODERATOR_FEATURED_SENDS,
        "epic": _SHEET_IDX_MODERATOR_EPIC_SENDS,
        "legendary": _SHEET_IDX_MODERATOR_LEGENDARY_SENDS,
        "mythic": _SHEET_IDX_MODERATOR_MYTHIC_SENDS,
    },
}



_NOT_SEND_REASON_COLUMNS_BY_GROUP = {
    "helper": {
        "already_rated": "HelperAlreadyRatedNotSends",
        "already_seen": "HelperAlreadySeenNotSends",
        "wrong_id": "HelperWrongIDNotSends",
        "report": "HelperReportNotSends",
    },
    "moderator": {
        "already_rated": "ModeratorAlreadyRatedNotSends",
        "already_seen": "ModeratorAlreadySeenNotSends",
        "wrong_id": "ModeratorWrongIDNotSends",
        "report": "ModeratorReportNotSends",
    },
}

_NOT_SEND_REASON_INDEXES_BY_GROUP = {
    "helper": {
        "already_rated": _SHEET_IDX_HELPER_ALREADY_RATED_NOT_SENDS,
        "already_seen": _SHEET_IDX_HELPER_ALREADY_SEEN_NOT_SENDS,
        "wrong_id": _SHEET_IDX_HELPER_WRONG_ID_NOT_SENDS,
        "report": _SHEET_IDX_HELPER_REPORT_NOT_SENDS,
    },
    "moderator": {
        "already_rated": _SHEET_IDX_MODERATOR_ALREADY_RATED_NOT_SENDS,
        "already_seen": _SHEET_IDX_MODERATOR_ALREADY_SEEN_NOT_SENDS,
        "wrong_id": _SHEET_IDX_MODERATOR_WRONG_ID_NOT_SENDS,
        "report": _SHEET_IDX_MODERATOR_REPORT_NOT_SENDS,
    },
}

_NOT_SEND_REASON_DISPLAY_MAP = {
    "already_rated": "Already Rated",
    "already_seen": "Already Seen",
    "wrong_id": "Wrong ID",
    "report": "Report",
}

_NOT_SEND_REASON_EMOJI_MAP = NOT_SEND_REASON_EMOJI_MAP

def _safe_str(val) -> str:
    return "" if val is None else str(val)


def _row_value(row, index: int, default: str = "") -> str:
    if row is None or len(row) <= index:
        return default
    value = row[index]
    return default if value is None else str(value)


def _button_value(message_id, column_name: str, default=""):
    if message_id in (None, "", 0, "0"):
        return default
    value = getbuttonparameterfrommessageid(str(message_id), column_name)
    if value in (None, ""):
        return default
    return value


def _normalize_platformer_filter(raw_value) -> str:
    value = str(raw_value or "").strip().lower()
    if value in {"platformer", "yes", "true", "1", "moon", "moons"}:
        return "platformer"
    if value in {"classic", "non_platformer", "non-platformer", "no", "false", "0", "stars", "star"}:
        return "classic"
    return ""


def _normalize_queue_send_type_filter(raw_value) -> str:
    value = str(raw_value or "").strip().lower()
    aliases = {
        "": "",
        "all": "",
        "none": "",
        "rate": "star_rate",
        "star": "star_rate",
        "star rate": "star_rate",
        "star-rate": "star_rate",
        "starrate": "star_rate",
        "star_rate": "star_rate",
        "moon": "star_rate",
        "moon rate": "star_rate",
        "moon-rate": "star_rate",
        "moonrate": "star_rate",
        "moon_rate": "star_rate",
        "feature": "featured",
        "featured": "featured",
        "feat": "featured",
        "epic": "epic",
        "legendary": "legendary",
        "mythic": "mythic",
    }
    return aliases.get(value, value if value in {"featured", "epic", "legendary", "mythic"} else "")


def _iter_queue_send_type_filter_parts(raw_value):
    if raw_value is None:
        return []
    if isinstance(raw_value, (list, tuple, set)):
        parts = []
        for item in raw_value:
            parts.extend(_iter_queue_send_type_filter_parts(item))
        return parts

    text = str(raw_value or "").strip()
    if not text:
        return []

    for separator in (";", "|"):
        text = text.replace(separator, ",")
    return [part.strip() for part in text.split(",") if part.strip()]


def _parse_queue_send_type_filters(*raw_values) -> list[str]:
    normalized_values = []
    seen = set()
    for raw_value in raw_values:
        for part in _iter_queue_send_type_filter_parts(raw_value):
            normalized = _normalize_queue_send_type_filter(part)
            if not normalized or normalized in seen:
                continue
            normalized_values.append(normalized)
            seen.add(normalized)
    return normalized_values


def _serialize_queue_send_type_filters(*raw_values) -> str:
    return ",".join(_parse_queue_send_type_filters(*raw_values))


def _normalize_difficulty_filter(raw_value) -> str:
    value = str(raw_value or "").strip().lower()
    aliases = {
        "": "",
        "all": "",
        "1": "auto",
        "auto": "auto",
        "2": "easy",
        "easy": "easy",
        "3": "normal",
        "normal": "normal",
        "4": "hard-4",
        "hard-4": "hard-4",
        "5": "hard-5",
        "hard-5": "hard-5",
        "6": "harder-6",
        "harder-6": "harder-6",
        "7": "harder-7",
        "harder-7": "harder-7",
        "8": "insane-8",
        "insane-8": "insane-8",
        "9": "insane-9",
        "insane-9": "insane-9",
        "easy demon": "demon-easy",
        "easy-demon": "demon-easy",
        "demon-easy": "demon-easy",
        "medium demon": "demon-medium",
        "medium-demon": "demon-medium",
        "demon-medium": "demon-medium",
        "hard demon": "demon-hard",
        "hard-demon": "demon-hard",
        "demon-hard": "demon-hard",
        "demon": "demon-hard",
        "insane demon": "demon-insane",
        "insane-demon": "demon-insane",
        "demon-insane": "demon-insane",
        "extreme demon": "demon-extreme",
        "extreme-demon": "demon-extreme",
        "demon-extreme": "demon-extreme",
    }
    return aliases.get(value, value if value in set(aliases.values()) else "")


def _difficulty_filter_matches(level_difficulty: str, difficulty_filter: str) -> bool:
    difficulty_filter = _normalize_difficulty_filter(difficulty_filter)
    if not difficulty_filter:
        return True
    text = str(level_difficulty or "").lower()
    first_token = text.split(maxsplit=1)[0] if text.split() else ""
    if difficulty_filter == "auto":
        return first_token == "1"
    if difficulty_filter == "easy":
        return first_token == "2"
    if difficulty_filter == "normal":
        return first_token == "3"
    if difficulty_filter == "hard-4":
        return first_token == "4"
    if difficulty_filter == "hard-5":
        return first_token == "5"
    if difficulty_filter == "harder-6":
        return first_token == "6"
    if difficulty_filter == "harder-7":
        return first_token == "7"
    if difficulty_filter == "insane-8":
        return first_token == "8"
    if difficulty_filter == "insane-9":
        return first_token == "9"
    if first_token != "10":
        return False
    if difficulty_filter == "demon-easy":
        return "easy demon" in text
    if difficulty_filter == "demon-medium":
        return "medium demon" in text
    if difficulty_filter == "demon-insane":
        return "insane demon" in text
    if difficulty_filter == "demon-extreme":
        return "extreme demon" in text
    if difficulty_filter == "demon-hard":
        return ("hard demon" in text) or ("demon" in text and not any(part in text for part in ("easy demon", "medium demon", "insane demon", "extreme demon")))
    return True


def _platformer_filter_matches(is_platformer: str, platformer_filter: str) -> bool:
    platformer_filter = _normalize_platformer_filter(platformer_filter)
    if not platformer_filter:
        return True
    is_platformer_bool = str(is_platformer or "").strip().lower() in {"yes", "true", "1", "platformer", "moon", "moons"}
    return is_platformer_bool if platformer_filter == "platformer" else not is_platformer_bool


def _safe_optional_int(value, default=None):
    if value is None or value == "":
        return default
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _senddb_filter_matches(total_sends: str, min_senddb=None, max_senddb=None) -> bool:
    min_value = _safe_optional_int(min_senddb)
    max_value = _safe_optional_int(max_senddb)
    if min_value is None and max_value is None:
        return True
    total_value = _safe_optional_int(total_sends)
    if total_value is None or total_value < 0:
        return False
    if min_value is not None and total_value < min_value:
        return False
    if max_value is not None and total_value > max_value:
        return False
    return True



def _count_csv_entries(raw_value) -> int:
    return len([part.strip() for part in str(raw_value or "").split(",") if part.strip()])


def _max_sent_to_filter_matches(sent_to_value: str, max_sent_to=None) -> bool:
    max_value = _safe_optional_int(max_sent_to)
    if max_value is None or max_value < 0:
        return True
    return _count_csv_entries(sent_to_value) <= max_value


def _send_type_storage_value(raw_value: str | None, *, default: str = "") -> str:
    normalized = _normalize_send_type_value(raw_value)
    if normalized in ("", "none"):
        return str(default or "").strip().lower()
    if normalized == "moon_rate":
        return "star_rate"
    if normalized in {"star_rate", "featured", "epic", "legendary", "mythic"}:
        return normalized
    return str(default or "").strip().lower()


def _default_send_type_for_request_row(row) -> str:
    return "moon_rate" if _row_value(row, _SHEET_IDX_IS_PLATFORMER).strip().lower() == "yes" else "star_rate"


def _history_contains_send_type(history_value: str, send_type_filter: str, *, fallback_star_rate: bool = False) -> bool:
    send_type_filters = _parse_queue_send_type_filters(send_type_filter)
    if not send_type_filters:
        return True
    text = str(history_value or "").lower()
    if not text.strip() and fallback_star_rate:
        text = "star_rate"
    for normalized_send_type in send_type_filters:
        if normalized_send_type == "star_rate":
            if any(token in text for token in ("star_rate", "star rate", "moon_rate", "moon rate", "rate")):
                return True
        elif normalized_send_type in text:
            return True
    return False


def _queue_filters_match_row(row, *, platformer_filter="", difficulty_filter="", min_senddb=None, max_senddb=None, max_sent_to=None, send_type_filter="", send_type_index=None, fallback_star_rate=False) -> bool:
    if not _platformer_filter_matches(_row_value(row, _SHEET_IDX_IS_PLATFORMER), platformer_filter):
        return False
    if not _senddb_filter_matches(_row_value(row, _SHEET_IDX_TOTAL_SENDS, "-1"), min_senddb, max_senddb):
        return False
    if not _max_sent_to_filter_matches(_row_value(row, _SHEET_IDX_SENT_TO), max_sent_to):
        return False
    if send_type_filter:
        history_value = _row_value(row, send_type_index) if send_type_index is not None else ""
        if not _history_contains_send_type(history_value, send_type_filter, fallback_star_rate=fallback_star_rate):
            return False
    return True


def _get_button_queue_filters(message_id) -> dict:
    min_senddb = _safe_optional_int(_button_value(message_id, "MinSendDB", -1), -1)
    max_senddb = _safe_optional_int(_button_value(message_id, "MaxSendDB", -1), -1)
    max_sent_to = _safe_optional_int(_button_value(message_id, "MaxSentTo", -1), -1)
    return {
        "platformer_filter": _normalize_platformer_filter(_button_value(message_id, "PlatformerFilter", "")),
        "difficulty_filter": "",
        "send_type_filter": _serialize_queue_send_type_filters(_button_value(message_id, "SendTypeFilter", "")),
        "min_senddb": None if min_senddb is None or min_senddb < 0 else min_senddb,
        "max_senddb": None if max_senddb is None or max_senddb < 0 else max_senddb,
        "max_sent_to": None if max_sent_to is None or max_sent_to < 0 else max_sent_to,
    }


def _append_unique_csv(existing_value, value_to_add) -> str:
    value = str(value_to_add or "").strip()
    if not value:
        return str(existing_value or "").strip()
    parts = [part.strip() for part in str(existing_value or "").split(",") if part.strip()]
    if value not in parts:
        parts.append(value)
    return ", ".join(parts)


def _legacy_send_actor_column(group: str) -> str:
    return "ModeratorsSend" if str(group or "").strip().lower() == "moderator" else "HelpersSend"


def _send_type_actor_column(group: str, send_type: str) -> str:
    normalized_group = "moderator" if str(group or "").strip().lower() == "moderator" else "helper"
    normalized_type = _send_type_storage_value(send_type, default="")
    if not normalized_type:
        return _legacy_send_actor_column(normalized_group)
    return _SEND_TYPE_COLUMNS_BY_GROUP.get(normalized_group, _SEND_TYPE_COLUMNS_BY_GROUP["helper"]).get(
        normalized_type,
        _legacy_send_actor_column(normalized_group),
    )


def _append_sheet_send_type_actor(request_id: int, group: str, actor: str, send_type: str):
    actor = str(actor or "").strip()
    if not actor:
        return
    column_name = _send_type_actor_column(group, send_type)
    try:
        current = getsheetparameterfromreqid(request_id, column_name)
        new_text = _append_unique_csv(current, actor)
        if new_text != str(current or "").strip():
            editsheetparameterfromreqid(request_id, column_name, new_text)
    except Exception as e:
        print(f"[SEND TYPE] failed to update {column_name} for request {request_id}: {e}")



def _normalize_not_send_reason_value(raw_value: str | None) -> str:
    value = "" if raw_value is None else str(raw_value).strip().lower()
    if value in ("", "none", "unknown", "na", "n/a"):
        return ""
    aliases = {
        "already rated": "already_rated",
        "already-rated": "already_rated",
        "already_rated": "already_rated",
        "rated": "already_rated",
        "gg": "already_rated",
        "гг": "already_rated",
        "already seen": "already_seen",
        "already-seen": "already_seen",
        "already_seen": "already_seen",
        "seen": "already_seen",
        "wrong id": "wrong_id",
        "wrong-id": "wrong_id",
        "wrong_id": "wrong_id",
        "id": "wrong_id",
        "report": "report",
        "reported": "report",
    }
    return aliases.get(value, value if value in _NOT_SEND_REASON_DISPLAY_MAP else "")


def _not_send_reason_display(raw_value: str | None) -> str:
    normalized = _normalize_not_send_reason_value(raw_value)
    return _NOT_SEND_REASON_DISPLAY_MAP.get(normalized, str(raw_value or "").strip())


def _not_send_reason_emoji(raw_value: str | None) -> str:
    normalized = _normalize_not_send_reason_value(raw_value)
    return _NOT_SEND_REASON_EMOJI_MAP.get(normalized, "<:not_sent:1155722772367028244>")


def _not_send_reason_actor_column(group: str, reason: str) -> str:
    normalized_reason = _normalize_not_send_reason_value(reason)
    return _NOT_SEND_REASON_COLUMNS_BY_GROUP.get(group, {}).get(normalized_reason, "HelpersNotSend" if group == "helper" else "ModeratorsNotSend")


def _append_sheet_not_send_reason_actor(request_id: int, group: str, actor: str, reason: str):
    actor = str(actor or "").strip()
    if not actor:
        return
    column_name = _not_send_reason_actor_column(group, reason)
    try:
        current = getsheetparameterfromreqid(request_id, column_name)
        new_text = _append_unique_csv(current, actor)
        if new_text != str(current or "").strip():
            editsheetparameterfromreqid(request_id, column_name, new_text)
    except Exception as e:
        print(f"[NOT SEND REASON] failed to update {column_name} for request {request_id}: {e}")


_MODAL_OVERWRITE_RECORD_COLUMNS = {
    "helper": (
        "HelpersSend",
        "HelperStarRateSends",
        "HelperFeaturedSends",
        "HelperEpicSends",
        "HelperLegendarySends",
        "HelperMythicSends",
        "HelpersNotSend",
        "HelperAlreadyRatedNotSends",
        "HelperAlreadySeenNotSends",
        "HelperWrongIDNotSends",
        "HelperReportNotSends",
    ),
    "moderator": (
        "ModeratorsSend",
        "ModeratorStarRateSends",
        "ModeratorFeaturedSends",
        "ModeratorEpicSends",
        "ModeratorLegendarySends",
        "ModeratorMythicSends",
        "ModeratorsNotSend",
        "ModeratorAlreadyRatedNotSends",
        "ModeratorAlreadySeenNotSends",
        "ModeratorWrongIDNotSends",
        "ModeratorReportNotSends",
    ),
}


def _modal_cleanup_parts(value) -> list[str]:
    return [part.strip() for part in str(value or "").split(",") if part.strip()]


def _modal_cleanup_actor_tokens(user) -> set[str]:
    user_id = str(getattr(user, "id", "") or "").strip()
    mention = str(getattr(user, "mention", "") or "").strip()
    return {token for token in (mention, f"<@{user_id}>" if user_id else "", f"<@!{user_id}>" if user_id else "") if token}


def _modal_cleanup_sent_to_tokens(user) -> set[str]:
    tokens = set(_modal_cleanup_actor_tokens(user))
    for attr_name in ("name", "display_name", "global_name"):
        value = str(getattr(user, attr_name, "") or "").strip()
        if value:
            tokens.add(value)
    return tokens


def _modal_remove_tokens(value, tokens: set[str], *, casefold: bool = False) -> str:
    if not tokens:
        return str(value or "").strip()
    if casefold:
        normalized_tokens = {str(token or "").casefold() for token in tokens if str(token or "").strip()}
        return ", ".join(part for part in _modal_cleanup_parts(value) if part.casefold() not in normalized_tokens)
    return ", ".join(part for part in _modal_cleanup_parts(value) if part not in tokens)


def _clear_existing_staff_records_on_success(request_id: int, user, staff_kind: str = "all") -> int:
    """Clear previous user records only after overwrite modal submission succeeds.

    The warning view must not delete records before opening a modal. A user can close a
    modal or hit a validation/send error, and then their old record would be gone. This
    helper is called from on_submit after the result message was sent and immediately
    before the new DB record is written.
    """
    normalized_kind = str(staff_kind or "all").strip().lower()
    if normalized_kind not in _MODAL_OVERWRITE_RECORD_COLUMNS:
        columns = []
        for group_columns in _MODAL_OVERWRITE_RECORD_COLUMNS.values():
            columns.extend(group_columns)
    else:
        columns = list(_MODAL_OVERWRITE_RECORD_COLUMNS[normalized_kind])

    tokens = _modal_cleanup_actor_tokens(user)
    changed = 0
    for column_name in columns:
        try:
            current = getsheetparameterfromreqid(int(request_id), column_name)
            new_value = _modal_remove_tokens(current, tokens)
            if new_value != str(current or "").strip():
                editsheetparameterfromreqid(int(request_id), column_name, new_value)
                changed += 1
        except Exception as e:
            print(f"[OVERWRITE CLEANUP] failed to clear {column_name} for request {request_id}: {e}")

    # SentTo is only helper -> moderator routing. Clear it only for moderator overwrites,
    # matching the Delete my records button behavior.
    if normalized_kind == "moderator":
        try:
            current = getsheetparameterfromreqid(int(request_id), "SentTo")
            new_value = _modal_remove_tokens(current, _modal_cleanup_sent_to_tokens(user), casefold=True)
            if new_value != str(current or "").strip():
                editsheetparameterfromreqid(int(request_id), "SentTo", new_value)
                changed += 1
        except Exception as e:
            print(f"[OVERWRITE CLEANUP] failed to clear SentTo for request {request_id}: {e}")

    return changed


def _maybe_clear_existing_records_on_success(modal, request_id: int, user):
    if not bool(getattr(modal, "clear_existing_records_on_submit", False)):
        return 0
    return _clear_existing_staff_records_on_success(
        int(request_id),
        user,
        getattr(modal, "clear_existing_records_staff_kind", getattr(modal, "staff_kind", "all")),
    )


def _mention_requester_enabled(raw_value: str | None) -> bool:
    value = str(raw_value or "").strip().lower()
    return value not in {"no", "false", "0", "off", "disable", "disabled", "no_ping", "no ping", "do not ping requester"}


def _coerce_modal_checkbox_bool(value):
    if isinstance(value, bool):
        return value
    if value is None:
        return None
    text = str(value).strip().lower()
    if text in {"true", "1", "yes", "y", "on", "checked", "selected"}:
        return True
    if text in {"false", "0", "no", "n", "off", "unchecked", "unselected", ""}:
        return False
    return None


def _extract_modal_checkbox_value(interaction: discord.Interaction, custom_id: str, checkbox_obj=None) -> bool:
    """Return True when the modal checkbox was checked.

    discord.py 2.7 exposes Checkbox.value, but this also walks raw interaction
    payloads so it keeps working if the library shape changes slightly.
    """
    try:
        value = getattr(checkbox_obj, "value", None)
        coerced = _coerce_modal_checkbox_bool(value)
        if coerced is not None:
            return coerced
    except Exception:
        pass

    def _walk(component_payload):
        if isinstance(component_payload, list):
            for item in component_payload:
                found = _walk(item)
                if found is not None:
                    return found
            return None

        if not isinstance(component_payload, dict):
            return None

        if component_payload.get("custom_id") == custom_id:
            for key in ("value", "checked", "selected", "default"):
                coerced = _coerce_modal_checkbox_bool(component_payload.get(key))
                if coerced is not None:
                    return coerced
            return False

        for nested_key in ("components", "component", "children"):
            found = _walk(component_payload.get(nested_key))
            if found is not None:
                return found
        return None

    interaction_data = getattr(interaction, "data", None)
    if isinstance(interaction_data, dict):
        found = _walk(interaction_data.get("components"))
        if found is not None:
            return bool(found)
    return False

def _row_contains_actor(row, group: str, send_type: str, actor: str) -> bool:
    actor_text = str(actor or "").strip()
    if not actor_text:
        return False
    normalized_type = _send_type_storage_value(send_type, default="")
    if not normalized_type:
        legacy_index = _SHEET_IDX_HELPERS_SEND if group == "helper" else _SHEET_IDX_MODERATORS_SEND if group == "moderator" else None
        return legacy_index is not None and actor_text in _row_value(row, legacy_index)
    index = _SEND_TYPE_INDEXES_BY_GROUP.get(group, {}).get(normalized_type)
    if index is None:
        return False
    return actor_text in _row_value(row, index)


def _row_has_send_type(row, group: str, send_type: str) -> bool:
    normalized_type = _send_type_storage_value(send_type, default="")
    if not normalized_type:
        legacy_index = _SHEET_IDX_HELPERS_SEND if group == "helper" else _SHEET_IDX_MODERATORS_SEND if group == "moderator" else None
        return legacy_index is not None and bool(_row_value(row, legacy_index).strip())
    index = _SEND_TYPE_INDEXES_BY_GROUP.get(group, {}).get(normalized_type)
    if index is None:
        return False
    return bool(_row_value(row, index).strip())




def _row_typed_actor_text(row, group: str, *, include_legacy: bool = True) -> str:
    values = []
    for send_type in ("star_rate", "featured", "epic", "legendary", "mythic"):
        index = _SEND_TYPE_INDEXES_BY_GROUP.get(group, {}).get(send_type)
        if index is not None:
            values.append(_row_value(row, index))
    if include_legacy:
        legacy_index = _SHEET_IDX_HELPERS_SEND if group == "helper" else _SHEET_IDX_MODERATORS_SEND if group == "moderator" else None
        if legacy_index is not None:
            values.append(_row_value(row, legacy_index))
    parts = []
    seen = set()
    for value in values:
        for part in str(value or "").split(","):
            part = part.strip()
            if not part or part in seen:
                continue
            parts.append(part)
            seen.add(part)
    return ", ".join(parts)


def _row_actor_in_any_typed_sends(row, group: str, actor: str) -> bool:
    actor_text = str(actor or "").strip()
    if not actor_text:
        return False
    combined = _row_typed_actor_text(row, group, include_legacy=True)
    return actor_text in combined



def _row_actor_in_any_not_send_reasons(row, group: str, actor: str) -> bool:
    actor_text = str(actor or "").strip()
    if not actor_text:
        return False
    for index in _NOT_SEND_REASON_INDEXES_BY_GROUP.get(group, {}).values():
        if index is not None and len(row) > index and actor_text in _row_value(row, index):
            return True
    return False

def _row_has_any_typed_sends(row, group: str) -> bool:
    return bool(_row_typed_actor_text(row, group, include_legacy=True).strip())

def _first_send_type_from_columns(request_id: int, group: str = "helper", *, default: str = "star_rate") -> str:
    for send_type in ("star_rate", "featured", "epic", "legendary", "mythic"):
        column_name = _SEND_TYPE_COLUMNS_BY_GROUP.get(group, {}).get(send_type)
        if not column_name:
            continue
        try:
            if str(getsheetparameterfromreqid(request_id, column_name) or "").strip():
                return send_type
        except Exception:
            continue
    return default


def _first_history_send_type(history_value: str, *, default: str = "star_rate") -> str:
    text = str(history_value or "").strip()
    if not text:
        return default
    first_entry = text.split(",", 1)[0].strip()
    if ":" in first_entry:
        first_entry = first_entry.rsplit(":", 1)[-1].strip()
    return _send_type_storage_value(first_entry, default=default)


def _first_history_difficulty(history_value: str, *, default: str = "unrated") -> str:
    return default

_MODERATOR_QUEUE_MODE_VALUES = {11, 13, 14, 15}
_HELPER_QUEUE_MODE_VALUES = {1, 7}

def _is_moderator_queue_mode_value(mode_value) -> bool:
    try:
        normalized_mode = int(mode_value)
    except (TypeError, ValueError):
        normalized_mode = mode_value
    return normalized_mode in _MODERATOR_QUEUE_MODE_VALUES

def _is_helper_queue_mode_value(mode_value) -> bool:
    try:
        normalized_mode = int(mode_value)
    except (TypeError, ValueError):
        normalized_mode = mode_value
    return normalized_mode in _HELPER_QUEUE_MODE_VALUES


def _normalize_min_request_id(min_request_id) -> int:
    try:
        normalized_min_request_id = int(min_request_id)
    except (TypeError, ValueError):
        normalized_min_request_id = 0
    return max(0, normalized_min_request_id)


def _get_button_min_request_id(message_id) -> int:
    if message_id in (None, "", 0, "0"):
        return 0
    return _normalize_min_request_id(getbuttonparameterfrommessageid(str(message_id), "MinRequestID"))


def _staff_has_components_v2() -> bool:
    return all(
        hasattr(discord.ui, attr)
        for attr in ("LayoutView", "Container", "Section", "TextDisplay", "Separator", "Thumbnail")
    )


def _normalize_platformer_flag(value) -> bool:
    normalized = str(value or "").strip().lower()
    return normalized in {"yes", "true", "1", "platformer", "moon", "moons"}


_STAFF_SEND_TYPE_OPTIONS = [
    discord.SelectOption(label="Star Rate", value="star_rate", emoji="<:rate_3:1290321896361164842>"),
    discord.SelectOption(label="Featured", value="featured", emoji="<:feature_3:1290320401146052752>"),
    discord.SelectOption(label="Epic", value="epic", emoji="<:epic_3:1290315599058309213>"),
    discord.SelectOption(label="Legendary", value="legendary", emoji="<:legendary_3:1290315669858156617>"),
    discord.SelectOption(label="Mythic", value="mythic", emoji="<:mythic_3:1290315801588666410>"),
]

_STAFF_SEND_TYPE_OPTIONS_MOON = [
    discord.SelectOption(label="Moon Rate", value="moon_rate", emoji="<:rate_3:1290321896361164842>"),
    discord.SelectOption(label="Featured", value="featured", emoji="<:feature_3:1290320401146052752>"),
    discord.SelectOption(label="Epic", value="epic", emoji="<:epic_3:1290315599058309213>"),
    discord.SelectOption(label="Legendary", value="legendary", emoji="<:legendary_3:1290315669858156617>"),
    discord.SelectOption(label="Mythic", value="mythic", emoji="<:mythic_3:1290315801588666410>"),
]

_HELPER_DIFFICULTY_OPTIONS = [
    discord.SelectOption(label="Unrated / NA", value="unrated", emoji="<:na:1445455241394006047>"),
    discord.SelectOption(label="Auto, 1 Star/Moon", value="auto", emoji="<:auto:1445455662296465581>"),
    discord.SelectOption(label="Easy, 2 Stars/Moons", value="easy", emoji="<:easy:1445455093519356106>"),
    discord.SelectOption(label="Normal, 3 Stars/Moons", value="normal", emoji="<:normal:1445455112028950579>"),
    discord.SelectOption(label="Hard, 4 Stars/Moons", value="hard-4", emoji="<:hard:1445455132287565885>"),
    discord.SelectOption(label="Hard, 5 Stars/Moons", value="hard-5", emoji="<:hard:1445455132287565885>"),
    discord.SelectOption(label="Harder, 6 Stars/Moons", value="harder-6", emoji="<:harder:1445455157658914896>"),
    discord.SelectOption(label="Harder, 7 Stars/Moons", value="harder-7", emoji="<:harder:1445455157658914896>"),
    discord.SelectOption(label="Insane, 8 Stars/Moons", value="insane-8", emoji="<:insane:1445455184661708920>"),
    discord.SelectOption(label="Insane, 9 Stars/Moons", value="insane-9", emoji="<:insane:1445455184661708920>"),
    discord.SelectOption(label="Easy Demon, 10 Stars/Moons", value="demon-easy", emoji="<:demoneasy:1475778974268391526>"),
    discord.SelectOption(label="Medium Demon, 10 Stars/Moons", value="demon-medium", emoji="<:demonmedium:1475779000529064058>"),
    discord.SelectOption(label="Hard Demon, 10 Stars/Moons", value="demon-hard", emoji="<:demon:1445455210591027344>"),
    discord.SelectOption(label="Insane Demon, 10 Stars/Moons", value="demon-insane", emoji="<:demoninsane:1475779034750124153>"),
    discord.SelectOption(label="Extreme Demon, 10 Stars/Moons", value="demon-extreme", emoji="<:demonextreme:1475779202035744871>"),
]


_MODERATOR_DIFFICULTY_OPTIONS = [
    discord.SelectOption(label="Unrated / NA", value="unrated", emoji="<:na:1445455241394006047>"),
    discord.SelectOption(label="Auto, 1 Star/Moon", value="auto", emoji="<:auto:1445455662296465581>"),
    discord.SelectOption(label="Easy, 2 Stars/Moons", value="easy", emoji="<:easy:1445455093519356106>"),
    discord.SelectOption(label="Normal, 3 Stars/Moons", value="normal", emoji="<:normal:1445455112028950579>"),
    discord.SelectOption(label="Hard, 4 Stars/Moons", value="hard-4", emoji="<:hard:1445455132287565885>"),
    discord.SelectOption(label="Hard, 5 Stars/Moons", value="hard-5", emoji="<:hard:1445455132287565885>"),
    discord.SelectOption(label="Harder, 6 Stars/Moons", value="harder-6", emoji="<:harder:1445455157658914896>"),
    discord.SelectOption(label="Harder, 7 Stars/Moons", value="harder-7", emoji="<:harder:1445455157658914896>"),
    discord.SelectOption(label="Insane, 8 Stars/Moons", value="insane-8", emoji="<:insane:1445455184661708920>"),
    discord.SelectOption(label="Insane, 9 Stars/Moons", value="insane-9", emoji="<:insane:1445455184661708920>"),
    discord.SelectOption(label="Demon, 10 Stars/Moons", value="demon", emoji="<:demon:1445455210591027344>"),
]



_NOT_SEND_REASON_OPTIONS = [
    discord.SelectOption(label="Already Rated", value="already_rated", emoji=_NOT_SEND_REASON_EMOJI_MAP["already_rated"]),
    discord.SelectOption(label="Already Seen", value="already_seen", emoji=_NOT_SEND_REASON_EMOJI_MAP["already_seen"]),
    discord.SelectOption(label="Wrong ID", value="wrong_id", emoji=_NOT_SEND_REASON_EMOJI_MAP["wrong_id"]),
    discord.SelectOption(label="Report", value="report", emoji=_NOT_SEND_REASON_EMOJI_MAP["report"]),
]

_PING_CONTROL_OPTIONS = [
    discord.SelectOption(label="Mention requester", value="mention", emoji="✅"),
    discord.SelectOption(label="Do not ping requester", value="no_ping", emoji="✅"),
]

_GDMOD_DIFFICULTY_OPTIONS = [
    discord.SelectOption(label="Unrated / NA", value="unrated", emoji="<:na:1445455241394006047>"),
    discord.SelectOption(label="Hard, 4-5 Stars/Moons", value="hard", emoji="<:hard:1445455132287565885>"),
    discord.SelectOption(label="Harder, 6-7 Stars/Moons", value="harder", emoji="<:harder:1445455157658914896>"),
    discord.SelectOption(label="Insane, 8-9 Stars/Moons", value="insane", emoji="<:insane:1445455184661708920>"),
    discord.SelectOption(label="Demon, 10 Stars/Moons", value="demon", emoji="<:demon:1445455210591027344>"),
]

_SEND_TYPE_DISPLAY_MAP_ALL = {
    "": "",
    "star_rate": "Star Rate",
    "moon_rate": "Moon Rate",
    "featured": "Featured",
    "epic": "Epic",
    "legendary": "Legendary",
    "mythic": "Mythic",
}

_SEND_TYPE_DISPLAY_MAP = {
    "": "",
    "star_rate": "Star Rate",
    "featured": "Featured",
    "epic": "Epic",
    "legendary": "Legendary",
    "mythic": "Mythic",
}

_SEND_TYPE_DISPLAY_MAP_MOON = {
    "": "",
    "moon_rate": "Moon Rate",
    "featured": "Featured",
    "epic": "Epic",
    "legendary": "Legendary",
    "mythic": "Mythic",
}

_SEND_TYPE_EMOJI_MAP = {
    "star_rate": "<:rate_3:1290321896361164842>",
    "moon_rate": "<:rate_3:1290321896361164842>",
    "featured": "<:feature_3:1290320401146052752>",
    "epic": "<:epic_3:1290315599058309213>",
    "legendary": "<:legendary_3:1290315669858156617>",
    "mythic": "<:mythic_3:1290315801588666410>",
}


_DIFFICULTY_CONFIGS = {
    "unrated": {"base_kind": "unrated", "amount": None, "label": "Unrated", "emoji": "<:na:1445455241394006047>"},
    "auto": {"base_kind": "auto", "amount": "1", "label": "Auto", "emoji": "<:auto:1445455662296465581>"},
    "easy": {"base_kind": "easy", "amount": "2", "label": "Easy", "emoji": "<:easy:1445455093519356106>"},
    "normal": {"base_kind": "normal", "amount": "3", "label": "Normal", "emoji": "<:normal:1445455112028950579>"},
    "hard-4": {"base_kind": "hard", "amount": "4", "label": "Hard", "emoji": "<:hard:1445455132287565885>"},
    "hard-5": {"base_kind": "hard", "amount": "5", "label": "Hard", "emoji": "<:hard:1445455132287565885>"},
    "hard": {"base_kind": "hard", "amount": "4-5", "label": "Hard", "emoji": "<:hard:1445455132287565885>"},
    "harder-6": {"base_kind": "harder", "amount": "6", "label": "Harder", "emoji": "<:harder:1445455157658914896>"},
    "harder-7": {"base_kind": "harder", "amount": "7", "label": "Harder", "emoji": "<:harder:1445455157658914896>"},
    "harder": {"base_kind": "harder", "amount": "6-7", "label": "Harder", "emoji": "<:harder:1445455157658914896>"},
    "insane-8": {"base_kind": "insane", "amount": "8", "label": "Insane", "emoji": "<:insane:1445455184661708920>"},
    "insane-9": {"base_kind": "insane", "amount": "9", "label": "Insane", "emoji": "<:insane:1445455184661708920>"},
    "insane": {"base_kind": "insane", "amount": "8-9", "label": "Insane", "emoji": "<:insane:1445455184661708920>"},
    "demon": {"base_kind": "demon-hard", "amount": "10", "label": "Demon", "emoji": "<:demon:1445455210591027344>"},
    "demon-easy": {"base_kind": "demon-easy", "amount": "10", "label": "Easy Demon", "emoji": "<:demoneasy:1475778974268391526>"},
    "demon-medium": {"base_kind": "demon-medium", "amount": "10", "label": "Medium Demon", "emoji": "<:demonmedium:1475779000529064058>"},
    "demon-hard": {"base_kind": "demon-hard", "amount": "10", "label": "Hard Demon", "emoji": "<:demon:1445455210591027344>"},
    "demon-insane": {"base_kind": "demon-insane", "amount": "10", "label": "Insane Demon", "emoji": "<:demoninsane:1475779034750124153>"},
    "demon-extreme": {"base_kind": "demon-extreme", "amount": "10", "label": "Extreme Demon", "emoji": "<:demonextreme:1475779202035744871>"},
    "unrate": {"base_kind": "unrate", "amount": None, "label": "Unrate", "emoji": "<:unrate_3:1290328913314316413>"},
}


def _extract_modal_select_value(interaction: discord.Interaction, custom_id: str, select_obj=None) -> str:
    try:
        if select_obj is not None:
            selected_values = getattr(select_obj, "values", None)
            if selected_values:
                return str(selected_values[0]).strip()
    except Exception:
        pass

    def _walk_components(components):
        if not isinstance(components, list):
            return None
        for component in components:
            if not isinstance(component, dict):
                continue
            if component.get("custom_id") == custom_id:
                values = component.get("values") or []
                if values:
                    return str(values[0]).strip()
                return ""
            nested_components = component.get("components")
            found = _walk_components(nested_components)
            if found is not None:
                return found
        return None

    interaction_data = getattr(interaction, "data", None)
    if isinstance(interaction_data, dict):
        found = _walk_components(interaction_data.get("components"))
        if found is not None:
            return found

    return ""


def _normalize_send_type_value(raw_value: str) -> str:
    value = "" if raw_value is None else str(raw_value).strip().lower()
    if value in ("", "none", "auto", "unknown", "na", "n/a"):
        return ""
    aliases = {
        "rate": "star_rate",
        "star rate": "star_rate",
        "star-rate": "star_rate",
        "starrate": "star_rate",
        "star_rate": "star_rate",
        "moon": "moon_rate",
        "moon rate": "moon_rate",
        "moon-rate": "moon_rate",
        "moonrate": "moon_rate",
        "moon_rate": "moon_rate",
        "feature": "featured",
        "featured": "featured",
        "feat": "featured",
        "epic": "epic",
        "legendary": "legendary",
        "mythic": "mythic",
    }
    return aliases.get(value, value)


def _send_type_display(raw_value: str) -> str:
    normalized = _normalize_send_type_value(raw_value)
    return _SEND_TYPE_DISPLAY_MAP_ALL.get(normalized, str(raw_value or "").strip())

def _send_type_emoji(raw_value: str) -> str:
    normalized = _normalize_send_type_value(raw_value)
    return _SEND_TYPE_EMOJI_MAP.get(normalized, "<:rate_3:1290321896361164842>")


def _send_type_to_tier_kind(raw_value: str) -> str:
    normalized = _normalize_send_type_value(raw_value)
    if normalized in ("", "star_rate", "moon_rate"):
        return "none"
    return normalized if normalized in {"featured", "epic", "legendary", "mythic"} else "none"


def _normalize_staff_difficulty_value(raw_value: str) -> str:
    value = str(raw_value or "").strip().lower()
    if value in ("", "none", "na", "n/a", "unrated", "not specified", "not_specified"):
        return "unrated"

    aliases = {
        "1": "auto",
        "auto": "auto",
        "2": "easy",
        "easy": "easy",
        "3": "normal",
        "normal": "normal",
        "4": "hard-4",
        "hard-4": "hard-4",
        "5": "hard-5",
        "hard-5": "hard-5",
        "4-5": "hard",
        "hard": "hard",
        "6": "harder-6",
        "harder-6": "harder-6",
        "7": "harder-7",
        "harder-7": "harder-7",
        "6-7": "harder",
        "harder": "harder",
        "8": "insane-8",
        "insane-8": "insane-8",
        "9": "insane-9",
        "insane-9": "insane-9",
        "8-9": "insane",
        "insane": "insane",
        "demon": "demon",
        "easy demon": "demon-easy",
        "easy-demon": "demon-easy",
        "demon-easy": "demon-easy",
        "medium demon": "demon-medium",
        "medium-demon": "demon-medium",
        "demon-medium": "demon-medium",
        "hard demon": "demon-hard",
        "hard-demon": "demon-hard",
        "demon-hard": "demon-hard",
        "insane demon": "demon-insane",
        "insane-demon": "demon-insane",
        "demon-insane": "demon-insane",
        "extreme demon": "demon-extreme",
        "extreme-demon": "demon-extreme",
        "demon-extreme": "demon-extreme",
        "unrate": "unrate",
    }
    return aliases.get(value, "unrated")


def _build_staff_difficulty_payload(raw_value: str, *, reward_kind: str) -> dict:
    difficulty_key = _normalize_staff_difficulty_value(raw_value)
    config = dict(_DIFFICULTY_CONFIGS.get(difficulty_key, _DIFFICULTY_CONFIGS["unrated"]))
    config["key"] = difficulty_key

    amount = config.get("amount")
    if amount in (None, ""):
        amount_text = ""
    elif amount in {"1", "2", "3", "10"}:
        reward_word = "Moon" if reward_kind == "moon" else "Star"
        if amount != "1":
            reward_word += "s"
        amount_text = f", {amount} {reward_word}"
    else:
        reward_word = "Moons" if reward_kind == "moon" else "Stars"
        amount_text = f", {amount} {reward_word}"

    config["display"] = f"{config['label']}{amount_text}" if config["label"] not in {"Unrated", "Unrate"} else config["label"]
    return config


async def _build_staff_badge_file(*, base_kind: str, tier_kind: str, reward_kind: str, amount: str | None, filename: str):
    try:
        from . import commands_config as _commands_config
    except Exception as e:
        print(f"staff badge import error: {e}")
        return None

    try:
        if base_kind in {"unrated", "unrate"}:
            base_asset_path = _commands_config._resolve_gd_rate_badge_base_asset(base_kind, tier_kind)
            if base_asset_path is None:
                return None

            with _commands_config.Image.open(base_asset_path) as base_raw:
                base_image = _commands_config._gd_rate_badge_crop(base_raw).convert("RGBA")

            base_box_size = 180
            base_target_height = 110 if tier_kind == "none" else 160
            base_image = _commands_config._gd_rate_badge_resize_to_height(base_image, base_target_height)

            left_padding = 72
            right_padding = 12
            top_padding = 12
            bottom_padding = 84
            box_x = left_padding
            box_y = top_padding - 12
            base_offset_x = -8 if tier_kind == "mythic" else 0

            canvas_width = left_padding + base_box_size + right_padding
            canvas_height = top_padding + base_box_size + bottom_padding
            canvas = _commands_config.Image.new("RGBA", (canvas_width, canvas_height), (0, 0, 0, 0))

            base_x = box_x + (base_box_size - base_image.width) // 2 + base_offset_x
            base_y = box_y + (base_box_size - base_image.height) // 2
            canvas.paste(base_image, (base_x, base_y), base_image)

            from io import BytesIO
            output = BytesIO()
            canvas.save(output, format="PNG")
            output.seek(0)
            return discord.File(output, filename=filename)

        normalized_amount = str(amount or "10")
        return await _commands_config._build_gd_rate_badge_file(
            base_kind=base_kind,
            tier_kind=tier_kind,
            reward_kind=reward_kind,
            amount=normalized_amount,
            filename=filename,
        )
    except Exception as e:
        print(f"staff badge build error: {e}")
        return None


def _build_staff_result_view(*, title_emoji: str, title_text: str, lead_text: str, level_name: str, level_id: str, creator_name: str,
                             difficulty_text: str, send_type_text: str, checker_line_label: str, checker_mention: str,
                             requester_mention: str, note_text: str = "", badge_file=None, accent_colour=None):
    if not _staff_has_components_v2():
        return None

    details_lines = [
        "### Level Info",
        f'1. **"{level_name}"** by **{creator_name}**',
        f"2. **{difficulty_text}**",
    ]
    if send_type_text:
        details_lines.append(f"3. **Type:** {send_type_text}")
    details_lines.append(f"-# {lead_text}")
    details_text = "\n".join(details_lines)

    view = discord.ui.LayoutView(timeout=None)
    container_kwargs = {}
    if accent_colour is not None:
        container_kwargs["accent_colour"] = accent_colour
    container = discord.ui.Container(**container_kwargs)
    container.add_item(discord.ui.TextDisplay(f"### {title_emoji} {title_text}"))
    container.add_item(discord.ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small))

    if badge_file is not None:
        container.add_item(
            discord.ui.Section(
                discord.ui.TextDisplay(details_text),
                accessory=discord.ui.Thumbnail(badge_file.uri, description=f"{difficulty_text} badge"),
            )
        )
    else:
        container.add_item(discord.ui.TextDisplay(details_text))

    if note_text:
        container.add_item(discord.ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small))
        container.add_item(discord.ui.TextDisplay(f"**Note:**\n{note_text}"))

    container.add_item(discord.ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small))
    container.add_item(
        discord.ui.TextDisplay(
            f"**Level ID:** `{level_id}`, **{checker_line_label}:** {checker_mention}, **Requester:** {requester_mention}"
        )
    )
    view.add_item(container)
    return view


def _pick_request_id_from_candidates(candidate_ids: list[int], *, random_mode: bool):
    if not candidate_ids:
        return None
    return random.choice(candidate_ids) if random_mode else candidate_ids[0]


def _helper_queue_candidate_ids(
    server_id: int,
    user_id: int,
    *,
    min_request_id: int = 0,
    event: str = "0",
    platformer_filter: str = "",
    difficulty_filter: str = "",
    min_senddb=None,
    max_senddb=None,
    max_sent_to=None,
) -> list[int]:
    max_request_id = get_max_request_id()
    candidates = []
    user_id_str = str(user_id)
    min_request_id = max(1, _normalize_min_request_id(min_request_id))

    for request_id in range(min_request_id, max_request_id + 1):
        row = getsheetlinefromreqid(request_id)
        if not row:
            continue
        if _safe_str(row[_SHEET_IDX_SERVER_ID]) != str(server_id):
            continue
        row_event = _safe_str(row[_SHEET_IDX_EVENT]) if len(row) > _SHEET_IDX_EVENT else "0"
        if normalize_event_name(row_event).lower() != normalize_event_name(event).lower():
            continue
        if not _queue_filters_match_row(
            row,
            platformer_filter=platformer_filter,
            difficulty_filter=difficulty_filter,
            min_senddb=min_senddb,
            max_senddb=max_senddb,
            max_sent_to=max_sent_to,
        ):
            continue

        helpers_not_send = _safe_str(row[_SHEET_IDX_HELPERS_NOT_SEND])

        if (
            _row_actor_in_any_typed_sends(row, "helper", user_id_str)
            or _row_actor_in_any_not_send_reasons(row, "helper", user_id_str)
            or user_id_str in helpers_not_send
        ):
            continue

        candidates.append(request_id)

    return candidates


def _find_next_helper_request_id(server_id: int, user_id: int, *, random_mode: bool, min_request_id: int = 0, event: str = "0", platformer_filter: str = "", difficulty_filter: str = "", min_senddb=None, max_senddb=None, max_sent_to=None):
    """Pick next request for helper workflow.
    random_mode=False -> first matching request
    random_mode=True  -> random among all matching requests
    """
    return _pick_request_id_from_candidates(
        _helper_queue_candidate_ids(
            server_id,
            user_id,
            min_request_id=min_request_id,
            event=event,
            platformer_filter=platformer_filter,
            difficulty_filter=difficulty_filter,
            min_senddb=min_senddb,
            max_senddb=max_senddb,
            max_sent_to=max_sent_to,
        ),
        random_mode=random_mode,
    )


def _find_next_helper_request_with_count(server_id: int, user_id: int, *, random_mode: bool, min_request_id: int = 0, event: str = "0", platformer_filter: str = "", difficulty_filter: str = "", min_senddb=None, max_senddb=None, max_sent_to=None):
    candidates = _helper_queue_candidate_ids(
        server_id,
        user_id,
        min_request_id=min_request_id,
        event=event,
        platformer_filter=platformer_filter,
        difficulty_filter=difficulty_filter,
        min_senddb=min_senddb,
        max_senddb=max_senddb,
        max_sent_to=max_sent_to,
    )
    return _pick_request_id_from_candidates(candidates, random_mode=random_mode), len(candidates)


def _helper_sent_queue_candidate_ids(
    server_id: int,
    user_mention: str,
    *,
    min_request_id: int = 0,
    event: str = "0",
    send_type_filter: str = "",
    platformer_filter: str = "",
    difficulty_filter: str = "",
    min_senddb=None,
    max_senddb=None,
    max_sent_to=None,
) -> list[int]:
    max_request_id = get_max_request_id()
    candidates = []
    min_request_id = max(1, _normalize_min_request_id(min_request_id))

    for request_id in range(min_request_id, max_request_id + 1):
        row = getsheetlinefromreqid(request_id)
        if not row:
            continue
        if _safe_str(row[_SHEET_IDX_SERVER_ID]) != str(server_id):
            continue
        row_event = _safe_str(row[_SHEET_IDX_EVENT]) if len(row) > _SHEET_IDX_EVENT else "0"
        if normalize_event_name(row_event).lower() != normalize_event_name(event).lower():
            continue
        if not _queue_filters_match_row(
            row,
            platformer_filter=platformer_filter,
            difficulty_filter=difficulty_filter,
            min_senddb=min_senddb,
            max_senddb=max_senddb,
            max_sent_to=max_sent_to,
            send_type_filter="",
            send_type_index=None,
            fallback_star_rate=False,
        ):
            continue
        if not _row_actor_in_any_typed_sends(row, "helper", user_mention):
            continue
        normalized_send_type_filters = _parse_queue_send_type_filters(send_type_filter)
        if normalized_send_type_filters and not any(
            _row_contains_actor(row, "helper", normalized_send_type_filter, user_mention)
            for normalized_send_type_filter in normalized_send_type_filters
        ):
            continue
        if _safe_str(row[_SHEET_IDX_IS_RATED]) == "Yes":
            continue

        candidates.append(request_id)

    return candidates


def _find_next_helper_sent_request_with_count(server_id: int, user_mention: str, *, random_mode: bool, min_request_id: int = 0, event: str = "0", send_type_filter: str = "", platformer_filter: str = "", difficulty_filter: str = "", min_senddb=None, max_senddb=None, max_sent_to=None):
    candidates = _helper_sent_queue_candidate_ids(
        server_id,
        user_mention,
        min_request_id=min_request_id,
        event=event,
        send_type_filter=send_type_filter,
        platformer_filter=platformer_filter,
        difficulty_filter=difficulty_filter,
        min_senddb=min_senddb,
        max_senddb=max_senddb,
        max_sent_to=max_sent_to,
    )
    return _pick_request_id_from_candidates(candidates, random_mode=random_mode), len(candidates)


def _reviewer_queue_candidate_ids(server_id: int, review_lang: str, *, min_request_id: int = 0, event: str = "0") -> list[int]:
    max_request_id = get_max_request_id()
    candidates = []
    min_request_id = max(1, _normalize_min_request_id(min_request_id))

    for request_id in range(min_request_id, max_request_id + 1):
        row = getsheetlinefromreqid(request_id)
        if not row:
            continue
        if _safe_str(row[_SHEET_IDX_SERVER_ID]) != str(server_id):
            continue
        row_event = _safe_str(row[_SHEET_IDX_EVENT]) if len(row) > _SHEET_IDX_EVENT else "0"
        if normalize_event_name(row_event).lower() != normalize_event_name(event).lower():
            continue
        if _safe_str(row[_SHEET_IDX_REVIEWERS]) != "":
            continue
        if _safe_str(row[_SHEET_IDX_IS_RATED]) != "No":
            continue
        if _safe_str(row[_SHEET_IDX_REVIEW_LANGUAGE]) != review_lang:
            continue
        review_requested = "0"
        if len(row) > _SHEET_IDX_REVIEW_REQUESTED:
            review_requested = _safe_str(row[_SHEET_IDX_REVIEW_REQUESTED]) or "0"
        if review_requested not in ("0", "Yes", "Yes Review and Feedback", "Yes Review No Feedback", "Yes Review Yes Feedback"):
            continue

        candidates.append(request_id)

    return candidates


def _find_next_reviewer_request_id(server_id: int, review_lang: str, *, random_mode: bool, min_request_id: int = 0, event: str = "0"):
    """Pick next request for reviewer workflow.
    review_lang: 'us'|'ru'|'esp'|'fr'
    random_mode=False -> first matching request
    random_mode=True  -> random among all matching requests
    """
    return _pick_request_id_from_candidates(
        _reviewer_queue_candidate_ids(server_id, review_lang, min_request_id=min_request_id, event=event),
        random_mode=random_mode,
    )


def _find_next_reviewer_request_with_count(server_id: int, review_lang: str, *, random_mode: bool, min_request_id: int = 0, event: str = "0"):
    candidates = _reviewer_queue_candidate_ids(server_id, review_lang, min_request_id=min_request_id, event=event)
    return _pick_request_id_from_candidates(candidates, random_mode=random_mode), len(candidates)


_MODERATOR_QUEUE_MODE_HELPER_APPROVED_FIRST = 11
_MODERATOR_QUEUE_MODE_HELPER_APPROVED_RANDOM = 13
_MODERATOR_QUEUE_MODE_NO_HELPER_APPROVED_FIRST = 14
_MODERATOR_QUEUE_MODE_NO_HELPER_APPROVED_RANDOM = 15


def _normalize_mode_value(mode_value):
    try:
        return int(mode_value)
    except (TypeError, ValueError):
        return mode_value


def _is_moderator_queue_mode(mode_value) -> bool:
    normalized_mode = _normalize_mode_value(mode_value)
    return normalized_mode in (
        _MODERATOR_QUEUE_MODE_HELPER_APPROVED_FIRST,
        _MODERATOR_QUEUE_MODE_HELPER_APPROVED_RANDOM,
        _MODERATOR_QUEUE_MODE_NO_HELPER_APPROVED_FIRST,
        _MODERATOR_QUEUE_MODE_NO_HELPER_APPROVED_RANDOM,
    )


def _is_moderator_queue_random_mode(mode_value) -> bool:
    normalized_mode = _normalize_mode_value(mode_value)
    return normalized_mode in (
        _MODERATOR_QUEUE_MODE_HELPER_APPROVED_RANDOM,
        _MODERATOR_QUEUE_MODE_NO_HELPER_APPROVED_RANDOM,
    )


def _is_moderator_queue_helper_approved_only(mode_value) -> bool:
    normalized_mode = _normalize_mode_value(mode_value)
    return normalized_mode in (
        _MODERATOR_QUEUE_MODE_HELPER_APPROVED_FIRST,
        _MODERATOR_QUEUE_MODE_HELPER_APPROVED_RANDOM,
    )


def _moderator_queue_candidate_ids(
    server_id: int,
    moderator_user,
    *,
    helper_approved_only: bool = True,
    min_request_id: int = 0,
    event: str = "0",
    platformer_filter: str = "",
    difficulty_filter: str = "",
    min_senddb=None,
    max_senddb=None,
) -> list[int]:
    max_request_id = get_max_request_id()
    candidates = []
    moderator_id_str = str(getattr(moderator_user, "id", moderator_user))
    min_request_id = max(1, _normalize_min_request_id(min_request_id))

    for request_id in range(min_request_id, max_request_id + 1):
        row = getsheetlinefromreqid(request_id)
        if not row:
            continue
        if _safe_str(row[_SHEET_IDX_SERVER_ID]) != str(server_id):
            continue
        row_event = _safe_str(row[_SHEET_IDX_EVENT]) if len(row) > _SHEET_IDX_EVENT else "0"
        if normalize_event_name(row_event).lower() != normalize_event_name(event).lower():
            continue
        if not _queue_filters_match_row(
            row,
            platformer_filter=platformer_filter,
            difficulty_filter=difficulty_filter,
            min_senddb=min_senddb,
            max_senddb=max_senddb,
        ):
            continue

        has_helper_approved = _row_has_any_typed_sends(row, "helper")
        if helper_approved_only and not has_helper_approved:
            continue

        if _safe_str(row[_SHEET_IDX_IS_RATED]) == "Yes":
            continue

        moderators_not_send_value = _safe_str(row[_SHEET_IDX_MODERATORS_NOT_SEND])
        already_processed_by_this_moderator = (
            _row_actor_in_any_typed_sends(row, "moderator", moderator_id_str)
            or _row_actor_in_any_not_send_reasons(row, "moderator", moderator_id_str)
            or (moderator_id_str in moderators_not_send_value)
        )
        if already_processed_by_this_moderator:
            continue

        candidates.append(request_id)

    return candidates


def _find_next_moderator_request_id(server_id: int, moderator_user, *, random_mode: bool, helper_approved_only: bool = True, min_request_id: int = 0, event: str = "0", platformer_filter: str = "", difficulty_filter: str = "", min_senddb=None, max_senddb=None):
    """Pick next request for moderator workflow (helper-approved or without helper-approved).
    Hides requests already processed by this moderator (ModeratorsSend/ModeratorsNotSend).
    """
    return _pick_request_id_from_candidates(
        _moderator_queue_candidate_ids(
            server_id,
            moderator_user,
            helper_approved_only=helper_approved_only,
            min_request_id=min_request_id,
            event=event,
            platformer_filter=platformer_filter,
            difficulty_filter=difficulty_filter,
            min_senddb=min_senddb,
            max_senddb=max_senddb,
        ),
        random_mode=random_mode,
    )


def _find_next_moderator_request_with_count(server_id: int, moderator_user, *, random_mode: bool, helper_approved_only: bool = True, min_request_id: int = 0, event: str = "0", platformer_filter: str = "", difficulty_filter: str = "", min_senddb=None, max_senddb=None):
    candidates = _moderator_queue_candidate_ids(
        server_id,
        moderator_user,
        helper_approved_only=helper_approved_only,
        min_request_id=min_request_id,
        event=event,
        platformer_filter=platformer_filter,
        difficulty_filter=difficulty_filter,
        min_senddb=min_senddb,
        max_senddb=max_senddb,
    )
    return _pick_request_id_from_candidates(candidates, random_mode=random_mode), len(candidates)



_SHEET_IDX_IS_PLATFORMER = 10

_STAFF_COMPONENTS_V2_REQUIRED = ("LayoutView", "Container", "Section", "TextDisplay", "Thumbnail", "Separator")

_GD_RATE_BADGE_TIER_SUFFIX = {
    "none": "",
    "featured": "-featured",
    "epic": "-epic",
    "legendary": "-legendary",
    "mythic": "-mythic",
}

_GD_RATE_BADGE_ASSET_ROOT_CANDIDATES = (
    Path(__file__).resolve().parent.parent / "rate_badge_assets",
    Path(__file__).resolve().parent.parent,
    Path.cwd() / "rate_badge_assets",
    Path.cwd(),
)

_GD_RATE_BADGE_BASE_DIR_NAMES = ("rate_types",)
_GD_RATE_BADGE_REWARD_DIR_NAMES = ("level_types",)
_GD_RATE_BADGE_NUMBER_DIR_NAMES = ("difficulty_types",)

_GD_RATE_BADGE_BASE_FILENAME_CANDIDATES = {
    "rate": ("rate",),
    "unrate": ("unrate",),
    "unrated": ("unrated",),
    "auto": ("auto",),
    "easy": ("easy",),
    "normal": ("normal",),
    "hard": ("hard",),
    "harder": ("harder",),
    "insane": ("insane",),
    "demon-easy": ("demon-easy", "easy-demon", "demon_easy", "easy_demon"),
    "demon-medium": ("demon-medium", "medium-demon", "demon_medium", "medium_demon"),
    "demon-hard": ("demon-hard", "hard-demon", "demon_hard", "hard_demon"),
    "demon-insane": ("demon-insane", "insane-demon", "demon_insane", "insane_demon"),
    "demon-extreme": ("demon-extreme", "extreme-demon", "demon_extreme", "extreme_demon"),
}

_GD_RATE_BADGE_REWARD_FILENAME_CANDIDATES = {
    "star": ("star.png", "stars.png", "star-rate.png", "rate.png"),
    "moon": ("moon.png", "moons.png"),
}

_GD_RATE_BADGE_NUMBER_FILENAME_CANDIDATES = {
    value: (f"font1 - {value}.png", f"font1-{value}.png", f"font1_{value}.png", f"{value}.png")
    for value in ["1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "4-5", "6-7", "8-9"]
}

_HELPER_DIFFICULTY_OPTION_SPECS = [
    ("auto|1|Auto, 1 Star/Moon", "Auto, 1 Star", "<:auto:1445455662296465581>"),
    ("easy|2|Easy, 2 Stars/Moons", "Easy, 2 Stars", "<:easy:1445455093519356106>"),
    ("normal|3|Normal, 3 Stars/Moons", "Normal, 3 Stars", "<:normal:1445455112028950579>"),
    ("hard|4|Hard, 4 Stars/Moons", "Hard, 4 Stars", "<:hard:1445455132287565885>"),
    ("hard|5|Hard, 5 Stars/Moons", "Hard, 5 Stars", "<:hard:1445455132287565885>"),
    ("harder|6|Harder, 6 Stars/Moons", "Harder, 6 Stars", "<:harder:1445455157658914896>"),
    ("harder|7|Harder, 7 Stars/Moons", "Harder, 7 Stars", "<:harder:1445455157658914896>"),
    ("insane|8|Insane, 8 Stars/Moons", "Insane, 8 Stars", "<:insane:1445455184661708920>"),
    ("insane|9|Insane, 9 Stars/Moons", "Insane, 9 Stars", "<:insane:1445455184661708920>"),
    ("demon-easy|10|Easy Demon, 10 Stars/Moons", "Easy Demon, 10 Stars", "<:demoneasy:1475778974268391526>"),
    ("demon-medium|10|Medium Demon, 10 Stars/Moons", "Medium Demon, 10 Stars", "<:demonmedium:1475779000529064058>"),
    ("demon-hard|10|Hard Demon, 10 Stars/Moons", "Hard Demon, 10 Stars", "<:demon:1445455210591027344>"),
    ("demon-insane|10|Insane Demon, 10 Stars/Moons", "Insane Demon, 10 Stars", "<:demoninsane:1475779034750124153>"),
    ("demon-extreme|10|Extreme Demon, 10 Stars/Moons", "Extreme Demon, 10 Stars", "<:demonextreme:1475779202035744871>"),
]

_HELPER_DIFFICULTY_OPTION_SPECS_MOON = [
    ("auto|1|Auto, 1 Star/Moon", "Auto, 1 Moon", "<:auto:1445455662296465581>"),
    ("easy|2|Easy, 2 Stars/Moons", "Easy, 2 Moons", "<:easy:1445455093519356106>"),
    ("normal|3|Normal, 3 Stars/Moons", "Normal, 3 Moons", "<:normal:1445455112028950579>"),
    ("hard|4|Hard, 4 Stars/Moons", "Hard, 4 Moons", "<:hard:1445455132287565885>"),
    ("hard|5|Hard, 5 Stars/Moons", "Hard, 5 Moons", "<:hard:1445455132287565885>"),
    ("harder|6|Harder, 6 Stars/Moons", "Harder, 6 Moons", "<:harder:1445455157658914896>"),
    ("harder|7|Harder, 7 Stars/Moons", "Harder, 7 Moons", "<:harder:1445455157658914896>"),
    ("insane|8|Insane, 8 Stars/Moons", "Insane, 8 Moons", "<:insane:1445455184661708920>"),
    ("insane|9|Insane, 9 Stars/Moons", "Insane, 9 Moons", "<:insane:1445455184661708920>"),
    ("demon-easy|10|Easy Demon, 10 Stars/Moons", "Easy Demon, 10 Moons", "<:demoneasy:1475778974268391526>"),
    ("demon-medium|10|Medium Demon, 10 Stars/Moons", "Medium Demon, 10 Moons", "<:demonmedium:1475779000529064058>"),
    ("demon-hard|10|Hard Demon, 10 Stars/Moons", "Hard Demon, 10 Moons", "<:demon:1445455210591027344>"),
    ("demon-insane|10|Insane Demon, 10 Stars/Moons", "Insane Demon, 10 Moons", "<:demoninsane:1475779034750124153>"),
    ("demon-extreme|10|Extreme Demon, 10 Stars/Moons", "Extreme Demon, 10 Moons", "<:demonextreme:1475779202035744871>"),
]


_MODERATOR_DIFFICULTY_OPTION_SPECS = [
    ("auto|1|Auto, 1 Star/Moon", "Auto, 1 Star", "<:auto:1445455662296465581>"),
    ("easy|2|Easy, 2 Stars/Moons", "Easy, 2 Stars", "<:easy:1445455093519356106>"),
    ("normal|3|Normal, 3 Stars/Moons", "Normal, 3 Stars", "<:normal:1445455112028950579>"),
    ("hard|4|Hard, 4 Stars/Moons", "Hard, 4 Stars", "<:hard:1445455132287565885>"),
    ("hard|5|Hard, 5 Stars/Moons", "Hard, 5 Stars", "<:hard:1445455132287565885>"),
    ("harder|6|Harder, 6 Stars/Moons", "Harder, 6 Stars", "<:harder:1445455157658914896>"),
    ("harder|7|Harder, 7 Stars/Moons", "Harder, 7 Stars", "<:harder:1445455157658914896>"),
    ("insane|8|Insane, 8 Stars/Moons", "Insane, 8 Stars", "<:insane:1445455184661708920>"),
    ("insane|9|Insane, 9 Stars/Moons", "Insane, 9 Stars", "<:insane:1445455184661708920>"),
    ("demon-hard|10|Demon, 10 Stars/Moons", "Demon, 10 Stars", "<:demon:1445455210591027344>"),
    ("demon-easy|10|Easy Demon, 10 Stars/Moons", "Easy Demon, 10 Stars", "<:demoneasy:1475778974268391526>"),
    ("demon-medium|10|Medium Demon, 10 Stars/Moons", "Medium Demon, 10 Stars", "<:demonmedium:1475779000529064058>"),
    ("demon-hard|10|Hard Demon, 10 Stars/Moons", "Hard Demon, 10 Stars", "<:demon:1445455210591027344>"),
    ("demon-insane|10|Insane Demon, 10 Stars/Moons", "Insane Demon, 10 Stars", "<:demoninsane:1475779034750124153>"),
    ("demon-extreme|10|Extreme Demon, 10 Stars/Moons", "Extreme Demon, 10 Stars", "<:demonextreme:1475779202035744871>"),
]

_MODERATOR_DIFFICULTY_OPTION_SPECS_MOON = [
    ("auto|1|Auto, 1 Star/Moon", "Auto, 1 Moon", "<:auto:1445455662296465581>"),
    ("easy|2|Easy, 2 Stars/Moons", "Easy, 2 Moons", "<:easy:1445455093519356106>"),
    ("normal|3|Normal, 3 Stars/Moons", "Normal, 3 Moons", "<:normal:1445455112028950579>"),
    ("hard|4|Hard, 4 Stars/Moons", "Hard, 4 Moons", "<:hard:1445455132287565885>"),
    ("hard|5|Hard, 5 Stars/Moons", "Hard, 5 Moons", "<:hard:1445455132287565885>"),
    ("harder|6|Harder, 6 Stars/Moons", "Harder, 6 Moons", "<:harder:1445455157658914896>"),
    ("harder|7|Harder, 7 Stars/Moons", "Harder, 7 Moons", "<:harder:1445455157658914896>"),
    ("insane|8|Insane, 8 Stars/Moons", "Insane, 8 Moons", "<:insane:1445455184661708920>"),
    ("insane|9|Insane, 9 Stars/Moons", "Insane, 9 Moons", "<:insane:1445455184661708920>"),
    ("demon-hard|10|Demon, 10 Stars/Moons", "Demon, 10 Moons", "<:demon:1445455210591027344>"),
    ("demon-easy|10|Easy Demon, 10 Stars/Moons", "Easy Demon, 10 Moons", "<:demoneasy:1475778974268391526>"),
    ("demon-medium|10|Medium Demon, 10 Stars/Moons", "Medium Demon, 10 Moons", "<:demonmedium:1475779000529064058>"),
    ("demon-hard|10|Hard Demon, 10 Stars/Moons", "Hard Demon, 10 Moons", "<:demon:1445455210591027344>"),
    ("demon-insane|10|Insane Demon, 10 Stars/Moons", "Insane Demon, 10 Moons", "<:demoninsane:1475779034750124153>"),
    ("demon-extreme|10|Extreme Demon, 10 Stars/Moons", "Extreme Demon, 10 Moons", "<:demonextreme:1475779202035744871>"),
]

_GDMOD_DIFFICULTY_OPTION_SPECS = [
    ("auto|1|Auto, 1 Star/Moon", "Auto, 1 Star", "<:auto:1445455662296465581>"),
    ("easy|2|Easy, 2 Stars/Moons", "Easy, 2 Stars", "<:easy:1445455093519356106>"),
    ("normal|3|Normal, 3 Stars/Moons", "Normal, 3 Stars", "<:normal:1445455112028950579>"),
    ("hard|4|Hard, 4 Stars/Moons", "Hard, 4 Stars", "<:hard:1445455132287565885>"),
    ("hard|5|Hard, 5 Stars/Moons", "Hard, 5 Stars", "<:hard:1445455132287565885>"),
    ("harder|6|Harder, 6 Stars/Moons", "Harder, 6 Stars", "<:harder:1445455157658914896>"),
    ("harder|7|Harder, 7 Stars/Moons", "Harder, 7 Stars", "<:harder:1445455157658914896>"),
    ("insane|8|Insane, 8 Stars/Moons", "Insane, 8 Stars", "<:insane:1445455184661708920>"),
    ("insane|9|Insane, 9 Stars/Moons", "Insane, 9 Stars", "<:insane:1445455184661708920>"),
    ("demon-hard|10|Demon, 10 Stars/Moons", "Demon, 10 Stars", "<:demon:1445455210591027344>"),
    ("hard|4-5|Hard, 4-5 Stars/Moons", "Hard, 4-5 Stars", "<:hard:1445455132287565885>"),
    ("harder|6-7|Harder, 6-7 Stars/Moons", "Harder, 6-7 Stars", "<:harder:1445455157658914896>"),
    ("insane|8-9|Insane, 8-9 Stars/Moons", "Insane, 8-9 Stars", "<:insane:1445455184661708920>"),
]

_GDMOD_DIFFICULTY_OPTION_SPECS_MOON = [
    ("auto|1|Auto, 1 Star/Moon", "Auto, 1 Moon", "<:auto:1445455662296465581>"),
    ("easy|2|Easy, 2 Stars/Moons", "Easy, 2 Moons", "<:easy:1445455093519356106>"),
    ("normal|3|Normal, 3 Stars/Moons", "Normal, 3 Moons", "<:normal:1445455112028950579>"),
    ("hard|4|Hard, 4 Stars/Moons", "Hard, 4 Moons", "<:hard:1445455132287565885>"),
    ("hard|5|Hard, 5 Stars/Moons", "Hard, 5 Moons", "<:hard:1445455132287565885>"),
    ("harder|6|Harder, 6 Stars/Moons", "Harder, 6 Moons", "<:harder:1445455157658914896>"),
    ("harder|7|Harder, 7 Stars/Moons", "Harder, 7 Moons", "<:harder:1445455157658914896>"),
    ("insane|8|Insane, 8 Stars/Moons", "Insane, 8 Moons", "<:insane:1445455184661708920>"),
    ("insane|9|Insane, 9 Stars/Moons", "Insane, 9 Moons", "<:insane:1445455184661708920>"),
    ("demon-hard|10|Demon, 10 Stars/Moons", "Demon, 10 Moons", "<:demon:1445455210591027344>"),
    ("hard|4-5|Hard, 4-5 Stars/Moons", "Hard, 4-5 Moons", "<:hard:1445455132287565885>"),
    ("harder|6-7|Harder, 6-7 Stars/Moons", "Harder, 6-7 Moons", "<:harder:1445455157658914896>"),
    ("insane|8-9|Insane, 8-9 Stars/Moons", "Insane, 8-9 Moons", "<:insane:1445455184661708920>"),
]


def _supports_staff_components_v2() -> bool:
    return all(hasattr(discord.ui, attr) for attr in _STAFF_COMPONENTS_V2_REQUIRED)


def _supports_modal_file_upload() -> bool:
    return hasattr(discord.ui, "FileUpload") and hasattr(discord.ui, "Label")


def _supports_staff_media_gallery() -> bool:
    return hasattr(discord.ui, "MediaGallery")


def _is_probably_image_attachment(attachment) -> bool:
    content_type = str(getattr(attachment, "content_type", "") or "").lower()
    if content_type:
        return content_type.startswith("image/")

    filename = str(getattr(attachment, "filename", "") or "").lower()
    return filename.endswith((".png", ".jpg", ".jpeg", ".gif", ".webp"))


async def _extract_uploaded_image_files(upload_item, *, max_count: int | None = None) -> list[discord.File]:
    if upload_item is None:
        return []

    raw_values = getattr(upload_item, "values", None) or getattr(upload_item, "resolved_values", None) or []
    result = []

    for index, value in enumerate(raw_values):
        if max_count is not None and len(result) >= max_count:
            break

        # Пропускаем, если нет метода to_file
        if not hasattr(value, "to_file"):
            continue

        try:
            file = await value.to_file(use_cached=True)
        except TypeError:
            file = await value.to_file()

        if not getattr(file, "filename", None):
            file.filename = f"upload_{index + 1}.png"

        result.append(file)

    return result


def _split_nonempty_paragraphs(text: str) -> tuple[list[str], list[str]]:
    """
    Разбивает текст на абзацы – каждая непустая строка считается отдельным абзацем.
    Возвращает (абзацы, разделители), где разделитель – все символы между
    абзацами (включая переносы и пустые строки).
    """
    text = str(text or "").replace("\r\n", "\n").replace("\r", "\n")
    if not text.strip():
        return [], []
    lines = text.split("\n")
    paragraphs = []
    separators = []
    i = 0
    while i < len(lines):
        if lines[i].strip() == "":
            i += 1
            continue
        # Начало абзаца – непустая строка
        paragraphs.append(lines[i])
        # Если это последняя строка, разделитель пустой
        if i == len(lines) - 1:
            separators.append("")
            i += 1
        else:
            # Перенос после строки всегда есть (потому что есть следующая строка)
            sep = "\n"
            i += 1
            # Добавляем все последующие пустые строки как часть разделителя
            while i < len(lines) and lines[i].strip() == "":
                sep += "\n"
                i += 1
            separators.append(sep)
    while len(separators) < len(paragraphs):
        separators.append("")
    return paragraphs, separators

def _split_long_block_soft(text: str, limit: int = 1900) -> list[str]:
    text = str(text or "").strip()
    if not text:
        return []

    if len(text) <= limit:
        return [text]

    parts = []
    remaining = text

    while len(remaining) > limit:
        cut_candidates = [
            remaining.rfind("\n", 0, limit),
            remaining.rfind(". ", 0, limit),
            remaining.rfind("! ", 0, limit),
            remaining.rfind("? ", 0, limit),
            remaining.rfind(" ", 0, limit),
        ]
        cut = max(cut_candidates)

        if cut < max(300, limit // 3):
            cut = limit

        part = remaining[:cut].rstrip()
        if not part:
            part = remaining[:limit].rstrip()
            cut = limit

        parts.append(part)
        remaining = remaining[cut:].lstrip()

    if remaining:
        parts.append(remaining)

    return parts


def _parse_review_photo_positions(raw_value: str, photo_count: int, paragraph_count: int) -> list[int | None]:
    """
    Returns one position per photo, in reviewer upload order:
    - int: attach/send this photo after that 1-based review paragraph number
    - None: attach/send this photo at the end of the review
    """
    if photo_count <= 0:
        return []

    tokens = [
        token.strip().lower()
        for token in str(raw_value or "").replace(";", ",").split(",")
        if token.strip()
    ]

    result = []
    for index in range(photo_count):
        token = tokens[index] if index < len(tokens) else ""

        if token in {"", "end", "last", "final", "конец"}:
            result.append(None)
            continue

        try:
            paragraph_number = int(token)
        except ValueError:
            result.append(None)
            continue

        if 1 <= paragraph_number <= paragraph_count:
            result.append(paragraph_number)
        else:
            result.append(None)

    return result


_REVIEW_MESSAGE_CONTENT_LIMIT = 2000
_REVIEW_FILE_LIMIT_PER_MESSAGE = 10


def _split_long_text_hard(text: str, limit: int = _REVIEW_MESSAGE_CONTENT_LIMIT) -> list[str]:
    """Split a single oversized paragraph into Discord-safe chunks.

    This is intentionally simple: if one paragraph alone is over 2000 chars,
    it is split directly instead of trying to treat it as multiple paragraphs.
    """
    text = str(text or "")
    if not text:
        return []
    return [text[index:index + limit] for index in range(0, len(text), limit)]


async def _send_review_files_only(thread, files: list[discord.File]):
    """Fallback for review photos that have no text message to attach to."""
    files = list(files or [])
    if not files:
        return

    for start in range(0, len(files), _REVIEW_FILE_LIMIT_PER_MESSAGE):
        batch = files[start:start + _REVIEW_FILE_LIMIT_PER_MESSAGE]
        if len(batch) == 1:
            await thread.send(file=batch[0])
        else:
            await thread.send(files=batch)


async def _send_review_message(thread, content: str, files: list[discord.File] | None = None):
    content = str(content or "")  # не обрезаем!
    files = list(files or [])

    if not content and not files:
        return

    if not files:
        await thread.send(content)
        return

    first_batch = files[:_REVIEW_FILE_LIMIT_PER_MESSAGE]
    remaining_files = files[_REVIEW_FILE_LIMIT_PER_MESSAGE:]

    if len(first_batch) == 1:
        await thread.send(content, file=first_batch[0])
    else:
        await thread.send(content, files=first_batch)

    if remaining_files:
        await _send_review_files_only(thread, remaining_files)

async def _send_compact_review_messages(
    thread,
    paragraphs: list[str],
    separators: list[str],
    photos_after_paragraph: dict[int, list[tuple[int, discord.File]]],
    photos_for_end: list[tuple[int, discord.File]],
):
    """Отправляет ревью, сохраняя все оригинальные разделители между абзацами."""
    if not paragraphs:
        all_files = [photo_file for _, photo_file in photos_for_end]
        for entries in photos_after_paragraph.values():
            all_files.extend(photo_file for _, photo_file in entries)
        await _send_review_files_only(thread, all_files)
        return

    # Добавляем фото "в конец" к последнему абзацу
    if photos_for_end:
        last_idx = len(paragraphs)
        photos_after_paragraph.setdefault(last_idx, []).extend(photos_for_end)

    current_indices = []  # индексы абзацев, которые пойдут в текущее сообщение

    def build_text(indices: list[int]) -> str:
        if not indices:
            return ""
        parts = []
        for i, idx in enumerate(indices):
            parts.append(paragraphs[idx])
            # Разделитель только между абзацами, не после последнего
            if i < len(indices) - 1:
                parts.append(separators[idx])
        return "".join(parts)

    for idx, _ in enumerate(paragraphs):
        current_indices.append(idx)
        paragraph_number = idx + 1  # нумерация для пользователя начинается с 1

        if paragraph_number in photos_after_paragraph:
            files = [photo_file for _, photo_file in photos_after_paragraph[paragraph_number]]
            text = build_text(current_indices)
            await _send_review_message(thread, text, files=files)
            current_indices = []  # начинаем новую группу

    # Отправляем оставшиеся абзацы без фото
    if current_indices:
        text = build_text(current_indices)
        await _send_review_message(thread, text, files=[])


def _normalize_platformer_flag(platformer_value) -> bool:
    return str(platformer_value or "").strip().lower() in {"yes", "true", "1", "platformer"}


def _request_reward_kind_from_row(request_row) -> str:
    is_platformer = False
    if len(request_row) > _SHEET_IDX_IS_PLATFORMER:
        is_platformer = _normalize_platformer_flag(request_row[_SHEET_IDX_IS_PLATFORMER])
    return "moon" if is_platformer else "star"


def _send_type_display_raw_for_request(send_type_raw: str, request_row) -> str:
    normalized = _normalize_send_type_value(send_type_raw)
    if normalized in {"star_rate", "moon_rate"}:
        return "moon_rate" if _request_reward_kind_from_row(request_row) == "moon" else "star_rate"
    return send_type_raw


def _send_type_display_for_request(send_type_raw: str, request_row) -> str:
    display_raw = _send_type_display_raw_for_request(send_type_raw, request_row)
    return _send_type_display(display_raw)


def _compose_send_title_parts_for_request(send_type_raw: str, request_row, *, default_emoji: str = "<:sent:1155722807037149224>") -> tuple[str, str]:
    send_type_text = _send_type_display_for_request(send_type_raw, request_row)
    if send_type_text:
        return _send_type_emoji(send_type_raw), f"Sent for {send_type_text}"
    return default_emoji, "Sent"


def _gd_rate_badge_crop(image: Image.Image) -> Image.Image:
    rgba = image.convert("RGBA")
    bbox = rgba.getbbox()
    if bbox is None:
        return rgba
    return rgba.crop(bbox)


def _gd_rate_badge_resize_to_height(image: Image.Image, target_height: int) -> Image.Image:
    source = _gd_rate_badge_crop(image)
    if target_height <= 0:
        return source
    scale = target_height / max(1, source.height)
    target_width = max(1, int(round(source.width * scale)))
    return source.resize((target_width, target_height), Image.LANCZOS)


def _find_gd_rate_badge_asset(*candidate_names: str, dir_names: tuple[str, ...] = ()) -> Path | None:
    seen_paths = set()
    for root_dir in _GD_RATE_BADGE_ASSET_ROOT_CANDIDATES:
        search_dirs = []
        if root_dir.exists():
            search_dirs.append(root_dir)
            for dir_name in dir_names:
                if dir_name:
                    search_dirs.append(root_dir / dir_name)

        for base_dir in search_dirs:
            if not base_dir.exists():
                continue
            for candidate_name in candidate_names:
                if not candidate_name:
                    continue
                candidate_path = (base_dir / candidate_name).resolve()
                if str(candidate_path) in seen_paths:
                    continue
                seen_paths.add(str(candidate_path))
                if candidate_path.exists() and candidate_path.is_file():
                    return candidate_path
    return None


def _resolve_gd_rate_badge_base_asset(base_kind: str, tier_kind: str) -> Path | None:
    normalized_base = str(base_kind or "unrated").strip().lower()
    normalized_tier = str(tier_kind or "none").strip().lower()

    base_candidates = _GD_RATE_BADGE_BASE_FILENAME_CANDIDATES.get(normalized_base)
    tier_suffix = _GD_RATE_BADGE_TIER_SUFFIX.get(normalized_tier)
    if base_candidates is None or tier_suffix is None:
        return None

    filename_candidates = []
    for base_candidate in base_candidates:
        filename_candidates.append(f"{base_candidate}{tier_suffix}.png")
        filename_candidates.append(f"{base_candidate}{tier_suffix}.webp")

    return _find_gd_rate_badge_asset(*filename_candidates, dir_names=_GD_RATE_BADGE_BASE_DIR_NAMES)


def _resolve_gd_rate_badge_number_asset(number_value: str) -> Path | None:
    return _find_gd_rate_badge_asset(*_GD_RATE_BADGE_NUMBER_FILENAME_CANDIDATES.get(str(number_value), ()), dir_names=_GD_RATE_BADGE_NUMBER_DIR_NAMES)


def _resolve_gd_rate_badge_reward_asset(reward_kind: str) -> Path | None:
    return _find_gd_rate_badge_asset(*_GD_RATE_BADGE_REWARD_FILENAME_CANDIDATES.get(str(reward_kind or "").strip().lower(), ()), dir_names=_GD_RATE_BADGE_REWARD_DIR_NAMES)


def _safe_badge_filename_fragment(raw_value: str) -> str:
    return "".join(ch if ch.isalnum() or ch in {"-", "_"} else "_" for ch in str(raw_value or "").lower()).strip("_") or "badge"


async def _build_staff_gd_rate_badge_file(*, base_kind: str, tier_kind: str, reward_kind: str | None = None, amount: str | None = None, filename: str = "staff_badge.png"):
    base_asset_path = _resolve_gd_rate_badge_base_asset(base_kind, tier_kind)
    if base_asset_path is None:
        return None

    reward_asset_path = None
    number_asset_path = None
    if reward_kind and amount:
        reward_asset_path = _resolve_gd_rate_badge_reward_asset(reward_kind)
        number_asset_path = _resolve_gd_rate_badge_number_asset(str(amount))
        if reward_asset_path is None or number_asset_path is None:
            reward_asset_path = None
            number_asset_path = None

    with Image.open(base_asset_path) as base_raw:
        base_image = _gd_rate_badge_crop(base_raw)

    reward_image = None
    number_image = None
    if reward_asset_path is not None and number_asset_path is not None:
        with Image.open(reward_asset_path) as reward_raw, Image.open(number_asset_path) as number_raw:
            reward_image = _gd_rate_badge_crop(reward_raw)
            number_image = _gd_rate_badge_crop(number_raw)

    base_box_size = 180
    base_target_height = 110 if tier_kind == "none" else 160
    base_image = _gd_rate_badge_resize_to_height(base_image, base_target_height)

    left_padding = 72
    right_padding = 12
    top_padding = 12
    bottom_padding = 84 if reward_image is not None and number_image is not None else 16
    global_y_offset = -12
    box_x = left_padding
    box_y = top_padding + global_y_offset

    base_offset_x = -8 if tier_kind == "mythic" else 0
    base_x = box_x + (base_box_size - base_image.width) // 2 + base_offset_x
    base_y = box_y + (base_box_size - base_image.height) // 2

    canvas_width = left_padding + base_box_size + right_padding
    canvas_height = top_padding + base_box_size + bottom_padding
    canvas = Image.new("RGBA", (canvas_width, canvas_height), (0, 0, 0, 0))
    canvas.paste(base_image, (base_x, base_y), base_image)

    if reward_image is not None and number_image is not None:
        number_target_height = max(1, int(round(base_box_size * 0.21)))
        reward_target_height = max(1, int(round(base_box_size * 0.21)))
        number_image = _gd_rate_badge_resize_to_height(number_image, number_target_height)
        reward_image = _gd_rate_badge_resize_to_height(reward_image, reward_target_height)

        badge_gap = 11 if reward_kind == "star" else 12
        badges_width = number_image.width + reward_image.width + badge_gap
        badges_x = box_x + (base_box_size - badges_width) // 2
        badges_y = box_y + base_box_size - 14

        number_x = badges_x
        number_y = badges_y + 4
        reward_x = number_x + number_image.width + badge_gap
        reward_y = badges_y

        canvas.paste(number_image, (number_x, number_y), number_image)
        canvas.paste(reward_image, (reward_x, reward_y), reward_image)

    output_dir = tempfile.mkdtemp(prefix="staff_gd_badge_")
    output_path = Path(output_dir) / filename
    canvas.save(output_path, format="PNG")
    return discord.File(str(output_path), filename=filename)


def _build_select_options(option_specs):
    return [
        discord.SelectOption(label=label, value=value, emoji=emoji)
        for value, label, emoji in option_specs
    ]


def _parse_difficulty_choice(raw_value: str | None) -> dict:
    value = str(raw_value or "").strip()
    if not value:
        return {
            "base_kind": "unrated",
            "amount": None,
            "label": "Unrated",
            "selected": False,
        }

    parts = value.split("|", 2)
    if len(parts) != 3:
        return {
            "base_kind": "unrated",
            "amount": None,
            "label": "Unrated",
            "selected": False,
        }

    base_kind, amount, label = parts
    return {
        "base_kind": str(base_kind or "unrated").strip().lower(),
        "amount": str(amount or "").strip() or None,
        "label": str(label or "Unrated").strip() or "Unrated",
        "selected": True,
    }


def _normalize_send_type_value(raw_value: str) -> str:
    value = "" if raw_value is None else str(raw_value).strip().lower()

    if value in ("", "none", "auto", "unknown", "na", "n/a"):
        return ""
    if value in ("rate", "star", "star rate", "star-rate", "starrate", "star_rate"):
        return "star_rate"
    if value in ("moon", "moon rate", "moon-rate", "moonrate", "moon_rate"):
        return "moon_rate"
    if value in ("feature", "featured", "feat"):
        return "featured"
    if value == "epic":
        return "epic"
    if value == "legendary":
        return "legendary"
    if value == "mythic":
        return "mythic"

    return value



def _send_type_tier_kind(raw_value: str) -> str:
    normalized = _normalize_send_type_value(raw_value)
    tier_map = {
        "featured": "featured",
        "epic": "epic",
        "legendary": "legendary",
        "mythic": "mythic",
    }
    # The visual asset for Star/Moon Rate is the base/non-tier badge.
    return tier_map.get(normalized, "none")


def _send_type_filename_kind(raw_value: str) -> str:
    normalized = _normalize_send_type_value(raw_value)
    if normalized in {"star_rate", "moon_rate"}:
        return "rate"
    if normalized in {"featured", "epic", "legendary", "mythic"}:
        return normalized
    return "none"

def _compose_send_title_parts(send_type_raw: str, *, default_emoji: str = "<:sent:1155722807037149224>") -> tuple[str, str]:
    send_type_text = _send_type_display(send_type_raw)
    if send_type_text:
        return _send_type_emoji(send_type_raw), f"Sent for {send_type_text}"
    return default_emoji, "Sent"


def _compose_send_badge_label(send_type_raw: str, difficulty_raw: str | None) -> str:
    send_type_text = _send_type_display(send_type_raw)
    difficulty_info = _parse_difficulty_choice(difficulty_raw)
    difficulty_label = difficulty_info["label"]

    if not send_type_text:
        return difficulty_label
    if difficulty_label == "Unrated":
        return f"{send_type_text} Unrated"
    return f"{send_type_text} • {difficulty_label}"


async def _build_send_badge_for_request(request_row, *, send_type_raw: str, difficulty_raw: str | None, filename_prefix: str):
    difficulty_info = _parse_difficulty_choice(difficulty_raw)
    reward_kind = None
    amount = difficulty_info["amount"]
    if amount:
        reward_kind = _request_reward_kind_from_row(request_row)

    filename = (
        f"{_safe_badge_filename_fragment(filename_prefix)}_"
        f"{_safe_badge_filename_fragment(difficulty_info['base_kind'])}_"
        f"{_safe_badge_filename_fragment(_send_type_filename_kind(send_type_raw))}_"
        f"{_safe_badge_filename_fragment(amount or 'none')}.png"
    )
    if send_type_raw in ("", "none"):
        return None
    return await _build_staff_gd_rate_badge_file(
        base_kind=difficulty_info["base_kind"],
        tier_kind=_send_type_tier_kind(send_type_raw),
        reward_kind=reward_kind,
        amount=amount,
        filename=filename,
    )


async def _build_unrate_badge(filename_prefix: str, modd: str = "1"):
    filename = f"{_safe_badge_filename_fragment(filename_prefix)}_unrate.png"
    if modd == "0":
        return None
    return await _build_staff_gd_rate_badge_file(
        base_kind="unrate",
        tier_kind="none",
        reward_kind=None,
        amount=None,
        filename=filename,
    )

async def _build_rate_badge(filename_prefix: str, modd: str = "1"):
    filename = f"{_safe_badge_filename_fragment(filename_prefix)}_rate.png"
    if modd == "0":
        return None
    return await _build_staff_gd_rate_badge_file(
        base_kind="rate",
        tier_kind="none",
        reward_kind=None,
        amount=None,
        filename=filename,
    )


async def _safe_send_staff_payload(
    interaction: discord.Interaction | None,
    channel,
    send_kwargs: dict,
    *,
    require_embed: bool,
    require_file: bool,
):
    require_external_emojis = payload_uses_external_custom_emojis(
        channel,
        send_kwargs.get("content"),
        send_kwargs.get("embed"),
        send_kwargs.get("view"),
    )
    try:
        return await channel.send(**send_kwargs)
    except (discord.Forbidden, discord.HTTPException) as send_error:
        status = getattr(send_error, "status", None)
        if isinstance(send_error, discord.Forbidden) or status in (403, 404):
            await warn_send_permission_problem(
                interaction,
                channel,
                "Discord rejected the request result notification.",
                require_embed=require_embed,
                require_file=require_file,
                require_external_emojis=require_external_emojis,
                exception=send_error,
            )
            return None
        raise


async def _send_staff_components_message(
    channel,
    *,
    counter: str,
    title_emoji: str,
    title_text: str,
    headline_text: str,
    level_name: str,
    level_creator: str,
    level_id: str,
    checker_label: str,
    checker_mention: str,
    requester_mention: str | None = None,
    note_text: str = "",
    accent_colour=None,
    badge_file=None,
    extra_media_files: list[discord.File] | None = None,
    interaction: discord.Interaction | None = None,
    allow_requester_ping: bool = True,
):
    allowed_mentions = discord.AllowedMentions(users=bool(allow_requester_ping), roles=False, everyone=False)
    uses_components_v2 = _supports_staff_components_v2()
    extra_media_files = list(extra_media_files or [])[:10]
    requires_file = badge_file is not None or bool(extra_media_files)

    if not await ensure_send_permissions(
        interaction,
        channel,
        "Bot cannot send the request result notification.",
        require_embed=not uses_components_v2,
        require_file=requires_file,
    ):
        return None

    files_to_send = []
    if badge_file is not None:
        files_to_send.append(badge_file)
    files_to_send.extend(extra_media_files)

    if uses_components_v2:
        details_lines = [
            f"## {title_emoji} {title_text}{counter}\n"
        ]

        details_lines.append(
            f'### __**{level_name}**__ by **{level_creator}** `[{level_id}]`',
        )

        if headline_text:
            details_lines.append(f"-# {headline_text}")
        details_text = "\n".join(details_lines)

        view = discord.ui.LayoutView(timeout=None)
        container_kwargs = {}
        if accent_colour is not None:
            container_kwargs["accent_colour"] = accent_colour
        container = discord.ui.Container(**container_kwargs)

        if badge_file is not None:
            body_item = discord.ui.Section(
                discord.ui.TextDisplay(details_text),
                accessory=discord.ui.Thumbnail(
                    badge_file.uri,
                    description=f"{title_text} badge",
                ),
            )
        else:
            body_item = discord.ui.TextDisplay(details_text)

        container.add_item(body_item)

        # Result screenshots/photos are intentionally placed after feedback/note.
        # If there is no note, this naturally places them right after the main result text.
        if extra_media_files and _supports_staff_media_gallery():
            gallery = discord.ui.MediaGallery()
            for index, media_file in enumerate(extra_media_files, start=1):
                gallery.add_item(
                    media=f"attachment://{media_file.filename}",
                    description=f"Attached image {index}",
                )
            container.add_item(discord.ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small))
            container.add_item(gallery)

        if note_text:
            container.add_item(discord.ui.TextDisplay(f"```{note_text}```"))

        container.add_item(discord.ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small))
        footer_text = f"**{checker_label}: {checker_mention}**"
        if requester_mention:
            footer_text += f", **Requester: {requester_mention}**"
        container.add_item(discord.ui.TextDisplay(footer_text))
        view.add_item(container)

        send_kwargs = {
            "view": view,
            "allowed_mentions": allowed_mentions,
        }
        if len(files_to_send) == 1:
            send_kwargs["file"] = files_to_send[0]
        elif files_to_send:
            send_kwargs["files"] = files_to_send

        return await _safe_send_staff_payload(
            interaction,
            channel,
            send_kwargs,
            require_embed=False,
            require_file=requires_file,
        )

    embed = discord.Embed(
        title=title_text,
        description=headline_text,
        color=accent_colour if accent_colour is not None else discord.Colour.light_grey(),
    )
    embed.add_field(name="Level ID", value=f"`{level_id}`", inline=False)
    embed.add_field(name="Level", value=f'"{level_name}"', inline=False)
    if note_text:
        embed.add_field(name="Feedback", value=note_text, inline=False)
    if extra_media_files:
        embed.set_image(url=f"attachment://{extra_media_files[0].filename}")
    footer_text = f"{checker_label}: {checker_mention}"
    if requester_mention:
        footer_text += f" | Requester: {requester_mention}"
    embed.set_footer(text=footer_text)

    send_kwargs = {"embed": embed, "allowed_mentions": allowed_mentions}
    if len(files_to_send) == 1:
        send_kwargs["file"] = files_to_send[0]
    elif files_to_send:
        send_kwargs["files"] = files_to_send

    return await _safe_send_staff_payload(
        interaction,
        channel,
        send_kwargs,
        require_embed=True,
        require_file=requires_file,
    )


_CANDIDATE_VOTE_LOCKS = {}


def _get_candidate_vote_lock(candidate_request_id):
    try:
        key = int(candidate_request_id)
    except (TypeError, ValueError):
        key = str(candidate_request_id)

    lock = _CANDIDATE_VOTE_LOCKS.get(key)
    if lock is None:
        lock = asyncio.Lock()
        _CANDIDATE_VOTE_LOCKS[key] = lock
    return lock


async def _safe_message_edit(message, *, retries: int = 3, delay: float = 1.0, **kwargs):
    if message is None:
        return None

    last_error = None
    for attempt in range(retries):
        try:
            return await message.edit(**kwargs)
        except (discord.NotFound, discord.Forbidden) as edit_error:
            print(f"[SAFE MESSAGE EDIT] edit rejected for message={getattr(message, 'id', 'unknown')}: {edit_error}")
            return None
        except discord.DiscordServerError as edit_error:
            last_error = edit_error
            if attempt + 1 >= retries:
                break
            await asyncio.sleep(delay * (attempt + 1))
        except discord.HTTPException as edit_error:
            last_error = edit_error
            status = getattr(edit_error, "status", None)
            if status in (500, 502, 503, 504) and attempt + 1 < retries:
                await asyncio.sleep(delay * (attempt + 1))
                continue
            print(f"[SAFE MESSAGE EDIT] edit failed for message={getattr(message, 'id', 'unknown')}: {edit_error}")
            return None

    print(f"[SAFE MESSAGE EDIT] edit failed after {retries} retries for message={getattr(message, 'id', 'unknown')}: {last_error}")
    return None



class CandidateVoteSentModal(discord.ui.Modal):
    def __init__(self, candidate_request_id, server_id, event="1000", voting_message_id="0"):
        super().__init__(title="Candidate approval form", timeout=1200)
        self.candidate_request_id = int(candidate_request_id)
        self.server_id = int(server_id)
        self.event = normalize_event_name(event)
        self.voting_message_id = str(voting_message_id or "0")

        self._send_type_select_custom_id = "candidatevotesent_send_type_select"
        self._uses_modal_selects = hasattr(discord.ui, "Label")

        candidate_row = getcandidaterequestlinefromid(self.candidate_request_id)
        platformer = "No"
        if candidate_row is not None and len(candidate_row) > 10:
            platformer = str(candidate_row[10] or "No")

        if self._uses_modal_selects:
            if platformer == "Yes":
                send_type_specs = _STAFF_SEND_TYPE_OPTIONS_MOON
            else:
                send_type_specs = _STAFF_SEND_TYPE_OPTIONS

            self.level_opinion_select = discord.ui.Select(
                custom_id=self._send_type_select_custom_id,
                placeholder="Select approval type (optional)",
                min_values=0,
                max_values=1,
                required=False,
                options=send_type_specs,
            )
            self.level_opinion = discord.ui.Label(
                text="Approval type",
                component=self.level_opinion_select,
            )

        else:
            self.level_opinion = discord.ui.TextInput(
                label="Approval type",
                placeholder="e.g. Star Rate / Featured / Epic / Legendary / Mythic",
                required=False,
                max_length=20,
                style=discord.TextStyle.short,
            )

        self.level_review = discord.ui.TextInput(
            label="Note",
            required=False,
            max_length=1500,
            style=discord.TextStyle.paragraph,
        )

        self.add_item(self.level_opinion)
        self.add_item(self.level_difficulty)
        self.add_item(self.level_review)

    def _extract_modal_select_value(self, interaction: discord.Interaction, custom_id: str, select_obj=None) -> str:
        try:
            if select_obj is not None:
                selected_values = getattr(select_obj, "values", None)
                if selected_values:
                    return str(selected_values[0]).strip()
        except Exception:
            pass

        def _walk_components(components):
            if not isinstance(components, list):
                return None
            for component in components:
                if not isinstance(component, dict):
                    continue
                if component.get("custom_id") == custom_id:
                    values = component.get("values") or []
                    if values:
                        return str(values[0]).strip()
                    return ""
                found = _walk_components(component.get("components"))
                if found is not None:
                    return found
            return None

        interaction_data = getattr(interaction, "data", None)
        if isinstance(interaction_data, dict):
            found = _walk_components(interaction_data.get("components"))
            if found is not None:
                return found
        return ""

    def _get_send_type_value(self, interaction: discord.Interaction) -> str:
        if not getattr(self, "_uses_modal_selects", False):
            return str(getattr(self.level_opinion, "value", "") or "").strip()
        return self._extract_modal_select_value(interaction, self._send_type_select_custom_id, getattr(self, "level_opinion_select", None))

    def _get_difficulty_value(self, interaction: discord.Interaction) -> str:
        return ""

    @staticmethod
    def _safe_positive_int(value, default: int = 1) -> int:
        try:
            normalized = int(str(value).strip())
        except (TypeError, ValueError):
            return default
        return normalized if normalized > 0 else default

    async def _get_voting_message(self):
        message_id = self.voting_message_id
        if message_id in (None, "", "0"):
            message_id = getcandidaterequestparameterfromid(self.candidate_request_id, "MessageIDVoting")
        try:
            message_id_int = int(message_id)
        except (TypeError, ValueError):
            return None

        try:
            channel = bot.get_channel(int(getserverparameterfromserverid(str(self.server_id), "CandidateVoteChannel", event=self.event)))
            if channel is None:
                return None
            return await channel.fetch_message(message_id_int)
        except Exception:
            return None

    async def _approve_candidate(self, interaction: discord.Interaction):
        closecandidaterequest(self.candidate_request_id, "Approved")
        sheet_request_id = write_sheet_from_candidate(self.candidate_request_id)
        candidate_row = getcandidaterequestlinefromid(self.candidate_request_id)
        voting_message = await self._get_voting_message()
        if candidate_row is None or sheet_request_id is None:
            if voting_message is not None:
                await _safe_message_edit(voting_message, embed=candidate_request_embed(self.candidate_request_id, "Approved"), view=None)
            return

        everyone_channel = None
        try:
            everyone_channel = bot.get_channel(int(getserverparameterfromserverid(str(self.server_id), "ChannelEveryone", event=self.event)))
        except (TypeError, ValueError):
            everyone_channel = None
        if everyone_channel is not None and await ensure_send_permissions(interaction, everyone_channel, "Bot cannot send public request embed after candidate vote", require_embed=True):
            everyone_msg = await safe_channel_send(everyone_channel, embed=request_embed_2(sheet_request_id, 1))
            if everyone_msg is not None:
                editsheetparameterfromreqid(sheet_request_id, "MessageIDEveryone", str(everyone_msg.id))
                editcandidaterequestparameterfromid(self.candidate_request_id, "MessageIDEveryone", str(everyone_msg.id))

        helpers_channel = bot.get_channel(int(getserverparameterfromserverid(str(self.server_id), "ChannelHelpers", event=self.event)))
        if await ensure_send_permissions(interaction, helpers_channel, "Bot cannot send private request embed after candidate vote", require_embed=True):
            helper_msg = await safe_channel_send(
                helpers_channel,
                embed=request_embed(sheet_request_id, "0"),
                view=ReactionView(RequestID=sheet_request_id, iscommand=0, msgid=0, chanid=0),
            )
            if helper_msg is not None:
                editsheetparameterfromreqid(sheet_request_id, "MessageIDHelper", str(helper_msg.id))
                editcandidaterequestparameterfromid(self.candidate_request_id, "MessageIDHelper", str(helper_msg.id))
                writebuttonline(str(self.server_id), "0", "0", 0, str(helper_msg.id), int(sheet_request_id), Event=self.event)

        if voting_message is not None:
            await _safe_message_edit(voting_message, embed=candidate_request_embed(self.candidate_request_id, "Approved"), view=None)

    async def on_submit(self, interaction: discord.Interaction):
        await interaction.response.defer(ephemeral=True)

        async with _get_candidate_vote_lock(self.candidate_request_id):
            return await self._on_submit_locked(interaction)

    async def _on_submit_locked(self, interaction: discord.Interaction):
        if normalize_event_name(self.event) != "1000":
            return await interaction.followup.send("Candidate voting is available only for event `1000`.", ephemeral=True)

        guild = bot.get_guild(int(self.server_id))
        if guild is None:
            return await interaction.followup.send("Server not found.", ephemeral=True)

        voter_role_id = getserverparameterfromserverid(str(self.server_id), "CandidateVoterRole", event=self.event)
        voter_role = None
        if voter_role_id not in (None, "", "0"):
            try:
                voter_role = guild.get_role(int(voter_role_id))
            except (TypeError, ValueError):
                voter_role = None
        if voter_role is None or not member_has_role_id(await resolve_guild_member(guild, interaction.user), voter_role.id):
            return await interaction.followup.send("You don't have permissions to vote on candidate requests.", ephemeral=True)

        candidate_row = getcandidaterequestlinefromid(self.candidate_request_id)
        if candidate_row is None:
            return await interaction.followup.send("Candidate request not found.", ephemeral=True)

        level_opinion_raw = self._get_send_type_value(interaction)
        if level_opinion_raw == "":
            level_opinion_raw = "none"
        level_review_value = str(getattr(self.level_review, "value", "") or "").strip()
        difficulty_raw = self._get_difficulty_value(interaction)

        vote_result = appendcandidaterequestvote(
            self.candidate_request_id,
            str(interaction.user.id),
            interaction.user.mention,
            "sent",
        )
        if not vote_result.get("ok"):
            reason = vote_result.get("reason")
            if reason == "own_request":
                return await interaction.followup.send("You cannot vote on your own request.", ephemeral=True)
            if reason == "already_voted":
                return await interaction.followup.send("You have already voted on this request.", ephemeral=True)
            if reason == "closed":
                return await interaction.followup.send("This candidate request is already closed.", ephemeral=True)
            return await interaction.followup.send("Failed to save your vote.", ephemeral=True)

        sent_required = self._safe_positive_int(
            getserverparameterfromserverid(str(self.server_id), "CandidateVoteSendsRequired", event=self.event),
            default=1,
        )
        not_sent_required = self._safe_positive_int(
            getserverparameterfromserverid(str(self.server_id), "CandidateVoteNotSendsRequired", event=self.event),
            default=1,
        )
        sent_count = int(vote_result.get("sent_count") or 0)
        not_sent_count = int(vote_result.get("not_sent_count") or 0)

        badge_file = await _build_send_badge_for_request(
            ["" if value is None else str(value) for value in candidate_row],
            send_type_raw=level_opinion_raw,
            difficulty_raw=difficulty_raw,
            filename_prefix=f"candidate_{self.candidate_request_id}_approval",
        )

        result_channel = None
        try:
            result_channel = bot.get_channel(int(getserverparameterfromserverid(str(self.server_id), "CandidateVoteSentToChannel", event=self.event)))
        except (TypeError, ValueError):
            result_channel = None

        candidate_row = getcandidaterequestlinefromid(self.candidate_request_id)
        candidate_row_str = ["" if value is None else str(value) for value in candidate_row]
        title_emoji = "<:sent:1155722807037149224>"
        title_text = "Approved"
        headline = f"Approvals: {sent_count}/{sent_required}. Declines: {not_sent_count}/{not_sent_required}."
        await _send_staff_components_message(
            result_channel,
            interaction=interaction,
            counter="",
            title_emoji=title_emoji,
            title_text=title_text,
            headline_text=headline,
            level_name=candidate_row_str[2],
            level_creator=candidate_row_str[4],
            level_id=candidate_row_str[3],
            checker_label="Voter",
            checker_mention=interaction.user.mention,
            requester_mention=candidate_row_str[1],
            note_text=level_review_value,
            accent_colour=discord.Color.from_rgb(31, 253, 29),
            badge_file=badge_file,
        )

        voting_message = await self._get_voting_message()
        if sent_count >= sent_required:
            await self._approve_candidate(interaction)
            return

        if voting_message is not None:
            await _safe_message_edit(voting_message, embed=candidate_request_embed(self.candidate_request_id))
        return await interaction.followup.send(
            f"Approval saved. Current score: {sent_count}/{sent_required} approvals, {not_sent_count}/{not_sent_required} declines.",
            ephemeral=True,
        )


class CandidateVoteNotSentModal(discord.ui.Modal, title="Candidate decline form"):
    def __init__(self, candidate_request_id, server_id, event="1000", voting_message_id="0"):
        super().__init__(timeout=1200)
        self.candidate_request_id = int(candidate_request_id)
        self.server_id = int(server_id)
        self.event = normalize_event_name(event)
        self.voting_message_id = str(voting_message_id or "0")

    level_review = discord.ui.TextInput(label="Note", required=False, max_length=1500, style=discord.TextStyle.paragraph)

    @staticmethod
    def _safe_positive_int(value, default: int = 1) -> int:
        try:
            normalized = int(str(value).strip())
        except (TypeError, ValueError):
            return default
        return normalized if normalized > 0 else default

    async def _get_voting_message(self):
        message_id = self.voting_message_id
        if message_id in (None, "", "0"):
            message_id = getcandidaterequestparameterfromid(self.candidate_request_id, "MessageIDVoting")
        try:
            message_id_int = int(message_id)
        except (TypeError, ValueError):
            return None

        try:
            channel = bot.get_channel(int(getserverparameterfromserverid(str(self.server_id), "CandidateVoteChannel", event=self.event)))
            if channel is None:
                return None
            return await channel.fetch_message(message_id_int)
        except Exception:
            return None

    async def _reject_candidate(self):
        closecandidaterequest(self.candidate_request_id, "Rejected")
        voting_message = await self._get_voting_message()
        if voting_message is not None:
            await _safe_message_edit(voting_message, embed=candidate_request_embed(self.candidate_request_id, "Rejected"), view=None)

    async def on_submit(self, interaction: discord.Interaction):
        await interaction.response.defer(ephemeral=True)

        async with _get_candidate_vote_lock(self.candidate_request_id):
            return await self._on_submit_locked(interaction)

    async def _on_submit_locked(self, interaction: discord.Interaction):
        if normalize_event_name(self.event) != "1000":
            return await interaction.followup.send("Candidate voting is available only for event `1000`.", ephemeral=True)

        guild = bot.get_guild(int(self.server_id))
        if guild is None:
            return await interaction.followup.send("Server not found.", ephemeral=True)

        voter_role_id = getserverparameterfromserverid(str(self.server_id), "CandidateVoterRole", event=self.event)
        voter_role = None
        if voter_role_id not in (None, "", "0"):
            try:
                voter_role = guild.get_role(int(voter_role_id))
            except (TypeError, ValueError):
                voter_role = None
        if voter_role is None or not member_has_role_id(await resolve_guild_member(guild, interaction.user), voter_role.id):
            return await interaction.followup.send("You don't have permissions to vote on candidate requests.", ephemeral=True)

        candidate_row = getcandidaterequestlinefromid(self.candidate_request_id)
        if candidate_row is None:
            return await interaction.followup.send("Candidate request not found.", ephemeral=True)

        vote_result = appendcandidaterequestvote(
            self.candidate_request_id,
            str(interaction.user.id),
            interaction.user.mention,
            "not_sent",
        )
        if not vote_result.get("ok"):
            reason = vote_result.get("reason")
            if reason == "own_request":
                return await interaction.followup.send("You cannot vote on your own request.", ephemeral=True)
            if reason == "already_voted":
                return await interaction.followup.send("You have already voted on this request.", ephemeral=True)
            if reason == "closed":
                return await interaction.followup.send("This candidate request is already closed.", ephemeral=True)
            return await interaction.followup.send("Failed to save your vote.", ephemeral=True)

        sent_required = self._safe_positive_int(
            getserverparameterfromserverid(str(self.server_id), "CandidateVoteSendsRequired", event=self.event),
            default=1,
        )
        not_sent_required = self._safe_positive_int(
            getserverparameterfromserverid(str(self.server_id), "CandidateVoteNotSendsRequired", event=self.event),
            default=1,
        )
        sent_count = int(vote_result.get("sent_count") or 0)
        not_sent_count = int(vote_result.get("not_sent_count") or 0)
        note_value = str(getattr(self.level_review, "value", "") or "").strip()

        badge_file = await _build_unrate_badge(f"candidate_{self.candidate_request_id}_decline", modd="0")

        result_channel = None
        try:
            result_channel = bot.get_channel(int(getserverparameterfromserverid(str(self.server_id), "CandidateVoteNotSentToChannel", event=self.event)))
        except (TypeError, ValueError):
            result_channel = None

        candidate_row = getcandidaterequestlinefromid(self.candidate_request_id)
        candidate_row_str = ["" if value is None else str(value) for value in candidate_row]
        headline = f"Approvals: {sent_count}/{sent_required}. Declines: {not_sent_count}/{not_sent_required}."
        await _send_staff_components_message(
            result_channel,
            interaction=interaction,
            counter="",
            title_emoji="<:not_sent:1155722772367028244>",
            title_text="Declined",
            headline_text=headline,
            level_name=candidate_row_str[2],
            level_creator=candidate_row_str[4],
            level_id=candidate_row_str[3],
            checker_label="Voter",
            checker_mention=interaction.user.mention,
            requester_mention=candidate_row_str[1],
            note_text=note_value,
            accent_colour=discord.Color.from_rgb(253, 29, 31),
            badge_file=badge_file,
        )

        voting_message = await self._get_voting_message()
        if not_sent_count >= not_sent_required:
            await self._reject_candidate()
            return

        if voting_message is not None:
            await _safe_message_edit(voting_message, embed=candidate_request_embed(self.candidate_request_id))
        return await interaction.followup.send(
            f"Decline saved. Current score: {sent_count}/{sent_required} approvals, {not_sent_count}/{not_sent_required} declines.",
            ephemeral=True,
        )


async def _update_stats_message_safe(finalid, event_name):
    try:
        ssss = int(getserverparameterfromserverid(str(finalid), "StatsMessageID", event=event_name))
        if ssss != 0:
            channel2 = bot.get_channel(int(getserverparameterfromserverid(str(finalid), "StatsChannelID", event=event_name)))
            if channel2 is None:
                return
            msgg = await channel2.fetch_message(int(ssss))
            info_embed2 = stats_embed(finalid, event=event_name)
            if info_embed2 is not None:
                await _safe_message_edit(msgg, embed=info_embed2)
    except Exception as e:
        print(f"[STAFF FORM] stats update skipped/failed: {e}")


def _geode_moderator_difficulty_choice(stars: int) -> str:
    try:
        stars_value = int(stars)
    except (TypeError, ValueError):
        stars_value = 0

    mapping = {
        1: ("auto", "Auto"),
        2: ("easy", "Easy"),
        3: ("normal", "Normal"),
        4: ("hard", "Hard"),
        5: ("hard", "Hard"),
        6: ("harder", "Harder"),
        7: ("harder", "Harder"),
        8: ("insane", "Insane"),
        9: ("insane", "Insane"),
        10: ("demon-hard", "Demon"),
    }
    base_kind, label = mapping.get(stars_value, ("unrated", "Unrated"))
    if stars_value <= 0:
        return ""
    return f"{base_kind}|{stars_value}|{label}, {stars_value} Stars/Moons"


async def process_geode_moderator_request_send(
    *,
    server_id: int | str,
    event_name: str,
    request_id: int,
    moderator,
    send_type_raw: str,
    stars: int,
    feedback: str = "",
):
    """Apply a Geode moderator send using the normal moderator request-result behavior.

    This intentionally behaves like choosing Overwrite on a second moderator result:
    the Discord result is posted first, then any previous moderator result/not-send
    records for this same Discord user are cleared, and the new typed send is stored.
    """
    event_name = normalize_event_name(event_name)
    try:
        y = int(request_id)
    except (TypeError, ValueError):
        return {"handled": False, "reason": "invalid_request_id"}

    row = getsheetlinefromreqid(y)
    if not row:
        return {"handled": False, "reason": "request_not_found"}
    a = ["" if value is None else str(value) for value in row]

    if len(a) <= _SHEET_IDX_SERVER_ID or str(a[_SHEET_IDX_SERVER_ID]) != str(server_id):
        return {"handled": False, "reason": "request_server_mismatch"}

    row_event_name = normalize_event_name(a[_SHEET_IDX_EVENT] if len(a) > _SHEET_IDX_EVENT else "0")
    if row_event_name.lower() != event_name.lower():
        return {"handled": False, "reason": "request_event_mismatch"}

    guild = bot.get_guild(int(server_id))
    if guild is None:
        return {"handled": False, "reason": "discord_server_not_available"}

    moderator_role_id = getserverparameterfromserverid(str(server_id), "ModeratorRole", event=event_name)
    if not member_has_role_id(moderator, moderator_role_id):
        return {"handled": False, "reason": "moderator_role_missing_for_event"}

    raw_channel_id = getserverparameterfromserverid(str(server_id), "ModSendChannel", event=event_name)
    try:
        mod_send_channel_id = int(raw_channel_id)
    except (TypeError, ValueError):
        mod_send_channel_id = 0
    if mod_send_channel_id <= 0:
        return {"handled": False, "reason": "mod_send_channel_not_configured"}

    channelmod = bot.get_channel(mod_send_channel_id) or guild.get_channel(mod_send_channel_id)
    if channelmod is None or getattr(getattr(channelmod, "guild", None), "id", None) != guild.id:
        return {"handled": False, "reason": "mod_send_channel_not_available"}

    difficulty_raw = _geode_moderator_difficulty_choice(stars)
    send_type_for_storage = _send_type_storage_value(send_type_raw, default="")
    display_send_type_raw = _send_type_display_raw_for_request(send_type_raw, a)

    badge_file = await _build_send_badge_for_request(
        a,
        send_type_raw=display_send_type_raw,
        difficulty_raw=difficulty_raw,
        filename_prefix=f"request_{y}_geode_moderator_accept",
    )
    if badge_file is None:
        badge_file = await _build_rate_badge(f"request_{y}_geode_moderator_accept", modd="1")

    send_count = int(getserverparameterfromserverid(str(server_id), "ModSendsCount", event=event_name))
    sendfin = f" #{send_count}" if send_count != 0 else ""
    title_emoji, title_text = _compose_send_title_parts_for_request(
        send_type_raw,
        a,
        default_emoji="<:rate_3:1290321896361164842>",
    )

    result_message = await _send_staff_components_message(
        channelmod,
        interaction=None,
        counter=sendfin,
        title_emoji=title_emoji,
        title_text=title_text,
        headline_text="Your level has been sent to RobTop!",
        level_name=a[2] if len(a) > 2 else "Unknown",
        level_creator=a[4] if len(a) > 4 else "Unknown",
        level_id=a[3] if len(a) > 3 else "0",
        checker_label="Moderator",
        checker_mention=moderator.mention,
        requester_mention=a[1] if len(a) > 1 else None,
        note_text=str(feedback or "").strip()[:1500],
        accent_colour=None,
        badge_file=badge_file,
        extra_media_files=None,
        allow_requester_ping=True,
    )
    if result_message is None:
        return {"handled": False, "reason": "mod_send_publish_failed"}

    # Match normal moderator result numbering, but mutate only after the message exists.
    if send_count != 0:
        editserverparameterfromserverid(str(server_id), "ModSendsCount", send_count + 1, event=event_name)

    overwritten_records = _clear_existing_staff_records_on_success(y, moderator, "moderator")
    _append_sheet_send_type_actor(y, "moderator", moderator.mention, send_type_for_storage)

    private_request_updated = False
    try:
        raw_helpers_channel_id = getserverparameterfromserverid(str(server_id), "ChannelHelpers", event=event_name)
        helpers_channel_id = int(raw_helpers_channel_id)
        channel_helpers = bot.get_channel(helpers_channel_id) or guild.get_channel(helpers_channel_id)
        private_message_id = int(a[12]) if len(a) > 12 and a[12] not in ("", "0", "None") else 0
        if channel_helpers is not None and private_message_id > 0:
            private_message = await channel_helpers.fetch_message(private_message_id)
            await _safe_message_edit(private_message, embed=request_embed(y, "0"))
            private_request_updated = True
    except Exception as exc:
        print(f"[GEODE REQUEST RESULT] private request update failed for {y}: {exc}")

    reaction_added = False
    try:
        if check_configs(str(server_id), 1, event=event_name):
            await AddReaction(y, str(server_id))
            reaction_added = True
    except Exception as exc:
        print(f"[GEODE REQUEST RESULT] reaction update failed for {y}: {exc}")

    return {
        "handled": True,
        "reason": "request_result",
        "request_id": y,
        "event": event_name,
        "message": result_message,
        "overwritten_records": overwritten_records,
        "private_request_updated": private_request_updated,
        "reaction_added": reaction_added,
    }


async def _geode_request_update_private_and_reaction(
    *,
    request_id: int,
    server_id: int | str,
    event_name: str,
    request_row: list[str],
):
    guild = bot.get_guild(int(server_id))
    private_request_updated = False
    try:
        raw_helpers_channel_id = getserverparameterfromserverid(str(server_id), "ChannelHelpers", event=event_name)
        helpers_channel_id = int(raw_helpers_channel_id)
        channel_helpers = bot.get_channel(helpers_channel_id) or (guild.get_channel(helpers_channel_id) if guild is not None else None)
        private_message_id = int(request_row[12]) if len(request_row) > 12 and request_row[12] not in ("", "0", "None") else 0
        if channel_helpers is not None and private_message_id > 0:
            private_message = await channel_helpers.fetch_message(private_message_id)
            await _safe_message_edit(private_message, embed=request_embed(request_id, "0"))
            private_request_updated = True
    except Exception as exc:
        print(f"[GEODE REQUEST RESULT] private request update failed for {request_id}: {exc}")

    reaction_added = False
    try:
        if check_configs(str(server_id), 1, event=event_name):
            await AddReaction(request_id, str(server_id))
            reaction_added = True
    except Exception as exc:
        print(f"[GEODE REQUEST RESULT] reaction update failed for {request_id}: {exc}")
    return private_request_updated, reaction_added


def _geode_request_row_and_event(server_id: int | str, request_id: int):
    row = getsheetlinefromreqid(int(request_id))
    if not row:
        return None, None
    values = ["" if value is None else str(value) for value in row]
    if len(values) <= _SHEET_IDX_SERVER_ID or str(values[_SHEET_IDX_SERVER_ID]) != str(server_id):
        return None, None
    event_name = normalize_event_name(values[_SHEET_IDX_EVENT] if len(values) > _SHEET_IDX_EVENT else "0")
    return values, event_name


async def process_geode_helper_request_send(
    *,
    server_id: int | str,
    request_id: int,
    helper,
    send_type_raw: str,
    stars: int,
    feedback: str = "",
):
    """Submit the fake in-game Helper send using the same persistent result fields as Discord."""
    try:
        y = int(request_id)
    except (TypeError, ValueError):
        return {"handled": False, "reason": "invalid_request_id"}

    a, event_name = _geode_request_row_and_event(server_id, y)
    if a is None:
        return {"handled": False, "reason": "request_not_found_or_server_mismatch"}

    guild = bot.get_guild(int(server_id))
    if guild is None:
        return {"handled": False, "reason": "discord_server_not_available"}
    helper_role_id = getserverparameterfromserverid(str(server_id), "HelperRole", event=event_name)
    if not member_has_role_id(helper, helper_role_id):
        return {"handled": False, "reason": "helper_role_missing_for_event"}

    raw_channel_id = getserverparameterfromserverid(str(server_id), "SentToChannel", event=event_name)
    try:
        channel_id = int(raw_channel_id)
    except (TypeError, ValueError):
        channel_id = 0
    if channel_id <= 0:
        return {"handled": False, "reason": "helper_send_channel_not_configured"}
    channel = bot.get_channel(channel_id) or guild.get_channel(channel_id)
    if channel is None or getattr(getattr(channel, "guild", None), "id", None) != guild.id:
        return {"handled": False, "reason": "helper_send_channel_not_available"}

    difficulty_raw = _geode_moderator_difficulty_choice(stars)
    send_type_for_storage = _send_type_storage_value(send_type_raw, default="")
    display_send_type_raw = _send_type_display_raw_for_request(send_type_raw, a)
    badge_file = await _build_send_badge_for_request(
        a,
        send_type_raw=display_send_type_raw,
        difficulty_raw=difficulty_raw,
        filename_prefix=f"request_{y}_geode_helper_accept",
    )

    result_message = await _send_staff_components_message(
        channel,
        interaction=None,
        counter="",
        title_emoji="<:sent:1155722807037149224>",
        title_text="Sent",
        headline_text="Your level will be sent to moderators!",
        level_name=a[2] if len(a) > 2 else "Unknown",
        level_creator=a[4] if len(a) > 4 else "Unknown",
        level_id=a[3] if len(a) > 3 else "0",
        checker_label="Helper",
        checker_mention=helper.mention,
        requester_mention=a[1] if len(a) > 1 else None,
        note_text=str(feedback or "").strip()[:1500],
        accent_colour=discord.Color.from_rgb(31, 253, 29),
        badge_file=badge_file,
        extra_media_files=None,
        allow_requester_ping=True,
    )
    if result_message is None:
        return {"handled": False, "reason": "helper_send_publish_failed"}

    try:
        await _update_stats_message_safe(server_id, event_name)
    except Exception:
        pass
    overwritten_records = _clear_existing_staff_records_on_success(y, helper, "helper")
    _append_sheet_send_type_actor(y, "helper", helper.mention, send_type_for_storage)
    private_request_updated, reaction_added = await _geode_request_update_private_and_reaction(
        request_id=y,
        server_id=server_id,
        event_name=event_name,
        request_row=a,
    )
    return {
        "handled": True,
        "reason": "request_result",
        "request_id": y,
        "event": event_name,
        "message": result_message,
        "overwritten_records": overwritten_records,
        "private_request_updated": private_request_updated,
        "reaction_added": reaction_added,
    }


async def process_geode_request_reject(
    *,
    server_id: int | str,
    request_id: int,
    actor,
    staff_kind: str,
    reason: str = "",
    feedback: str = "",
):
    """Submit Helper/Moderator reject from the in-game Requests context.

    The Geode client intentionally does not expose Wrong ID, but the Discord-side
    schema keeps supporting it. Old records are cleared only after the new Discord
    result message was published, matching the normal Overwrite flow.
    """
    kind = "moderator" if str(staff_kind or "").strip().lower() == "moderator" else "helper"
    try:
        y = int(request_id)
    except (TypeError, ValueError):
        return {"handled": False, "reason": "invalid_request_id"}

    a, event_name = _geode_request_row_and_event(server_id, y)
    if a is None:
        return {"handled": False, "reason": "request_not_found_or_server_mismatch"}
    guild = bot.get_guild(int(server_id))
    if guild is None:
        return {"handled": False, "reason": "discord_server_not_available"}

    role_key = "ModeratorRole" if kind == "moderator" else "HelperRole"
    role_id = getserverparameterfromserverid(str(server_id), role_key, event=event_name)
    if not member_has_role_id(actor, role_id):
        return {"handled": False, "reason": f"{kind}_role_missing_for_event"}

    normalized_reason = _normalize_not_send_reason_value(reason)
    if normalized_reason == "wrong_id":
        return {"handled": False, "reason": "wrong_id_not_available_in_geode"}
    if normalized_reason not in {"", "already_seen", "already_rated", "report"}:
        return {"handled": False, "reason": "invalid_reject_reason"}

    channel_key = "ModNotSendChannel" if kind == "moderator" else "NotSentToChannel"
    raw_channel_id = getserverparameterfromserverid(str(server_id), channel_key, event=event_name)
    try:
        channel_id = int(raw_channel_id)
    except (TypeError, ValueError):
        channel_id = 0
    if channel_id <= 0:
        return {"handled": False, "reason": f"{kind}_reject_channel_not_configured"}
    channel = bot.get_channel(channel_id) or guild.get_channel(channel_id)
    if channel is None or getattr(getattr(channel, "guild", None), "id", None) != guild.id:
        return {"handled": False, "reason": f"{kind}_reject_channel_not_available"}

    title_text = _not_send_reason_display(normalized_reason) if normalized_reason else "Not Sent"
    title_emoji = _not_send_reason_emoji(normalized_reason) if normalized_reason else "<:not_sent:1155722772367028244>"
    badge_file = None
    if kind == "helper" or not normalized_reason:
        badge_file = await _build_unrate_badge(
            f"request_{y}_geode_{kind}_reject",
            modd="1" if kind == "moderator" else "0",
        )

    counter = ""
    if kind == "moderator" and not normalized_reason:
        notsend = int(getserverparameterfromserverid(str(server_id), "ModNotSendsCount", event=event_name))
        if notsend != 0:
            counter = f" #{notsend}"

    result_message = await _send_staff_components_message(
        channel,
        interaction=None,
        counter=counter,
        title_emoji=(title_emoji if normalized_reason or kind == "helper" else "<:unrate_3:1290328913314316413>"),
        title_text=title_text,
        headline_text="Unfortunately, your level has not been sent...",
        level_name=a[2] if len(a) > 2 else "Unknown",
        level_creator=a[4] if len(a) > 4 else "Unknown",
        level_id=a[3] if len(a) > 3 else "0",
        checker_label="Moderator" if kind == "moderator" else "Helper",
        checker_mention=actor.mention,
        requester_mention=a[1] if len(a) > 1 else None,
        note_text=str(feedback or "").strip()[:1500],
        accent_colour=(None if normalized_reason or kind == "moderator" else discord.Color.from_rgb(253, 29, 31)),
        badge_file=badge_file,
        extra_media_files=None,
        allow_requester_ping=True,
    )
    if result_message is None:
        return {"handled": False, "reason": f"{kind}_reject_publish_failed"}

    if kind == "moderator" and not normalized_reason:
        notsend = int(getserverparameterfromserverid(str(server_id), "ModNotSendsCount", event=event_name))
        if notsend != 0:
            editserverparameterfromserverid(str(server_id), "ModNotSendsCount", notsend + 1, event=event_name)
    if kind == "helper":
        try:
            await _update_stats_message_safe(server_id, event_name)
        except Exception:
            pass

    overwritten_records = _clear_existing_staff_records_on_success(y, actor, kind)
    _append_sheet_not_send_reason_actor(y, kind, actor.mention, normalized_reason)
    private_request_updated, reaction_added = await _geode_request_update_private_and_reaction(
        request_id=y,
        server_id=server_id,
        event_name=event_name,
        request_row=a,
    )
    return {
        "handled": True,
        "reason": "request_result",
        "request_id": y,
        "event": event_name,
        "message": result_message,
        "overwritten_records": overwritten_records,
        "private_request_updated": private_request_updated,
        "reaction_added": reaction_added,
    }


class RequestModalAccept(discord.ui.Modal):
    def __init__(
        self,
        RequestID,
        iscommand,
        msgid,
        chanid,
        event="0",
        staff_kind: str = "helper",
        clear_existing_records_on_submit: bool = False,
        clear_existing_records_staff_kind: str | None = None,
    ):
        super().__init__(title=("Moderator sent form" if str(staff_kind or "helper").lower() == "moderator" else "Helper sent form"), timeout=1200)
        self.RequestID = RequestID
        self.iscommand = iscommand
        self.msgid = msgid
        self.chanid = chanid
        self.event = normalize_event_name(event)
        self.staff_kind = "moderator" if str(staff_kind or "helper").lower() == "moderator" else "helper"
        self.clear_existing_records_on_submit = bool(clear_existing_records_on_submit)
        self.clear_existing_records_staff_kind = (
            "moderator" if str(clear_existing_records_staff_kind or self.staff_kind).lower() == "moderator" else "helper"
        )

        self._send_type_select_custom_id = "requestmodalaccept_send_type_select"
        self._difficulty_select_custom_id = "requestmodalaccept_difficulty_select"
        self._ping_checkbox_custom_id = "requestmodalaccept_no_ping_checkbox"
        self._uses_modal_selects = hasattr(discord.ui, "Label")

        if self._uses_modal_selects:
            platformer = getsheetparameterfromreqid(self.RequestID, "IsPlatformer")
            if platformer == "Yes":
                send_type_specs = _STAFF_SEND_TYPE_OPTIONS_MOON
            else:
                send_type_specs = _STAFF_SEND_TYPE_OPTIONS
            self.level_opinion_select = discord.ui.Select(
                custom_id=self._send_type_select_custom_id,
                placeholder="Select type of the send (optional)",
                min_values=0,
                max_values=1,
                required=False,
                options=send_type_specs,
            )
            self.level_opinion = discord.ui.Label(
                text="Type of the send",
                component=self.level_opinion_select,
            )

            if self.staff_kind == "helper":
                difficulty_specs = _HELPER_DIFFICULTY_OPTION_SPECS_MOON if platformer == "Yes" else _HELPER_DIFFICULTY_OPTION_SPECS
            else:
                difficulty_specs = _MODERATOR_DIFFICULTY_OPTION_SPECS_MOON if platformer == "Yes" else _MODERATOR_DIFFICULTY_OPTION_SPECS

            self.level_difficulty_select = discord.ui.Select(
                custom_id=self._difficulty_select_custom_id,
                placeholder="Select difficulty (optional)",
                min_values=0,
                max_values=1,
                required=False,
                options=_build_select_options(difficulty_specs),
            )
            self.level_difficulty = discord.ui.Label(
                text="Difficulty",
                component=self.level_difficulty_select,
            )
            self._uses_ping_checkbox = hasattr(discord.ui, "Checkbox")
            if self._uses_ping_checkbox:
                self.no_requester_ping_checkbox = discord.ui.Checkbox(
                    custom_id=self._ping_checkbox_custom_id,
                    default=False,
                )
                self.ping_control = discord.ui.Label(
                    text="Do not ping requester",
                    description="Leave unchecked to ping as usual.",
                    component=self.no_requester_ping_checkbox,
                )
            else:
                self.ping_control = discord.ui.TextInput(
                    label="Requester ping",
                    placeholder="Leave empty/yes = ping, no = do not ping",
                    required=False,
                    max_length=16,
                    style=discord.TextStyle.short,
                )
        else:
            self.level_opinion = discord.ui.TextInput(
                label="Type of the send",
                placeholder="e.g. Star Rate / Featured / Epic / Legendary / Mythic",
                required=False,
                max_length=20,
                style=discord.TextStyle.short,
            )
            self.level_difficulty = discord.ui.TextInput(
                label="Difficulty",
                placeholder="e.g. Hard, 4-5 Stars/Moons / Demon",
                required=False,
                max_length=64,
                style=discord.TextStyle.short,
            )
            self.ping_control = discord.ui.TextInput(
                label="Requester ping",
                placeholder="Leave empty/yes = ping, no = do not ping",
                required=False,
                max_length=16,
                style=discord.TextStyle.short,
            )

        self.level_review = discord.ui.TextInput(
            label="Note",
            required=False,
            max_length=1500,
            style=discord.TextStyle.paragraph,
        )

        self.result_photo_upload = None
        self.result_photo = None
        if _supports_modal_file_upload():
            self.result_photo_upload = discord.ui.FileUpload(
                custom_id="requestmodalaccept_result_photo",
                required=False,
                min_values=0,
                max_values=1,
            )
            self.result_photo = discord.ui.Label(
                text="Message image (optional)",
                description="Attach one image under the main text.",
                component=self.result_photo_upload,
            )

        self.add_item(self.level_opinion)
        self.add_item(self.level_difficulty)
        self.add_item(self.level_review)
        if self.result_photo is not None:
            self.add_item(self.result_photo)
        self.add_item(self.ping_control)

    def _extract_modal_select_value(self, interaction: discord.Interaction, custom_id: str, select_obj=None) -> str:
        try:
            if select_obj is not None:
                selected_values = getattr(select_obj, "values", None)
                if selected_values:
                    return str(selected_values[0]).strip()
        except Exception:
            pass

        def _walk_components(components):
            if not isinstance(components, list):
                return None
            for component in components:
                if not isinstance(component, dict):
                    continue
                if component.get("custom_id") == custom_id:
                    values = component.get("values") or []
                    if values:
                        return str(values[0]).strip()
                    return ""
                nested_components = component.get("components")
                found = _walk_components(nested_components)
                if found is not None:
                    return found
            return None

        interaction_data = getattr(interaction, "data", None)
        if isinstance(interaction_data, dict):
            found = _walk_components(interaction_data.get("components"))
            if found is not None:
                return found

        return ""

    def _get_send_type_value(self, interaction: discord.Interaction) -> str:
        if not getattr(self, "_uses_modal_selects", False):
            return str(getattr(self.level_opinion, "value", "") or "").strip()
        return self._extract_modal_select_value(interaction, self._send_type_select_custom_id, getattr(self, "level_opinion_select", None))

    def _get_difficulty_value(self, interaction: discord.Interaction) -> str:
        if not getattr(self, "_uses_modal_selects", False):
            return str(getattr(self.level_difficulty, "value", "") or "").strip()
        return self._extract_modal_select_value(interaction, self._difficulty_select_custom_id, getattr(self, "level_difficulty_select", None))

    def _get_ping_value(self, interaction: discord.Interaction) -> str:
        if getattr(self, "_uses_ping_checkbox", False):
            no_ping_checked = _extract_modal_checkbox_value(
                interaction,
                self._ping_checkbox_custom_id,
                getattr(self, "no_requester_ping_checkbox", None),
            )
            return "no_ping" if no_ping_checked else ""
        return str(getattr(self.ping_control, "value", "") or "").strip()

    async def on_submit(self, interaction: discord.Interaction):
        await interaction.response.defer()

        finalid = interaction.guild_id
        event_name = self.event
        if isinstance(interaction.channel, discord.DMChannel):
            linked_server_id = getdmusersparameterfromuserid(str(interaction.user.id), "ServerID")
            if linked_server_id and bot.get_guild(int(linked_server_id)) is not None:
                finalid = int(linked_server_id)
            else:
                return await interaction.followup.send("The server is not linked or linked wrong", ephemeral=True)

        level_opinion_raw = self._get_send_type_value(interaction)
        level_review_value = str(getattr(self.level_review, "value", "") or "").strip()
        difficulty_raw = self._get_difficulty_value(interaction)
        allow_requester_ping = _mention_requester_enabled(self._get_ping_value(interaction))
        uploaded_photos = await _extract_uploaded_image_files(self.result_photo_upload, max_count=1)

        guild = bot.get_guild(finalid)
        guild_member = await resolve_guild_member(guild, interaction.user)
        moderator_role_id = getserverparameterfromserverid(str(finalid), "ModeratorRole", event=event_name)
        helper_role_id = getserverparameterfromserverid(str(finalid), "HelperRole", event=event_name)
        is_moderator = member_has_role_id(guild_member, moderator_role_id)
        is_helper = member_has_role_id(guild_member, helper_role_id)

        modd = 0
        if is_moderator:
            if self.iscommand != 0 and not _is_moderator_queue_mode_value(self.iscommand):
                return await interaction.followup.send(
                    "You can only use moderator's Accept button or you have both Helper/Moderator roles...",
                    ephemeral=True,
                )
            modd = 1
            if not check_configs(str(finalid), 3, event=event_name):
                if guild_member is not None and guild_member.guild_permissions.administrator:
                    return await interaction.followup.send(embed=report_configs(str(finalid), 3, event=event_name), ephemeral=True)
                return await interaction.followup.send("Config error...", ephemeral=True)
        else:
            if not check_configs(str(finalid), 0, event=event_name):
                if guild_member is not None and guild_member.guild_permissions.administrator:
                    return await interaction.followup.send(embed=report_configs(str(finalid), 0, event=event_name), ephemeral=True)
                return await interaction.followup.send("Config error...", ephemeral=True)

        if is_helper and self.iscommand != 0 and not _is_helper_queue_mode_value(self.iscommand):
            return await interaction.followup.send(
                "You can only use helper's Accept button or you have both Helper/Moderator roles...",
                ephemeral=True,
            )

        y = int(self.RequestID)
        channel1 = bot.get_channel(int(getserverparameterfromserverid(str(finalid), "ChannelHelpers", event=event_name)))
        a = getsheetlinefromreqid(y)
        a = [str(x) for x in a]


        send_type_for_storage = _send_type_storage_value(level_opinion_raw, default="")
        display_send_type_raw = _send_type_display_raw_for_request(level_opinion_raw, a)
        badge_file = await _build_send_badge_for_request(a, send_type_raw=display_send_type_raw, difficulty_raw=difficulty_raw, filename_prefix=f"request_{y}_accept")
        if badge_file is None and modd == 1:
            badge_file = await _build_rate_badge(f"request_{y}_accept", modd=str(modd))

        if modd == 1:
            send = int(getserverparameterfromserverid(str(finalid), "ModSendsCount", event=event_name))
            sendfin = ""
            channelmod = bot.get_channel(int(getserverparameterfromserverid(str(finalid), "ModSendChannel", event=event_name)))
            if not await ensure_send_permissions(interaction, channelmod, "Bot cannot send messages in mod send channel", require_file=badge_file is not None):
                return

            if send != 0:
                sendfin = f' #{send}'
                editserverparameterfromserverid(str(finalid), "ModSendsCount", send + 1, event=event_name)

            title_emoji, title_text = _compose_send_title_parts_for_request(level_opinion_raw, a, default_emoji = "<:rate_3:1290321896361164842>")
            headline = f'Your level has been sent to RobTop!'
            await _send_staff_components_message(
                channelmod,
                interaction=interaction,
                counter=sendfin,
                title_emoji=title_emoji,
                title_text=title_text,
                headline_text=headline,
                level_name=a[2],
                level_creator=a[4],
                level_id=a[3],
                checker_label="Moderator",
                checker_mention=interaction.user.mention,
                requester_mention=a[1],
                note_text=level_review_value,
                accent_colour=None,
                badge_file=badge_file,
                extra_media_files=uploaded_photos,
                allow_requester_ping=allow_requester_ping,
            )
            _maybe_clear_existing_records_on_success(self, y, interaction.user)
            _append_sheet_send_type_actor(y, "moderator", interaction.user.mention, send_type_for_storage)
            try:
                msg = await channel1.fetch_message(int(a[12]))
                await _safe_message_edit(msg, embed=request_embed(y, "0"))
            except Exception:
                print("Request Deleted :(")
            if check_configs(str(finalid), 1, event=event_name):
                await AddReaction(y, str(finalid))

            if self.msgid != 0:
                try:
                    channel3 = bot.get_channel(self.chanid)
                    msg3 = await channel3.fetch_message(self.msgid)
                except Exception:
                    channel3 = await interaction.user.create_dm()
                    msg3 = await channel3.fetch_message(self.msgid)

                if _is_moderator_queue_mode(self.iscommand):
                    random_mode = _is_moderator_queue_random_mode(self.iscommand)
                    helper_approved_only = _is_moderator_queue_helper_approved_only(self.iscommand)
                    button_filters = _get_button_queue_filters(self.msgid)
                    next_request_id, remaining_requests_count = _find_next_moderator_request_with_count(
                        finalid,
                        interaction.user,
                        random_mode=random_mode,
                        helper_approved_only=helper_approved_only,
                        min_request_id=_get_button_min_request_id(self.msgid),
                        event=event_name,
                        platformer_filter=button_filters["platformer_filter"],
                        difficulty_filter="",
                        min_senddb=button_filters["min_senddb"],
                        max_senddb=button_filters["max_senddb"],
                    )

                    if next_request_id is None:
                        await _safe_message_edit(msg3, content="All requests have been checked!", suppress=True, view=None)
                        deletebuttonlinefrommessageid(str(msg3.id))
                    else:
                        await _safe_message_edit(msg3, 
                            embed=request_embed(next_request_id, "0", remaining_count=remaining_requests_count),
                            view=ReactionView(RequestID=next_request_id, iscommand=self.iscommand, msgid=msg3.id, chanid=interaction.channel_id),
                        )
                        editbuttonparameterfrommessageid(str(msg3.id), "RequestID", next_request_id)
                else:
                    await _safe_message_edit(msg3, embed=request_embed(y, "0"))
        else:
            channel = bot.get_channel(int(getserverparameterfromserverid(str(finalid), "SentToChannel", event=event_name)))
            if not await ensure_send_permissions(interaction, channel, "Bot cannot send messages in sent-to channel", require_file=badge_file is not None):
                return

            headline = f'Your level will be sent to moderators!'
            result_message = await _send_staff_components_message(
                channel,
                interaction=interaction,
                counter="",
                title_emoji="<:sent:1155722807037149224>",
                title_text="Sent",
                headline_text=headline,
                level_name=a[2],
                level_creator=a[4],
                level_id=a[3],
                checker_label="Helper",
                checker_mention=interaction.user.mention,
                requester_mention=a[1],
                note_text=level_review_value,
                accent_colour=discord.Color.from_rgb(31, 253, 29),
                badge_file=badge_file,
                extra_media_files=uploaded_photos,
                allow_requester_ping=allow_requester_ping,
            )
            if result_message is None:
                return
            await _update_stats_message_safe(finalid, event_name)
            _maybe_clear_existing_records_on_success(self, y, interaction.user)
            _append_sheet_send_type_actor(y, "helper", interaction.user.mention, send_type_for_storage)

            try:
                msg = await channel1.fetch_message(int(a[12]))
                await _safe_message_edit(msg, embed=request_embed(y, "0"))
            except Exception:
                print("Request Deleted :(")
            if check_configs(str(finalid), 1, event=event_name):
                await AddReaction(y, str(finalid))

            if self.msgid != 0:
                try:
                    channel3 = bot.get_channel(self.chanid)
                    msg3 = await channel3.fetch_message(self.msgid)
                except Exception:
                    channel3 = await interaction.user.create_dm()
                    msg3 = await channel3.fetch_message(self.msgid)
                if self.iscommand == 0:
                    await _safe_message_edit(msg3, embed=request_embed(y, "0"))
                else:
                    kol = 0
                    idd = interaction.user.id
                    random_mode = (self.iscommand != 1)
                    while kol == 0:
                        button_filters = _get_button_queue_filters(self.msgid)
                        request_id, remaining_requests_count = _find_next_helper_request_with_count(
                            finalid,
                            idd,
                            random_mode=random_mode,
                            min_request_id=_get_button_min_request_id(self.msgid),
                            event=event_name,
                            platformer_filter=button_filters["platformer_filter"],
                            difficulty_filter="",
                            min_senddb=button_filters["min_senddb"],
                            max_senddb=button_filters["max_senddb"],
                            max_sent_to=button_filters.get("max_sent_to"),
                        )
                        if request_id is None:
                            kol = 1
                            await _safe_message_edit(msg3, content="All requests have been checked!", suppress=True, view=None)
                            deletebuttonlinefrommessageid(str(msg3.id))
                        else:
                            kol = 1
                            await _safe_message_edit(msg3, 
                                embed=request_embed(request_id, "0", remaining_count=remaining_requests_count),
                                view=ReactionView(RequestID=request_id, iscommand=self.iscommand, msgid=msg3.id, chanid=interaction.channel_id),
                            )
                            editbuttonparameterfrommessageid(str(msg3.id), "RequestID", request_id)



class RequestModalReject(discord.ui.Modal, title='Not sent form'):
    def __init__(
        self,
        RequestID,
        iscommand,
        msgid,
        chanid,
        event="0",
        staff_kind: str = "helper",
        clear_existing_records_on_submit: bool = False,
        clear_existing_records_staff_kind: str | None = None,
    ):
        super().__init__(timeout=1200)
        self.RequestID = RequestID
        self.iscommand = iscommand
        self.msgid = msgid
        self.chanid = chanid
        self.event = normalize_event_name(event)
        self.staff_kind = "moderator" if str(staff_kind or "helper").lower() == "moderator" else "helper"
        self.clear_existing_records_on_submit = bool(clear_existing_records_on_submit)
        self.clear_existing_records_staff_kind = (
            "moderator" if str(clear_existing_records_staff_kind or self.staff_kind).lower() == "moderator" else "helper"
        )

        self._not_send_reason_select_custom_id = "requestmodalreject_not_send_reason_select"
        self._ping_checkbox_custom_id = "requestmodalreject_no_ping_checkbox"
        self._uses_modal_selects = hasattr(discord.ui, "Label")

        if self._uses_modal_selects:
            self.not_send_reason_select = discord.ui.Select(
                custom_id=self._not_send_reason_select_custom_id,
                placeholder="Select not-sent reason (optional)",
                min_values=0,
                max_values=1,
                required=False,
                options=_NOT_SEND_REASON_OPTIONS,
            )
            self.not_send_reason = discord.ui.Label(
                text="Not-sent reason",
                component=self.not_send_reason_select,
            )
            self._uses_ping_checkbox = hasattr(discord.ui, "Checkbox")
            if self._uses_ping_checkbox:
                self.no_requester_ping_checkbox = discord.ui.Checkbox(
                    custom_id=self._ping_checkbox_custom_id,
                    default=False,
                )
                self.ping_control = discord.ui.Label(
                    text="Do not ping requester",
                    description="Leave unchecked to ping as usual.",
                    component=self.no_requester_ping_checkbox,
                )
            else:
                self.ping_control = discord.ui.TextInput(
                    label="Requester ping",
                    placeholder="Leave empty/yes = ping, no = do not ping",
                    required=False,
                    max_length=16,
                    style=discord.TextStyle.short,
                )
        else:
            self.not_send_reason = discord.ui.TextInput(
                label="Not-sent reason",
                placeholder="Already Rated / Already Seen / Wrong ID / Report",
                required=False,
                max_length=32,
                style=discord.TextStyle.short,
            )
            self.ping_control = discord.ui.TextInput(
                label="Requester ping",
                placeholder="Leave empty/yes = ping, no = do not ping",
                required=False,
                max_length=16,
                style=discord.TextStyle.short,
            )

        self.level_review = discord.ui.TextInput(
            label="Note",
            required=False,
            max_length=1500,
            style=discord.TextStyle.paragraph,
        )
        self.result_photo_upload = None
        self.result_photo = None
        if _supports_modal_file_upload():
            self.result_photo_upload = discord.ui.FileUpload(
                custom_id="requestmodalreject_result_photo",
                required=False,
                min_values=0,
                max_values=1,
            )
            self.result_photo = discord.ui.Label(
                text="Message image (optional)",
                description="Attach one image under the main text.",
                component=self.result_photo_upload,
            )

        self.add_item(self.not_send_reason)
        self.add_item(self.level_review)
        if self.result_photo is not None:
            self.add_item(self.result_photo)
        self.add_item(self.ping_control)

    def _extract_modal_select_value(self, interaction: discord.Interaction, custom_id: str, select_obj=None) -> str:
        try:
            if select_obj is not None:
                selected_values = getattr(select_obj, "values", None)
                if selected_values:
                    return str(selected_values[0]).strip()
        except Exception:
            pass

        def _walk_components(components):
            if not isinstance(components, list):
                return None
            for component in components:
                if not isinstance(component, dict):
                    continue
                if component.get("custom_id") == custom_id:
                    values = component.get("values") or []
                    if values:
                        return str(values[0]).strip()
                    return ""
                found = _walk_components(component.get("components"))
                if found is not None:
                    return found
            return None

        interaction_data = getattr(interaction, "data", None)
        if isinstance(interaction_data, dict):
            found = _walk_components(interaction_data.get("components"))
            if found is not None:
                return found
        return ""

    def _get_not_send_reason_value(self, interaction: discord.Interaction) -> str:
        if not getattr(self, "_uses_modal_selects", False):
            return str(getattr(self.not_send_reason, "value", "") or "").strip()
        return self._extract_modal_select_value(interaction, self._not_send_reason_select_custom_id, getattr(self, "not_send_reason_select", None))

    def _get_ping_value(self, interaction: discord.Interaction) -> str:
        if getattr(self, "_uses_ping_checkbox", False):
            no_ping_checked = _extract_modal_checkbox_value(
                interaction,
                self._ping_checkbox_custom_id,
                getattr(self, "no_requester_ping_checkbox", None),
            )
            return "no_ping" if no_ping_checked else ""
        return str(getattr(self.ping_control, "value", "") or "").strip()

    async def on_submit(self, interaction: discord.Interaction):
        await interaction.response.defer()
        finalid = interaction.guild_id
        event_name = self.event
        if isinstance(interaction.channel, discord.DMChannel):
            linked_server_id = getdmusersparameterfromuserid(str(interaction.user.id), "ServerID")
            if linked_server_id and bot.get_guild(int(linked_server_id)) is not None:
                finalid = int(linked_server_id)
            else:
                return await interaction.followup.send("The server is not linked or linked wrong", ephemeral=True)

        guild = bot.get_guild(finalid)
        guild_member = await resolve_guild_member(guild, interaction.user)
        moderator_role_id = getserverparameterfromserverid(str(finalid), "ModeratorRole", event=event_name)
        helper_role_id = getserverparameterfromserverid(str(finalid), "HelperRole", event=event_name)
        is_moderator = member_has_role_id(guild_member, moderator_role_id)
        is_helper = member_has_role_id(guild_member, helper_role_id)

        modd = 0
        if is_moderator:
            if self.iscommand != 0 and not _is_moderator_queue_mode_value(self.iscommand):
                return await interaction.followup.send(
                    "You can only use moderator's Reject button or you have both Helper/Moderator roles......",
                    ephemeral=True,
                )
            modd = 1
            if not check_configs(str(finalid), 3, event=event_name):
                if guild_member is not None and guild_member.guild_permissions.administrator:
                    return await interaction.followup.send(embed=report_configs(str(finalid), 3, event=event_name), ephemeral=True)
                return await interaction.followup.send("Config error...", ephemeral=True)
        else:
            if not check_configs(str(finalid), 0, event=event_name):
                if guild_member is not None and guild_member.guild_permissions.administrator:
                    return await interaction.followup.send(embed=report_configs(str(finalid), 0, event=event_name), ephemeral=True)
                return await interaction.followup.send("Config error...", ephemeral=True)

        if is_helper and self.iscommand != 0 and not _is_helper_queue_mode_value(self.iscommand):
            return await interaction.followup.send(
                "You can only use helper's Reject button or you have both Helper/Moderator roles...",
                ephemeral=True,
            )

        y = int(self.RequestID)
        channel1 = bot.get_channel(int(getserverparameterfromserverid(str(finalid), "ChannelHelpers", event=event_name)))
        a = getsheetlinefromreqid(y)
        a = [str(x) for x in a]
        linerej = getsheetparameterfromreqid(y, "HelpersNotSend")
        linerejmod = getsheetparameterfromreqid(y, "ModeratorsNotSend")
        note_value = str(getattr(self.level_review, "value", "") or "").strip()
        not_send_reason_raw = self._get_not_send_reason_value(interaction)
        not_send_reason_value = _normalize_not_send_reason_value(not_send_reason_raw)
        not_send_title = _not_send_reason_display(not_send_reason_value) if not_send_reason_value else "Not Sent"
        not_send_emoji = _not_send_reason_emoji(not_send_reason_value) if not_send_reason_value else "<:not_sent:1155722772367028244>"
        allow_requester_ping = _mention_requester_enabled(self._get_ping_value(interaction))
        uploaded_photos = await _extract_uploaded_image_files(self.result_photo_upload, max_count=1)



        # Moderator typed not-send reasons (Already Rated / Already Seen / Wrong ID / Report)
        # are status outcomes, not normal rejection screenshots. Keep the old reject badge only
        # for plain moderator Not Sent and for all helper reject paths.
        badge_file = None if (modd == 1 and not_send_reason_value) else await _build_unrate_badge(f"request_{y}_reject", modd = str(modd))

        if modd == 1:
            # Reason-based moderator outcomes (Already Rated / Already Seen / Wrong ID / Report)
            # are separate typed statuses. They must not consume/increment the plain Not Sent counter.
            notsendfin = ""
            channelmod = bot.get_channel(int(getserverparameterfromserverid(str(finalid), "ModNotSendChannel", event=event_name)))
            if not await ensure_send_permissions(interaction, channelmod, "Bot cannot send messages in mod not-send channel", require_file=badge_file is not None):
                return
            if not not_send_reason_value:
                notsend = int(getserverparameterfromserverid(str(finalid), "ModNotSendsCount", event=event_name))
                if notsend != 0:
                    notsendfin = f' #{notsend}'
                    editserverparameterfromserverid(str(finalid), "ModNotSendsCount", notsend + 1, event=event_name)

            headline = f'Unfortunately, your level has not been sent...'
            await _send_staff_components_message(
                channelmod,
                interaction=interaction,
                counter=notsendfin,
                # Plain moderator Not Sent historically used the moderator/unrate emoji,
                # not the helper Not Sent emoji. Typed reasons keep their own reason emoji.
                title_emoji=not_send_emoji if not_send_reason_value else "<:unrate_3:1290328913314316413>",
                title_text=not_send_title,
                headline_text=headline,
                level_name=a[2],
                level_creator=a[4],
                level_id=a[3],
                checker_label="Moderator",
                checker_mention=interaction.user.mention,
                requester_mention=a[1],
                note_text=note_value,
                accent_colour=None,
                badge_file=badge_file,
                extra_media_files=uploaded_photos,
                allow_requester_ping=allow_requester_ping,
            )
            _maybe_clear_existing_records_on_success(self, y, interaction.user)
            _append_sheet_not_send_reason_actor(y, "moderator", interaction.user.mention, not_send_reason_value)

            # SentTo is helper -> moderator routing info, not a moderator reject result.
            # Do not append moderator rejects here, otherwise sent-to filters/reactions get polluted.
            try:
                msg = await channel1.fetch_message(int(a[12]))
                await _safe_message_edit(msg, embed=request_embed(y, "0"))
            except Exception:
                print("Request Deleted :(")
            if check_configs(str(finalid), 1, event=event_name):
                await AddReaction(y, str(finalid))
            if self.msgid != 0:
                try:
                    channel3 = bot.get_channel(self.chanid)
                    msg3 = await channel3.fetch_message(self.msgid)
                except Exception:
                    channel3 = await interaction.user.create_dm()
                    msg3 = await channel3.fetch_message(self.msgid)

                if _is_moderator_queue_mode(self.iscommand):
                    random_mode = _is_moderator_queue_random_mode(self.iscommand)
                    helper_approved_only = _is_moderator_queue_helper_approved_only(self.iscommand)
                    button_filters = _get_button_queue_filters(self.msgid)
                    next_request_id, remaining_requests_count = _find_next_moderator_request_with_count(
                        finalid,
                        interaction.user,
                        random_mode=random_mode,
                        helper_approved_only=helper_approved_only,
                        min_request_id=_get_button_min_request_id(self.msgid),
                        event=event_name,
                        platformer_filter=button_filters["platformer_filter"],
                        difficulty_filter="",
                        min_senddb=button_filters["min_senddb"],
                        max_senddb=button_filters["max_senddb"],
                    )

                    if next_request_id is None:
                        await _safe_message_edit(msg3, content="All requests have been checked!", suppress=True, view=None)
                        deletebuttonlinefrommessageid(str(msg3.id))
                    else:
                        await _safe_message_edit(msg3, 
                            embed=request_embed(next_request_id, "0", remaining_count=remaining_requests_count),
                            view=ReactionView(RequestID=next_request_id, iscommand=self.iscommand, msgid=msg3.id, chanid=interaction.channel_id),
                        )
                        editbuttonparameterfrommessageid(str(msg3.id), "RequestID", next_request_id)
                else:
                    await _safe_message_edit(msg3, embed=request_embed(y, "0"))
        else:
            channel = bot.get_channel(int(getserverparameterfromserverid(str(finalid), "NotSentToChannel", event=event_name)))
            if not await ensure_send_permissions(interaction, channel, "Bot cannot send messages in not-sent-to channel", require_file=badge_file is not None):
                return

            headline = f'Unfortunately, your level has not been sent...'
            result_message = await _send_staff_components_message(
                channel,
                interaction=interaction,
                counter="",
                title_emoji=not_send_emoji,
                title_text=not_send_title,
                headline_text=headline,
                level_name=a[2],
                level_creator=a[4],
                level_id=a[3],
                checker_label="Helper",
                checker_mention=interaction.user.mention,
                requester_mention=a[1],
                note_text=note_value,
                # Typed not-send reasons (Already Rated / Already Seen / Wrong ID / Report)
                # should use the reason title/emoji only, without the red Components v2 accent.
                # Plain old Not Sent without a selected reason keeps the old red helper color.
                accent_colour=None if not_send_reason_value else discord.Color.from_rgb(253, 29, 31),
                badge_file=badge_file,
                extra_media_files=uploaded_photos,
                allow_requester_ping=allow_requester_ping,
            )
            if result_message is None:
                return
            await _update_stats_message_safe(finalid, event_name)
            _maybe_clear_existing_records_on_success(self, y, interaction.user)
            _append_sheet_not_send_reason_actor(y, "helper", interaction.user.mention, not_send_reason_value)
            try:
                msg = await channel1.fetch_message(int(a[12]))
                await _safe_message_edit(msg, embed=request_embed(y, "0"))
            except Exception:
                print("Request Deleted :(")
            if check_configs(str(finalid), 1, event=event_name):
                await AddReaction(y, str(finalid))
            if self.msgid != 0:
                try:
                    channel3 = bot.get_channel(self.chanid)
                    msg3 = await channel3.fetch_message(self.msgid)
                except Exception:
                    channel3 = await interaction.user.create_dm()
                    msg3 = await channel3.fetch_message(self.msgid)
                if self.iscommand == 0:
                    await _safe_message_edit(msg3, embed=request_embed(y, "0"))
                else:
                    kol = 0
                    idd = interaction.user.id
                    random_mode = (self.iscommand != 1)
                    while kol == 0:
                        button_filters = _get_button_queue_filters(self.msgid)
                        request_id, remaining_requests_count = _find_next_helper_request_with_count(
                            finalid,
                            idd,
                            random_mode=random_mode,
                            min_request_id=_get_button_min_request_id(self.msgid),
                            event=event_name,
                            platformer_filter=button_filters["platformer_filter"],
                            difficulty_filter="",
                            min_senddb=button_filters["min_senddb"],
                            max_senddb=button_filters["max_senddb"],
                            max_sent_to=button_filters.get("max_sent_to"),
                        )
                        if request_id is None:
                            kol = 1
                            await _safe_message_edit(msg3, content="All requests have been checked!", suppress=True, view=None)
                            deletebuttonlinefrommessageid(str(msg3.id))
                        else:
                            kol = 1
                            await _safe_message_edit(msg3, 
                                embed=request_embed(request_id, "0", remaining_count=remaining_requests_count),
                                view=ReactionView(RequestID=request_id, iscommand=self.iscommand, msgid=msg3.id, chanid=interaction.channel_id),
                            )
                            editbuttonparameterfrommessageid(str(msg3.id), "RequestID", request_id)





class RequestModalReview(discord.ui.Modal, title='Review form'):
    def __init__(self, RequestID, iscommand, msgid, chanid, event="0"):
        super().__init__(timeout=7200)
        self.RequestID = RequestID
        self.iscommand = iscommand
        self.msgid = msgid
        self.chanid = chanid
        self.event = normalize_event_name(event)

        self.level_review = discord.ui.TextInput(
            label="Review",
            required=True,
            max_length=4000,
            style=discord.TextStyle.paragraph,
            placeholder="Write your review.",
        )
        self.add_item(self.level_review)

        self.photo_positions = discord.ui.TextInput(
            label="Photo positions by paragraph",
            required=False,
            max_length=100,
            style=discord.TextStyle.short,
            placeholder="Example: 1,2,end = after paragraphs 1, 2, then end",
        )
        self.add_item(self.photo_positions)

        self.review_photos_upload = None
        self.review_photos = None
        if _supports_modal_file_upload():
            self.review_photos_upload = discord.ui.FileUpload(
                custom_id="requestmodalreview_photos",
                required=False,
                min_values=0,
                max_values=10,
            )
            self.review_photos = discord.ui.Label(
                text="Review images (optional)",
                description="Use paragraph numbers above. Empty/invalid positions go to the end.",
                component=self.review_photos_upload,
            )
            self.add_item(self.review_photos)

    async def on_submit(self, interaction: discord.Interaction):
        await interaction.response.defer()
        finalid = interaction.guild_id
        event_name = self.event
        if isinstance(interaction.channel, discord.DMChannel):
            if bot.get_guild(int(getdmusersparameterfromuserid(str(interaction.user.id), "ServerID"))) is not None:
                finalid = int(getdmusersparameterfromuserid(str(interaction.user.id), "ServerID"))
            else:
                return await interaction.followup.send("The server is not linked or linked wrong", ephemeral=True)
        if not check_configs(str(finalid), 2, event=event_name):
            userfin = await resolve_guild_member(bot.get_guild(finalid), interaction.user)
            if userfin is not None and userfin.guild_permissions.administrator:
                return await interaction.followup.send(embed=report_configs(str(finalid), 2, event=event_name), ephemeral=True)
            else:
                return await interaction.followup.send("Config error...", ephemeral=True)
        self.RequestID = self.RequestID
        self.iscommand = self.iscommand
        self.msgid = self.msgid
        self.chanid = self.chanid
        y = int(self.RequestID)
        nn = count_specific_values("Sheet", "ServerID", str(finalid))
        print(y)
        channel1 = bot.get_channel(int(getserverparameterfromserverid(str(finalid), "ChannelHelpers", event=event_name)))
        a = getsheetlinefromreqid(y)
        a = [str(x) for x in a]


        channel = bot.get_channel(int(getserverparameterfromserverid(str(finalid), "ReviewsChannel", event=event_name)))
        if not await ensure_thread_permissions(
                interaction,
                channel,
                "Bot needs permissions to create threads in reviews channel"
        ):
            return

        info_embed = discord.Embed(title=f":pencil: Review",
                                   description=f'{interaction.user.mention} wrote a review of your level!\nCheck it out in the thread below this message!')
        info_embed.set_footer(text=f"Level Name: {a[2]}, Level ID: {a[3]}")

        if not await ensure_send_permissions(
            interaction,
            channel,
            "Bot cannot send the review notification message.",
            require_embed=True,
        ):
            return

        try:
            message = await channel.send(a[1], embed=info_embed)
        except (discord.Forbidden, discord.HTTPException) as send_error:
            status = getattr(send_error, "status", None)
            if isinstance(send_error, discord.Forbidden) or status in (403, 404):
                await warn_send_permission_problem(
                    interaction,
                    channel,
                    "Discord rejected the review notification message.",
                    require_embed=True,
                    exception=send_error,
                )
                return
            raise

        try:
            thread = await message.create_thread(name=f"{a[2]} by {a[4]}")

            review_text = str(getattr(self.level_review, "value", "") or "").strip()
            paragraphs, separators = _split_nonempty_paragraphs(review_text)
            uploaded_photos = await _extract_uploaded_image_files(self.review_photos_upload, max_count=10)
            # Discord/FileUpload can return multi-upload values in reverse visual order.
            # Reverse them here so positions like 1,2,end match the order the reviewer picked.
            uploaded_photos = list(reversed(uploaded_photos))
            photo_positions = _parse_review_photo_positions(
                str(getattr(self.photo_positions, "value", "") or ""),
                len(uploaded_photos),
                len(paragraphs),
            )

            if not await ensure_send_permissions(
                interaction,
                thread,
                "Bot cannot send the review text in the review thread.",
                require_file=bool(uploaded_photos),
            ):
                return

            photos_after_paragraph: dict[int, list[tuple[int, discord.File]]] = {}
            photos_for_end: list[tuple[int, discord.File]] = []

            for index, paragraph_number in enumerate(photo_positions):
                if index >= len(uploaded_photos):
                    break
                photo_file = uploaded_photos[index]
                photo_entry = (index + 1, photo_file)
                if paragraph_number is None:
                    photos_for_end.append(photo_entry)
                else:
                    photos_after_paragraph.setdefault(paragraph_number, []).append(photo_entry)

            await _send_compact_review_messages(
                thread,
                paragraphs,
                separators,
                photos_after_paragraph,
                photos_for_end,
            )
        except Exception as e:
            print(f"Error creating thread: {e}")
            await interaction.followup.send("Failed to create review thread", ephemeral=True)
            return
        linerev = getsheetparameterfromreqid(y, "Reviewers")
        if linerev == "":
            linerevfin = interaction.user.mention
        else:
            linerevfin = linerev + ", " + interaction.user.mention
        editsheetparameterfromreqid(y, "Reviewers", linerevfin)
        try:
            msg = await channel1.fetch_message(int(a[12]))
            await _safe_message_edit(msg, embed=request_embed(y, "0"))
        except Exception:
            print("Request Deleted :(")
        if check_configs(str(finalid), 1, event=event_name):
            await AddReaction(y, str(finalid))
        if self.msgid != 0:
            try:
                channel3 = bot.get_channel(self.chanid)
                msg3 = await channel3.fetch_message(self.msgid)
            except Exception:
                channel3 = await interaction.user.create_dm()
                msg3 = await channel3.fetch_message(self.msgid)

            if self.iscommand == 0:
                await _safe_message_edit(msg3, embed=request_embed(y, "0"))
            else:
                try:
                    kol = 0
                    aaa = []
                    x = self.iscommand
                    random_i = 1

                    # Optimized reviewer next-request selection (single DB row read per request)
                    if x in (2, 8):
                        review_lang = "us"
                    elif x in (6, 12):
                        review_lang = "esp"
                    elif x in (10, 16):
                        review_lang = "fr"
                    else:
                        review_lang = "ru"

                    random_mode = x in (2, 3, 6, 10)
                    next_request_id, remaining_requests_count = _find_next_reviewer_request_with_count(
                        finalid,
                        review_lang,
                        random_mode=random_mode,
                        min_request_id=_get_button_min_request_id(self.msgid),
                        event=event_name,
                    )

                    if next_request_id is None:
                        await _safe_message_edit(msg3, content="All requests have been checked!", suppress=True, view=None)
                        deletebuttonlinefrommessageid(str(msg3.id))
                    else:
                        await _safe_message_edit(msg3, 
                            embed=request_embed(next_request_id, "0", remaining_count=remaining_requests_count),
                            view=ReactionView(RequestID=next_request_id, iscommand=x, msgid=msg3.id, chanid=interaction.channel_id)
                        )
                        editbuttonparameterfrommessageid(str(msg3.id), "RequestID", next_request_id)
                except Exception as e:
                    print(e)



class RequestModalModAccept(discord.ui.Modal, title='GD Mod sent form'):
    def __init__(self, RequestID, iscommand, msgid, chanid, event="0"):
        super().__init__(timeout=600)
        self.RequestID = RequestID
        self.iscommand = iscommand
        self.msgid = msgid
        self.chanid = chanid
        self.event = normalize_event_name(event)

        self.mod_name = discord.ui.TextInput(
            label="Moderator name",
            required=True,
            max_length=50,
            style=discord.TextStyle.short,
        )

        self._send_type_select_custom_id = "requestmodalmodaccept_send_type_select"
        self._difficulty_select_custom_id = "requestmodalmodaccept_difficulty_select"
        self._uses_modal_selects = hasattr(discord.ui, "Label")

        if self._uses_modal_selects:
            platformer = getsheetparameterfromreqid(self.RequestID, "IsPlatformer")
            if platformer == "Yes":
                send_type_specs = _STAFF_SEND_TYPE_OPTIONS_MOON
            else:
                send_type_specs = _STAFF_SEND_TYPE_OPTIONS
            self.level_opinion_select = discord.ui.Select(
                custom_id=self._send_type_select_custom_id,
                placeholder="Select type of the send (optional)",
                min_values=0,
                max_values=1,
                required=False,
                options=send_type_specs,
            )
            self.level_opinion = discord.ui.Label(
                text="Type of the send",
                component=self.level_opinion_select,
            )

            difficulty_specs = _GDMOD_DIFFICULTY_OPTION_SPECS_MOON if platformer == "Yes" else _GDMOD_DIFFICULTY_OPTION_SPECS
            self.level_difficulty_select = discord.ui.Select(
                custom_id=self._difficulty_select_custom_id,
                placeholder="Select difficulty (optional)",
                min_values=0,
                max_values=1,
                required=False,
                options=_build_select_options(difficulty_specs),
            )
            self.level_difficulty = discord.ui.Label(
                text="Difficulty",
                component=self.level_difficulty_select,
            )

        else:
            self.level_opinion = discord.ui.TextInput(
                label="Type of the send",
                placeholder="None if unknown",
                required=False,
                max_length=20,
                style=discord.TextStyle.short,
            )
            self.level_difficulty = discord.ui.TextInput(
                label="Difficulty",
                placeholder="e.g. Demon / Hard, 4-5 Stars/Moons",
                required=False,
                max_length=64,
                style=discord.TextStyle.short,
            )

        self.level_review = discord.ui.TextInput(
            label="Note from GD Mod",
            required=False,
            max_length=1500,
            style=discord.TextStyle.paragraph,
        )

        self.result_photo_upload = None
        self.result_photo = None
        if _supports_modal_file_upload():
            self.result_photo_upload = discord.ui.FileUpload(
                custom_id="requestmodalmodaccept_result_photo",
                required=False,
                min_values=0,
                max_values=1,
            )
            self.result_photo = discord.ui.Label(
                text="Message image (optional)",
                description="Attach one image under the main text.",
                component=self.result_photo_upload,
            )

        self.add_item(self.mod_name)
        self.add_item(self.level_opinion)
        self.add_item(self.level_difficulty)
        self.add_item(self.level_review)
        if self.result_photo is not None:
            self.add_item(self.result_photo)

    def _extract_modal_select_value(self, interaction: discord.Interaction, custom_id: str, select_obj=None) -> str:
        try:
            if select_obj is not None:
                selected_values = getattr(select_obj, "values", None)
                if selected_values:
                    return str(selected_values[0]).strip()
        except Exception:
            pass

        def _walk_components(components):
            if not isinstance(components, list):
                return None
            for component in components:
                if not isinstance(component, dict):
                    continue
                if component.get("custom_id") == custom_id:
                    values = component.get("values") or []
                    if values:
                        return str(values[0]).strip()
                    return ""
                nested_components = component.get("components")
                found = _walk_components(nested_components)
                if found is not None:
                    return found
            return None

        interaction_data = getattr(interaction, "data", None)
        if isinstance(interaction_data, dict):
            found = _walk_components(interaction_data.get("components"))
            if found is not None:
                return found

        return ""

    def _get_send_type_value(self, interaction: discord.Interaction) -> str:
        if not getattr(self, "_uses_modal_selects", False):
            return str(getattr(self.level_opinion, "value", "") or "").strip()
        return self._extract_modal_select_value(interaction, self._send_type_select_custom_id, getattr(self, "level_opinion_select", None))

    def _get_difficulty_value(self, interaction: discord.Interaction) -> str:
        if not getattr(self, "_uses_modal_selects", False):
            return str(getattr(self.level_difficulty, "value", "") or "").strip()
        return self._extract_modal_select_value(interaction, self._difficulty_select_custom_id, getattr(self, "level_difficulty_select", None))

    async def on_submit(self, interaction: discord.Interaction):
        await interaction.response.defer()

        finalid = interaction.guild_id
        event_name = self.event
        if isinstance(interaction.channel, discord.DMChannel):
            linked_server_id = getdmusersparameterfromuserid(str(interaction.user.id), "ServerID")
            if linked_server_id and bot.get_guild(int(linked_server_id)) is not None:
                finalid = int(linked_server_id)
            else:
                return await interaction.followup.send("The server is not linked or linked wrong", ephemeral=True)

        if not check_configs(str(finalid), 3, event=event_name):
            userfin = await resolve_guild_member(bot.get_guild(finalid), interaction.user)
            if userfin is not None and userfin.guild_permissions.administrator:
                return await interaction.followup.send(embed=report_configs(str(finalid), 3, event=event_name), ephemeral=True)
            return await interaction.followup.send("Config error...", ephemeral=True)

        mod_name_value = str(getattr(self.mod_name, "value", "") or "").strip()
        level_opinion_raw = self._get_send_type_value(interaction)
        difficulty_raw = self._get_difficulty_value(interaction)
        level_review_value = str(getattr(self.level_review, "value", "") or "").strip()
        uploaded_photos = await _extract_uploaded_image_files(self.result_photo_upload, max_count=1)

        y = int(self.RequestID)
        send = int(getserverparameterfromserverid(str(finalid), "ModSendsCount", event=event_name))
        a = getsheetlinefromreqid(y)
        a = [str(x) for x in a]
        send_type_for_storage = _send_type_storage_value(level_opinion_raw, default="")

        _append_sheet_send_type_actor(y, "moderator", mod_name_value, send_type_for_storage)

        channel = bot.get_channel(int(getserverparameterfromserverid(str(finalid), "ModSendChannel", event=event_name)))
        if not await ensure_send_permissions(interaction, channel, "Bot cannot send messages in mod send channel, the information is saved in the database"):
            return

        sendfin = ""
        if getserverparameterfromserverid(str(finalid), "ModSendsCount", event=event_name) != 0:
            sendfin = f' #{send}'
            editserverparameterfromserverid(str(finalid), "ModSendsCount", send + 1, event=event_name)

        display_send_type_raw = _send_type_display_raw_for_request(level_opinion_raw, a)
        badge_file = await _build_send_badge_for_request(a, send_type_raw=display_send_type_raw, difficulty_raw=difficulty_raw, filename_prefix=f"request_{y}_gdmod_accept")
        if badge_file is None:
            badge_file = await _build_rate_badge(f"request_{y}_gdmod_accept", modd="1")
        title_emoji, title_text = _compose_send_title_parts_for_request(level_opinion_raw, a, default_emoji="<:rate_3:1290321896361164842>")
        headline = f'**{mod_name_value}** did send your level to RobTop!'
        await _send_staff_components_message(
            channel,
            interaction=interaction,
            counter=sendfin,
            title_emoji=title_emoji,
            title_text=title_text,
            headline_text=headline,
            level_name=a[2],
            level_creator=a[4],
            level_id=a[3],
            checker_label="Helper",
            checker_mention=interaction.user.mention,
            requester_mention=a[1],
            note_text=level_review_value,
            accent_colour=None,
            badge_file=badge_file,
            extra_media_files=uploaded_photos,
        )

        channel1 = bot.get_channel(int(getserverparameterfromserverid(str(finalid), "ChannelHelpers", event=event_name)))
        try:
            msg = await channel1.fetch_message(int(a[12]))
            await _safe_message_edit(msg, embed=request_embed(y, "0"))
        except Exception:
            print("Request Deleted :(")

        if self.msgid != 0:
            try:
                channel3 = bot.get_channel(self.chanid)
                msg3 = await channel3.fetch_message(self.msgid)
            except Exception:
                channel3 = await interaction.user.create_dm()
                msg3 = await channel3.fetch_message(self.msgid)

            mode_value = _normalize_mode_value(self.iscommand)
            if mode_value in (4, 5):
                button_filters = _get_button_queue_filters(self.msgid)
                next_request_id, remaining_requests_count = _find_next_helper_sent_request_with_count(
                    finalid,
                    interaction.user.mention,
                    random_mode=(mode_value == 5),
                    min_request_id=_get_button_min_request_id(self.msgid),
                    event=event_name,
                    send_type_filter=button_filters["send_type_filter"],
                    platformer_filter=button_filters["platformer_filter"],
                    difficulty_filter="",
                    min_senddb=button_filters["min_senddb"],
                    max_senddb=button_filters["max_senddb"],
                    max_sent_to=button_filters.get("max_sent_to"),
                )
                if next_request_id is None:
                    await _safe_message_edit(msg3, content="All requests have been checked!", suppress=True, view=None)
                    deletebuttonlinefrommessageid(str(msg3.id))
                else:
                    await _safe_message_edit(msg3, 
                        embed=request_embed(next_request_id, "0", remaining_count=remaining_requests_count),
                        view=ReactionView(
                            RequestID=next_request_id,
                            iscommand=self.iscommand,
                            msgid=msg3.id,
                            chanid=interaction.channel_id,
                        ),
                    )
                    editbuttonparameterfrommessageid(str(msg3.id), "RequestID", next_request_id)
            else:
                await _safe_message_edit(msg3, embed=request_embed(y, "0"))



class RequestModalModReject(discord.ui.Modal, title='GD Mod not sent form'):
    def __init__(self, RequestID, iscommand, msgid, chanid, event="0"):
        super().__init__(timeout=600)
        self.RequestID = RequestID
        self.iscommand = iscommand
        self.msgid = msgid
        self.chanid = chanid
        self.event = normalize_event_name(event)

        self.mod_name = discord.ui.TextInput(
            label="Moderator name",
            required=True,
            max_length=50,
            style=discord.TextStyle.short,
        )
        self.level_review = discord.ui.TextInput(
            label="Note from GD Mod",
            required=False,
            max_length=1500,
            style=discord.TextStyle.paragraph,
        )
        self.add_item(self.mod_name)
        self.add_item(self.level_review)

        self.result_photo_upload = None
        self.result_photo = None
        if _supports_modal_file_upload():
            self.result_photo_upload = discord.ui.FileUpload(
                custom_id="requestmodalmodreject_result_photo",
                required=False,
                min_values=0,
                max_values=1,
            )
            self.result_photo = discord.ui.Label(
                text="Message image (optional)",
                description="Attach one image under the main text.",
                component=self.result_photo_upload,
            )
            self.add_item(self.result_photo)

    async def on_submit(self, interaction: discord.Interaction):
        await interaction.response.defer()
        finalid = interaction.guild_id
        event_name = self.event
        if isinstance(interaction.channel, discord.DMChannel):
            linked_server_id = getdmusersparameterfromuserid(str(interaction.user.id), "ServerID")
            if linked_server_id and bot.get_guild(int(linked_server_id)) is not None:
                finalid = int(linked_server_id)
            else:
                return await interaction.followup.send("The server is not linked or linked wrong", ephemeral=True)
        if not check_configs(str(finalid), 3, event=event_name):
            userfin = await resolve_guild_member(bot.get_guild(finalid), interaction.user)
            if userfin is not None and userfin.guild_permissions.administrator:
                return await interaction.followup.send(embed=report_configs(str(finalid), 3, event=event_name), ephemeral=True)
            return await interaction.followup.send("Config error...", ephemeral=True)

        y = int(self.RequestID)
        notsend = int(getserverparameterfromserverid(str(finalid), "ModNotSendsCount", event=event_name))
        a = getsheetlinefromreqid(y)
        a = [str(x) for x in a]
        mod_name_value = str(getattr(self.mod_name, "value", "") or "").strip()
        note_value = str(getattr(self.level_review, "value", "") or "").strip()
        uploaded_photos = await _extract_uploaded_image_files(self.result_photo_upload, max_count=1)
        linerejmod = getsheetparameterfromreqid(y, "ModeratorsNotSend")
        linerejmodfin = mod_name_value if linerejmod == "" else linerejmod + ", " + mod_name_value
        editsheetparameterfromreqid(y, "ModeratorsNotSend", linerejmodfin)
        channel = bot.get_channel(int(getserverparameterfromserverid(str(finalid), "ModNotSendChannel", event=event_name)))
        if not await ensure_send_permissions(interaction, channel, "Bot cannot send messages in mod not send channel, the information is saved in the database"):
            return
        notsendfin = ""
        if getserverparameterfromserverid(str(finalid), "ModNotSendsCount", event=event_name) != 0:
            notsendfin = f' #{notsend}'
            editserverparameterfromserverid(str(finalid), "ModNotSendsCount", notsend + 1, event=event_name)

        badge_file = await _build_unrate_badge(f"request_{y}_gdmod_reject", modd = "1")
        headline = f'**{mod_name_value}** did not send your level...'
        await _send_staff_components_message(
            channel,
            interaction=interaction,
            counter=notsendfin,
            title_emoji="<:unrate_3:1290328913314316413>",
            title_text="Not Sent",
            headline_text=headline,
            level_name=a[2],
            level_creator=a[4],
            level_id=a[3],
            checker_label="Helper",
            checker_mention=interaction.user.mention,
            requester_mention=a[1],
            note_text=note_value,
            accent_colour=None,
            badge_file=badge_file,
            extra_media_files=uploaded_photos,
        )
        channel1 = bot.get_channel(int(getserverparameterfromserverid(str(finalid), "ChannelHelpers", event=event_name)))
        try:
            msg = await channel1.fetch_message(int(a[12]))
            await _safe_message_edit(msg, embed=request_embed(y, "0"))
        except Exception:
            print("Request Deleted :(")
        if self.msgid != 0:
            try:
                channel3 = bot.get_channel(self.chanid)
                msg3 = await channel3.fetch_message(self.msgid)
            except Exception:
                channel3 = await interaction.user.create_dm()
                msg3 = await channel3.fetch_message(self.msgid)
            await _safe_message_edit(msg3, embed=request_embed(y, "0"))



class RequestModalSentToMod(discord.ui.Modal, title='Sent to GD Mod form'):
    def __init__(self, RequestID, iscommand, msgid, chanid, event="0"):
        super().__init__(timeout=600)
        self.RequestID = RequestID
        self.iscommand = iscommand
        self.msgid = msgid
        self.chanid = chanid
        self.event = normalize_event_name(event)

    mod_name = discord.ui.TextInput(label="Moderator name", required=True, max_length=500, style=discord.TextStyle.short)

    async def on_submit(self, interaction: discord.Interaction):
        await interaction.response.defer()
        finalid = interaction.guild_id
        event_name = self.event
        if isinstance(interaction.channel, discord.DMChannel):
            if bot.get_guild(int(getdmusersparameterfromuserid(str(interaction.user.id), "ServerID"))) is not None:
                finalid = int(getdmusersparameterfromuserid(str(interaction.user.id), "ServerID"))
            else:
                return await interaction.followup.send("The server is not linked or linked wrong", ephemeral=True)
        if not check_configs(str(finalid), 1, event=event_name):
            userfin = await resolve_guild_member(bot.get_guild(finalid), interaction.user)
            if userfin is not None and userfin.guild_permissions.administrator:
                return await interaction.followup.send(embed=report_configs(str(finalid), 1, event=event_name), ephemeral=True)
            else:
                return await interaction.followup.send("Config error...", ephemeral=True)
        self.RequestID = self.RequestID
        self.iscommand = self.iscommand
        self.msgid = self.msgid
        self.chanid = self.chanid
        y = int(self.RequestID)
        print(y)
        channel1 = bot.get_channel(int(getserverparameterfromserverid(str(finalid), "ChannelHelpers", event=event_name)))
        a = getsheetlinefromreqid(y)
        a = [str(x) for x in a]
        mod_name_value = str(getattr(self.mod_name, "value", "") or "").strip()
        # SentTo is only a routing note: helper -> GD moderator. It is not a send result.
        linesent = getsheetparameterfromreqid(y, "SentTo")
        if linesent == "":
            linesentfin = mod_name_value
        else:
            linesentfin = linesent + ", " + mod_name_value
        editsheetparameterfromreqid(y, "SentTo", str(linesentfin))
        try:
            msg = await channel1.fetch_message(int(a[12]))
            await _safe_message_edit(msg, embed=request_embed(y, "0"))
        except Exception:
            print("Request Deleted :(")
        channel = bot.get_channel(int(getserverparameterfromserverid(str(finalid), "ChannelEveryone", event=event_name)))
        if not await ensure_send_permissions(interaction, channel, "Bot cannot send messages in everyone channel, the information is saved in the database"):
            return
        try:
            message = await channel.fetch_message(int(getsheetparameterfromreqid(y, "MessageIDEveryone")))
            has_perms, missing = await can_create_threads(channel)

            if has_perms:
                # Есть все права - создаём новый тред
                try:
                    thread = await message.create_thread(name=f"Sent To")
                    thread_text = f'{interaction.user.mention} sent "**{a[2]}**" to {mod_name_value}! ||{a[1]}||'
                    if await ensure_send_permissions(
                        interaction,
                        thread,
                        "Bot cannot send the sent-to notification inside the thread.",
                    ):
                        await thread.send(thread_text)
                    print(f"Created new thread for {a[2]}")
                except Exception as e:
                    thread = channel.get_thread(int(getsheetparameterfromreqid(y, "MessageIDEveryone")))
                    if thread is None:
                        print("None")
                        thread = await interaction.guild.fetch_channel(
                            int(getsheetparameterfromreqid(y, "MessageIDEveryone")))
                    if thread is not None:
                        thread_text = f'{interaction.user.mention} sent "**{a[2]}**" to {mod_name_value}! ||{a[1]}||'
                        if await ensure_send_permissions(
                            interaction,
                            thread,
                            "Bot cannot send the sent-to notification inside the existing thread.",
                        ):
                            await thread.send(thread_text)
            else:
                # Нет прав на создание веток - пробуем найти существующий тред
                print(f"Missing thread permissions: {missing}")
        except Exception as e:
            print(f"Error in sent to mod: {e}")
        if self.msgid != 0:
            try:
                channel3 = bot.get_channel(self.chanid)
                msg3 = await channel3.fetch_message(self.msgid)
            except Exception:
                channel3 = await interaction.user.create_dm()
                msg3 = await channel3.fetch_message(self.msgid)
            await _safe_message_edit(msg3, embed=request_embed(y, "0"))


__all__ = [
    'RequestModalAccept',
    'RequestModalReject',
    'RequestModalReview',
    'RequestModalModAccept',
    'RequestModalModReject',
    'RequestModalSentToMod',
    'CandidateVoteSentModal',
    'CandidateVoteNotSentModal',
]
