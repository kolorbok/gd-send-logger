# v2.0.27

- Reject/cancel icon scale changed from 1.18 to 1.25.
- Requests star is now shipped as a raw resource file and loaded directly from `Mod::get()->getResourcesDir()` instead of the sprite-frame cache. This avoids Geode's magenta/black missing-texture fallback when the custom sprite frame is not registered.
- No request, feedback, filter, or queue logic changes.
