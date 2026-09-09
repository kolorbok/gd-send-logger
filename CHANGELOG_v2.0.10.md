# GD Requests v2.0.10

- Reworked Request Feedback input around Geode's public `TextInput::focus()` path. The hidden native input now feeds a `TextArea`, so the visible feedback wraps inside the centered box while keyboard/IME focus stays reliable.
- Added a standard GD `No Ping` checkbox to the right of Submit in both Suggest Stars and rejection windows, mirrored against the feedback edit button on the left.
- Request actions now send the `noPing` state to the Discord bridge.
- Rejection Cancel/Submit buttons now use a consistent green `GJ_button_01.png` + `goldFont.fnt` action-button template with a larger native-style footprint. Submit remains disabled until a reason is selected.
- Helper Suggest Stars title is enforced for the full lifetime of the helper popup, including late mutations by other rate-related mods.
- Filter UI prevents contradictory `Not checked`/`Rejected` + `My send` combinations.
- Request Hub asks the bridge for up to 50,000 matches and still opens Geometry Dash's native level list in safe 100-ID batches.
- If the bridge still returns only 100 rows, the hub explicitly warns that the server may be capping the response.

The corresponding bot bridge patch is required for `No Ping`, old-request filtering, Request Hub result writes, and removal of the old server-side 100-row cap.
