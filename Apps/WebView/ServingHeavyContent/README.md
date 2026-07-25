# ServingHeavyContent

A minimal embedded-webview example that serves **heavy content** — a ~10 MB
1080p `heavy.mp4` — out of the embedded resource bundle and plays it in a
`<video>` element, while asserting the embedded scheme's HTTP byte-range
behavior on screen.

The clip isn't committed: CMake **downloads it at configure time** (see
`CMakeLists.txt`), so the repo stays light. It's Big Buck Bunny (CC-BY,
&copy; Blender Foundation).

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
example plays a deliberately heavy clip to make that self-evident.

## Run it (macOS)

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug -DEACP_UNITY_BUILD=OFF
cmake --build build --target ServingHeavyContent
open build/Apps/WebView/ServingHeavyContent/ServingHeavyContent.app
```

On launch the page:

- plays the embedded 1080p clip smoothly, and
- runs a checklist that should read **ALL CHECKS PASSED**:
  range probe (`bytes=0-0` → 206), a 1 MiB partial (`206` + exact
  `Content-Range`), a suffix range (`bytes=-65536`), an unsatisfiable range
  (`416`), a full `GET` (`200`, `Content-Length` matches, `Content-Type:
  video/mp4`), and the `<video>` element reaching a playing state.

Open the Web Inspector's Network tab and you'll see a handful of efficient
range reads instead of a storm of whole-file copies.

## Windows (WebView2)

The Windows backend (`WebView-Windows.cpp`) resolves ranges through the same
`planStreamingResponse` plan and `streamingSchemes`, so the identical target
builds and verifies there — build `ServingHeavyContent` and confirm the same
on-screen checklist passes and the clip plays.

## Files

- `Main.cpp` — opens a window with an embedded webview (`embeddedOptions`).
- `web/dist/index.html` — the `<video>` + the byte-range checklist.
- `web/dist/heavy.mp4` — the heavy 1080p clip, **fetched by CMake at configure
  time** (hash-pinned, git-ignored). Not committed.
