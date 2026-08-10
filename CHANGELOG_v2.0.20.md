# v2.0.20

- Difficulty picker: increased native difficulty icon/caption scale.
- Difficulty picker: moved the star-count row upward and enlarged/aligned the star icon.
- Difficulty picker: moved custom demon subtype captions up to the same visual band as normal difficulty captions.
- Feedback: replaced the menu-item hit target with a dedicated targeted-touch editor layer, so repeated mouse/touch placement works.
- Feedback: click/touch placement maps to the nearest UTF-8 character boundary on the selected visible line; drag placement is supported too.
- Feedback: reopened drafts place the native insertion cursor at the actual UTF-8 end.
- Feedback: visual caret is thinner and aligned to the rendered BMFont advance.
- Feedback: keyboard Left/Right/Up/Down/Home/End update the multiline visual cursor directly instead of inferring it from an off-screen single-line label.
- Feedback: insert/paste/backspace/delete cursor position is derived from the actual text edit span, keeping the visual caret synchronized with the hidden native input.
- Feedback: the 1500 limit/counter now uses UTF-8 character boundaries instead of raw bytes, so non-ASCII text is not cut mid-character.
