# GD Requests v2.0.7

- Removed internal Event 0 / native-list UI labels while keeping Event != 0 rows hidden.
- Raised request fetch limit from 100 to 10000 and kept backend FOUND count visible.
- Helper/reject level buttons now use full scale 1.0.
- Helper RateStars title now follows helper request context directly.
- Feedback button is positioned relative to and left of Cancel.
- Feedback editor is smaller and uses a large wrapped text area backed by the GD text input/IME.
- Rejection popup now matches the send popup proportions and uses exclusive gray/green reason buttons using the bot's canonical four reasons (Wrong ID / Already Seen / Already Rated / Report).
- Rejection title is role-aware: MOD / HELPER: REJECTION REASON.

- Compile fix: rejection popup title now uses `char const*` directly with `setTitle`.
