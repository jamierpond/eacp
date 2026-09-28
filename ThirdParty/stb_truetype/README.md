# stb_truetype

[stb_truetype.h](https://github.com/nothings/stb) v1.26, from commit
`2c980bb59875b0d32144a71867fbdebb2f77cd20`, unmodified. Public domain or MIT
at your choice; `LICENSE` is upstream's.

Built only on Android, where it backs `Font`, `TextMetrics` and
`SoftwareContext::drawText`. `stb_truetype.c` is its one implementation unit;
only `eacp-graphics` links it, PRIVATE.

To update: replace `stb_truetype.h` and `LICENSE` with a newer commit's, and
change the version above.
