# GD Requests v2.0.6

- Reworked **Not Sent** reason buttons into an exclusive GD-style toggle group: selecting one reason deselects the previous one.
- Rebalanced the rejection popup and made **Feedback / Cancel / Submit** consistent sizes and spacing.
- Helper star popup title is now **HELPER: SUGGEST STARS**.
- Increased the Helper send icon from `0.80` to `0.95` scale and the reject icon from `0.48` to `0.80`.
- Replaced the cramped one-line feedback popup with a wider editor plus a live wrapped preview and character counter (1500 max).
- Feedback input now uses Geode's `CommonFilter::Any` instead of the old restricted GD text popup path.
- Request results are opened in Geometry Dash's native level browser as real level cells instead of custom text rows.
- The in-game request list now hard-filters all requests whose `Event` is not exactly `0`.
