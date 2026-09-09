# v2.0.12

- Reworked feedback rendering: the native single-line TextInput is now only an off-screen IME buffer.
- Visible feedback uses Geode SimpleTextArea with real width-based multiline wrapping.
- Removed the CCTextInputNode/TextArea coupling that made typed text invisible on Android.
- Restored normal Discord requester mention rendering while keeping No Ping notification suppression.
