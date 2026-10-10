# MD4C parser

`md4c.c` and `md4c.h` based on Martin Mitáš' MD4C 0.5.2, with the local allocation-cleanup modification below.

- Repository: https://github.com/mity/md4c
- Commit: `729e6b8b320caa96328968ab27d7db2235e4fb47` (`release-0.5.2`)
- License: MIT, see LICENSE.md and original file headers.
- Original upstream Git blob SHA-1: md4c.c `812bde535850dfff546c357ed0ed3e6b9a177cfb`; md4c.h `8d6be1cb4631528e7c387ca2e49ce42d1ef87477`.

Only the parser is included; HTML rendering is not used. CMakeLists.txt is the local ESP-IDF adapter. Record every local source modification here.
# Local modification / 本地修改

2026-10-05: `md4c.c` attribute cleanup now retains successfully reallocated
storage and clears freed builders. This handles partial allocation failure
and repeated cleanup; the public parser API is unchanged. The header remains
byte-identical to the pinned upstream. MIT attribution is retained.

2026-10-06: initialize the local reference-label line index to zero. GCC 15
at ESP-IDF's O2 level could not prove that the successful label parser always
sets this output, causing `-Werror=maybe-uninitialized`. Parsing behavior and
the public API are unchanged; the adapter keeps warning checks enabled.
