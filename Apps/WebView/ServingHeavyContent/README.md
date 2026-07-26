# ServingHeavyContent

A minimal embedded-webview example that serves **heavy content** — ~25 MB of
1080p/720p clips — out of the embedded resource bundle, and drives them the way
a real product does: a **hover-switched video background**. It asserts the
embedded scheme's HTTP byte-range behavior on screen, and stress-tests the
hover interaction that byte-range support alone does *not* make work.

The clips aren't committed: CMake **downloads them at configure time** (see
`CMakeLists.txt`), so the repo stays light. Big Buck Bunny and Sintel (CC-BY,
&copy; Blender Foundation) plus the Jellyfish sample from test-videos.co.uk.

Every check is also posted to the host and printed to **stdout**, so the demo
can be read from a terminal instead of squinted at:

```
[PASS] All clips reach a playable state — 4/4 clips at readyState ≥ 3 (HAVE_FUTURE_DATA)
[PASS] A clip auto-plays on launch (no user gesture) — heavy.mp4 visible and advancing …
[PASS] Fast sweep (24 switches @ 60 ms) with no stalls — 0 stall/waiting/error events
[PASS] Every clip advances when shown — 4/4 clips advanced playback …
...
ALL CHECKS PASSED — 42 hover switches, 0 stalls, 0 re-fetches
```

It doubles as a **regression test / self-evident proof** for the embedded-media
fix, and as a verification target for the Windows (WebView2) backend.

## What it demonstrates

WebKit (macOS `WKWebView`) and WebView2 (Windows) load `<video>`/`<audio>`
through a media resource loader that issues **HTTP byte-range requests**. It
needs real `206 Partial Content` responses with `Content-Range` /
`Content-Length` / `Accept-Ranges`.

Previously `registerEmbeddedScheme` served embedded resources through a plain
provider that:

1. **copied the entire resource** into a buffer on every request, and
2. answered `200` with no `Content-Length` and **no range support**.

So a `<video>` from the embedded scheme range-thrashed — hundreds of tiny reads,
each re-materialising the whole file — and typically failed outright with
*"An error occurred trying to load the resource."* The heavier the asset, the
more obvious the pathology.

The fix routes embedded resources through the **streaming provider**: each range
copies only the bytes requested, straight from the static embedded memory, and
`planStreamingResponse` emits the correct `200 / 206 / 416` + headers. This
example plays deliberately heavy clips to make that self-evident.

eacp also guarantees **gesture-free playback** on both backends
(`WKAudiovisualMediaTypeNone` on macOS, `--autoplay-policy=no-user-gesture-required`
on WebView2): autoplay and programmatic `play()` never wait for a click, like a
normal app. The launch check below asserts it.

## What it also demonstrates: the page side

Correct range support is necessary and **not sufficient**. A hover-video
background can still be broken in three ways that all look identical — a frozen
first frame, a black rectangle, or “it plays sometimes”:

1. **Swapping `src`, or letting the element remount.** Every hover re-downloads
   the clip. In one real trace: 265 requests and 6.7 MB for five small loops,
   each clip fetched twice per hover, while the element never got to play. The
   cure is one `<video>` per clip, created once and never rebuilt — hover only
   changes which layer is opaque.
2. **Calling `play()` in the same frame the layer becomes visible.** WebKit
   refuses playback on an element it considers invisible, and the opacity set
   microseconds earlier doesn't exist for it until the style has been
   composited. The rejection is a `NotAllowedError` on a promise nobody awaits,
   so it is completely silent. This is the “works sometimes” bug: success
   depends purely on frame timing. The demo never drives play/pause off hover —
   the clips simply run.
3. **Assuming a background window still plays.** WebKit suspends media in a
   window that isn't frontmost, by design. Anything that must resume has to do
   it on `visibilitychange` / `focus`, and a retry driven by
   `requestAnimationFrame` will never fire — rAF is suspended in exactly that
   state, so the retry must be on a timer.

The on-screen HUD shows per-clip `readyState` + buffered percentage and running
counts of switches, stalls, waits, errors and requests, so all of this is
visible rather than asserted.

## Run it (macOS)

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug -DEACP_UNITY_BUILD=OFF
cmake --build build --target ServingHeavyContent
open build/Apps/WebView/ServingHeavyContent/ServingHeavyContent.app
```

On launch the page:

- shows the first clip **immediately, auto-playing** — no click, no hover; the
  stage is never black. All four clips run continuously; interaction only
  changes which one is visible;
- preloads all four clips, then runs a **hover stress pass** — a slow browse
  across every clip, then 24 switches at 60 ms — and reports switches, stalls,
  waits, errors and re-fetches;
- lets you hover the tiles yourself afterwards; and
- runs a checklist that should read **ALL CHECKS PASSED**:
  range probe (`bytes=0-0` → 206), a 1 MiB partial (`206` + exact
  `Content-Range`), a suffix range (`bytes=-65536`), an unsatisfiable range
  (`416`), a full `GET` (`200`, `Content-Length` matches, `Content-Type:
  video/mp4`), every clip reaching `readyState ≥ 3`, a clip auto-playing on
  launch with no user gesture, and every clip's `currentTime` actually
  advancing.

Note the playback check reports **SKIP**, not FAIL, when the window never comes
to the front — WebKit suspends media there by design, so there is nothing to
measure. Click the window and the clips start. Frame counters
(`getVideoPlaybackQuality`, `webkitDecodedFrameCount`) are *not* used as the
measure: they stay at zero in WKWebView even while a clip plays perfectly.

Open the Web Inspector's Network tab and you'll see a handful of efficient
range reads instead of a storm of whole-file copies.

## Windows (WebView2)

The Windows backend (`WebView-Windows.cpp`) resolves ranges through the same
`planStreamingResponse` plan and `streamingSchemes`, so the identical target
builds and verifies there — build `ServingHeavyContent` and confirm the same
on-screen checklist passes and the clip plays.

## Files

- `Main.cpp` — opens a window with an embedded webview (`embeddedOptions`), and
  registers a `report` script-message handler that prints the page's results to
  stdout.
- `web/dist/index.html` — the hover stage, the live HUD, the stress pass and the
  byte-range checklist.
- `web/dist/*.mp4` — the clips, **fetched by CMake at configure time**
  (hash-pinned, git-ignored). Not committed.
