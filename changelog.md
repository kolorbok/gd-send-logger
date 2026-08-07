# v1.0.0
- Initial GD moderator send detector.
- Detects successful RateStarsLayer uploads only.
- Sends level/stars/feature/platformer metadata to the Discord bot bridge.
- Supports per-user Discord ID and per-server connection key.

- Removed API URL from user settings; release builds embed it at compile time.
- GitHub Actions reads the production endpoint from repository variable `SEND_API_URL`.
