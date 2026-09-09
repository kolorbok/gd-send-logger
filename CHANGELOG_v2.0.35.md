# v2.0.35

- Reverted the Server Requests info button to the v2.0.33 `GJ_plus2Btn_001.png` texture.
- When a request has a video, the button order is now `+ -> YouTube -> VIEW/GET IT`.
- When there is no video, `+` remains directly beside `VIEW/GET IT`.
- Kept the tight 2 px visual spacing introduced in v2.0.34.
- Added one automatic retry to the GitHub Actions Geode build step so transient SDK release/binary-download failures (such as the Windows `Could not parse Geode release` failure) do not immediately kill that platform build.
