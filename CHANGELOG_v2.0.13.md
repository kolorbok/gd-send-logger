# GD Requests v2.0.13

- Stabilized the multiline request feedback editor: it now scrolls by complete wrapped lines instead of rebuilding a moving character tail.
- Added a visible blinking caret to the mirrored feedback editor.
- Replaced the arrow-only difficulty control with a clickable requested-difficulty picker: 1-9 stars plus Easy, Medium, Hard, Insane, and Extreme Demon.
- Moved the `NO PING` label below its checkbox and reduced its size in send/reject UI.
- Extended the request bridge protocol with an exact requested-difficulty key while remaining compatible with older six-column responses.
