# Heaviside

OpenColorIO compiled to WebAssembly, for baking LUTs in a browser.

Load an OCIO config (an `.ocio` with its LUT files, an `.ocioz`, or a built-in `ocio://`
config), list its colour spaces, displays and views, and bake any source space through a
display and view into a 3D LUT. Nothing is sent anywhere; the config is read from memory.

It is a LUT baker, not a pixel pipeline. OCIO runs on the CPU for the few milliseconds a bake
takes, and the result goes to whatever already draws the picture (a WebGPU or WebGL shader).
It was written for [Nearside](https://github.com/Tedcharlesbrown/Nearside), which will use it that way.

Status: working build, not yet published. OCIO 2.5.2.

## Use

```js
import Heaviside from './heaviside.mjs';

const ocio = await Heaviside();
ocio.FS.writeFile('/config.ocioz', new Uint8Array(await file.arrayBuffer()));
ocio.load('/config.ocioz');

const { spaces, displays } = ocio.info();
const lut = ocio.bakeView('ARRI LogC4', 'Rec.1886 Rec.709 - Display', 'ACES 2.0 - SDR 100 nits (Rec.709)', 65);
// Float32Array of 65 * 65 * 65 * 3 values, red varying fastest (the .cube order)
```

A plain `.ocio` usually refers to LUT files by relative path. Write them into `FS` next to
the config, keeping the folder layout, before calling `load`.

| Function | Returns |
|---|---|
| `version()` | OCIO version string |
| `build()` | Heaviside's version, `YY.MMDD.HHMM` (`dev` for a local build) |
| `load(path)` | nothing; throws an `Error` with OCIO's message if the config fails to load or validate |
| `info()` | `{ name, spaces: [{ name, family, encoding, display }], displays: [{ name, views }] }` |
| `bakeView(source, display, view, size)` | `Float32Array`, source code values 0 to 1 through the display and view |
| `bakeSpace(source, destination, size)` | `Float32Array`, source to destination colour space |

## Build

Needs [Emscripten](https://emscripten.org/docs/getting_started/downloads.html), CMake 3.20+
and Ninja.

```
git clone --recursive <this repo>
emcmake cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

OCIO is a submodule in `extern/OpenColorIO`. Its dependencies (yaml-cpp, expat, Imath,
pystring, minizip-ng, zlib) are downloaded and built by OCIO's own CMake. On Windows, set
`git config core.longpaths true` first or the minizip-ng clone fails on path length.

The output is `build/heaviside.mjs` and `build/heaviside.wasm` (about 3 MB).

Build notes:

- Every object is compiled with `-fwasm-exceptions`. OCIO tries each LUT reader in turn and
  relies on catching the failures, so without it a valid file can be rejected.
- SIMD is off. Browsers support WebAssembly SIMD, but OCIO's SIMD code is written for SSE.
- No threads, so the page needs no cross-origin isolation headers.

## Releases

The version is the commit's time in New York, `YY.MMDD.HHMM` (`26.1015.1215`), the same
scheme as Nearside and Farside. `.github/workflows/release.yml` builds and tests every push
and pull request, and every push to `main` that passes becomes a GitHub Release tagged with
that version, holding `heaviside.mjs`, `heaviside.wasm` and `SHA256SUMS`. There is no
tagging by hand.

Users pin a release by tag and checksum, so a published release is never replaced; a
version that already has one is skipped. CI uses Emscripten 6.0.12, the version the local
builds were made with. A local build reports `dev` unless configured with
`-DHEAVISIDE_VERSION=...`.

## Test

Compares bakes against PyOpenColorIO.

```
pip install opencolorio
python test/reference.py reference.json ocio://studio-config-latest path/to/other.ocio
node test/compare.mjs reference.json
```

Measured on Windows against PyOpenColorIO 2.5.2:

- Built-in studio config: worst difference under 0.001 (values 0 to 1).
- A config whose ACES views are LUT files: worst up to 0.004 at single grid points, mean
  under 0.00005.

The cause of the larger worst case is not confirmed. A likely candidate is the native
library's SSE approximations, which this build does not use.

Timing in Node on a laptop: config load 20 to 40 ms, a 65-point bake 20 to 90 ms. The first
bake that touches a large LUT file takes about 0.75 s while OCIO reads it.

## License

BSD-3-Clause, the same as OpenColorIO. See [LICENSE](LICENSE). OpenColorIO and its
dependencies keep their own licenses.
