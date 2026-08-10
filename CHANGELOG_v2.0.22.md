# v2.0.22

- Reverted difficulty artwork to Geometry Dash's known `*_btn_001.png` frames only. The attempted face-only demon frame names could resolve to the magenta missing-texture atlas on some installs / texture packs.
- Demon fallback artwork is scaled slightly smaller and moved upward; only the baked caption strip is covered, so the mask no longer clips the demon face.
- Star counts were moved closer to the difficulty caption and stars enlarged/aligned.
- Feedback editor no longer treats the hidden single-line TextInput cursor as the multiline cursor. The wrapped editor owns its cursor; the hidden TextInput is only an IME transport.
- TextInput mutations are diffed only to learn what was inserted/deleted, then applied at the wrapped editor cursor. This keeps click/arrow position and typed text synchronized even when the native single-line cursor differs.
- Restored keyboard navigation on the feedback touch layer for Left/Right/Up/Down/Home/End.
- Feedback caret made thinner and moved slightly left for better glyph alignment.
