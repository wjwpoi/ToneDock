# TONE3000 adapter provenance

Source: https://github.com/tone-3000/api/tree/cc37a11f62f0d3472a3e5a1c1274664bd3a78e47/juce/Source/T3K

MIT licensed. ToneDock modifications: app-specific storage, runtime publishable key, architecture selection, bounded HTTP bodies, redirect credential handling, OS random PKCE, callback lifetime guards, a Simplified Chinese callback landing page and in-memory storage on Linux development hosts. The development catalog uses the documented search endpoint and merges A2 with A1/Custom models using request-local architecture filters. Full API production use is subject to TONE3000's integration terms; this does not claim approval for shipping a full-catalog product.

The rich catalog also parses the documented `a2_models_count` field. Detail model counts reflect the deduplicated captures actually returned. Cover/avatar fetching, popularity presentation, recent-history storage and UI are ToneDock application code; no model weights or account credentials are bundled.
