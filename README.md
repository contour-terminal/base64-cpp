base64-cpp
==========

**DO NOT USE YET. IT'S STILL IN BOOTSTRAPPING PHASE**

Base64 decoding/encoding header-only C++17 library.

The SIMD accelerated algorithms are heavily based on
the work by Wojciech Muła with the original work being
located at https://github.com/WojciechMula/base64simd/

This library aims to provide an easy to use API with
runtime autodetection of CPU features to automatically choose
the best algorithm available.

TODO
----

- [ ] create Github CI for building and running tests on Ubuntu 18.04, 20.04, ArchLinux
- [ ] add and make use of CPU feature detection (ensure MSVC and ARM64 support).
- [ ] decoder: add more SIMD versions (AVX, maybe SSE4?, ...?)
- [ ] encoder: missing completely for now
- [ ] add examples how to use and how to integrate via `FetchContent` and via `CPM`
- [ ] make use of this lib in Contour for base64 decoding in VT streams (image processing)
- [ ] automated benchmarks and graph generation. also integrated into CI.
- [ ] the usual badges ;-)
