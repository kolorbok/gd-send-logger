# v2.0.15

- Difficulty filtering is now multi-select instead of one-at-a-time.
- Rebuilt the difficulty picker as a Geometry Dash-style icon grid with exact 1★-9★ entries plus Easy / Medium / Hard / Insane / Extreme Demon.
- Uses native Geometry Dash difficulty face sprites so compatible texture packs can reskin the picker automatically.
- Selected difficulties stay highlighted with a check indicator; Any clears the selection.
- Feedback remains capped at 500 characters.
- Replaced SimpleTextArea rendering with fixed-position BMFont rows to eliminate the left/down drift shown in-game.
- Feedback scrolling now drops whole wrapped rows only; no character slicing or paragraph-driven node movement.
- The visible caret is positioned from the final rendered row.
