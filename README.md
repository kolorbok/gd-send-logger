# GD Send Logger

Private Geode mod for forwarding successful Geometry Dash moderator sends to the companion Discord bot.

Settings:
- Enabled
- Connection Key
- Send Test Request
- Debug Logging

`Send Test Request` sends a synthetic 6-star Featured payload through the exact same HTTP bridge code without requiring a Geometry Dash moderator send. It resets itself to off after triggering.

The API endpoint is compiled into the mod from the GitHub Actions repository variable `SEND_API_URL`.
