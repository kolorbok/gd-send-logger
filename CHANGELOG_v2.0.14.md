# v2.0.14

- Feedback hard limit changed to 500 characters.
- Removed per-character tail slicing and dynamic whole-text repositioning from the feedback editor.
- Feedback now uses fixed visual rows; only complete wrapped lines scroll out of view.
- Keeps a visible blinking caret for the mirrored feedback editor.
- Difficulty filter opens a dedicated picker with Any, 1★-9★, Easy Demon, Medium Demon, Hard Demon, Insane Demon, and Extreme Demon.
- Exact demon filtering uses the bot bridge `difficultyKey` field.
- No Ping label remains compact and below its checkbox.
