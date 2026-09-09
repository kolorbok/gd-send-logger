# v2.0.33

- Request cell controls are now positioned and scaled from the actual vanilla `view-button` instead of hard-coded cell coordinates.
- Replaced the round plus asset with the square outlined `GJ_plus2Btn_001.png`; YouTube and info controls share VIEW-based sizing and spacing.
- Replaced the request-info FLAlert with a fixed-size scrollable popup for long descriptions.
- Request descriptions use a Unicode-capable TTF label path so Cyrillic text can render instead of relying on GD bitmap fonts.
- Removed redundant video availability text from request info.
- Review / Feedback rows now follow the server `RequestReviewStat` mode; disabled fields are omitted, and review language is shown only when that section is enabled.
- `/api/v1/requests` appends the normalized `RequestReviewStat` metadata field while preserving the existing TSV prefix for older clients.
