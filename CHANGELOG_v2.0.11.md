# v2.0.11

- Fixed Request Feedback text staying invisible after typing by forcing the TextArea glyphs visible after every input update and again on the next frame.
- Fixed No Ping checkbox state being applied one click late / inverted by reading CCMenuItemToggler state after its native activation finishes.
- Hardened No Ping on Discord: when disabled, requester display is rendered as plain @name / @id text and AllowedMentions.none() is used, so the requester cannot be pinged by the result message.
