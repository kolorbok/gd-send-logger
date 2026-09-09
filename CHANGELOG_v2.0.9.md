# v2.0.9

- Rebuilt request feedback editing around the native `CCTextInputNode` + `TextArea` path so typed text is visible and wraps inside the feedback box instead of using a hidden single-line input.
- Restored the rejection choices to `NOT SENT`, `ALREADY SEEN`, `ALREADY RATED`, and `REPORT`.
- Reworked the rejection footer to match the native Suggest Stars popup: centered green/gold Cancel + Submit, feedback aligned under the first button column, and Submit disabled/dimmed until a reason is selected.
- Re-applies `HELPER: SUGGEST STARS` after late popup mutations.
- Request result ordering for newest/oldest/random is now applied on the full returned request set client-side; difficulty/rated are also applied client-side.
- Native Geometry Dash request browsing is split into 100-ID batches and the native previous/next arrows bridge between batches, instead of creating one oversized ID search query.
- Non-zero event requests remain excluded from the in-game request browser without exposing developer-only text in the UI.
