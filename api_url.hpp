#pragma once

// Local-development fallback.
// GitHub Actions overwrites this file from the repository variable SEND_API_URL
// before producing distributable builds.
inline constexpr char SEND_API_URL[] = "http://127.0.0.1:8765/api/v1/gd-send";
