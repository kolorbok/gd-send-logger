# v2.0.21

- Difficulty picker now prefers the face-only `difficulty_*_001.png` frames instead of the `*_btn_*` frames, so demon faces are no longer covered by a caption mask in normal/vanilla resource packs.
- Difficulty names are rendered consistently for every entry; demon subtype labels use the same vertical band as normal difficulty labels.
- Difficulty icons are larger, and the star-count row is moved upward to reduce the empty gap beneath each difficulty name.
- Feedback cursor placement now writes the actual `CCTextFieldTTF::m_uCursorPos` insertion offset used by native editing instead of only moving the blink label.
- Feedback no longer handles Left/Right/Home/End in a second custom keyboard layer; the native TextInput is the only keyboard/IME cursor owner.
- The visible multiline caret mirrors the native `m_uCursorPos` on a short timer, fixing arrow-key divergence.
- Feedback text-change handling no longer guesses the cursor from a longest-common-prefix/suffix diff, fixing repeated-character insertions such as typing a character equal to the next character.
- Touch/click placement remains based on popup-local coordinates, so the same hit-testing path is used on Windows, Android, macOS, and iOS.
- Feedback limit remains 1500 characters.
