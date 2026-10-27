# PIPP Modern Camera Support

Adds support for modern camera RAW files to [PIPP](https://sites.google.com/site/yourpipppage/) (Planetary Imaging PreProcessor) v2.5.9, which was last updated in 2017 and ships with dcraw 9.27.

## How it works

PIPP's built-in dcraw 9.27 doesn't recognize cameras released after ~2015. This patch adds [LibRaw](https://www.libraw.org/) 0.22 support via two drop-in replacements:

1. **pipp_dll.dll** (proxy) — Intercepts PIPP's `raw_decode_check` call. Tries the original dcraw first; if it fails and the file has a known RAW extension, checks it with LibRaw instead. All 240+ libpng exports are forwarded transparently.

2. **pipp_core.exe** (wrapper) — Sits between PIPP.exe and the original processing engine. When PIPP sends a RAW file path for processing, the wrapper converts it to a 16-bit TIFF via LibRaw and passes the TIFF to the original engine. Temp files are cleaned up automatically.

Both components load `raw_r.dll` (LibRaw) at runtime from the PIPP directory.

## Supported cameras

Any camera supported by LibRaw 0.22, including:
- **Sony**: Alpha 1 II, A7R V, A7 IV, A7C II, ZV-E1, etc.
- **Nikon**: Z 9, Z 8, Z 7 II, Z 6 III, Z f, etc.
- **Canon**: EOS R5 II, R6 III, R1, R7, R10, etc.
- **Fujifilm**: X-H2S, X-H2, X-T5, GFX 100 II, etc.
- **OM System**: OM-1, OM-1 Mark II, OM-5, etc.
- And hundreds more

## Prerequisites

- PIPP v2.5.9 installed (default: `C:\Program Files\PIPP`)
- Python with rawpy: `pip install rawpy` (provides the LibRaw DLL)

## Installation

```powershell
# From this directory (Run as Administrator, or it will self-elevate):
.\install_modern_camera_support.ps1

# Custom PIPP location:
.\install_modern_camera_support.ps1 -PippDir "D:\Astro\PIPP"
```

## Uninstallation

```powershell
.\install_modern_camera_support.ps1 -Uninstall
```

This restores the original `pipp_dll.dll` and `pipp_core.exe` from backups and removes the added files.

## Building from source

Requires MinGW-w64 GCC (install via `winget install BrechtSanders.WinLibs.POSIX.UCRT`).

```bash
# Proxy DLL (pipp_dll.dll)
gcc -shared -O2 -o pipp_dll.dll pipp_dll_proxy.c pipp_dll_proxy.def -lkernel32

# pipp_core wrapper (pipp_core.exe)
gcc -O2 -o pipp_core.exe pipp_core_wrapper.c -lkernel32
```

## Files

| File | Description |
|------|-------------|
| `pipp_dll_proxy.c` | Proxy DLL source — intercepts raw_decode_check, forwards png_* exports |
| `pipp_dll_proxy.def` | DEF file with all 254 exports (13 PIPP + ~241 libpng) |
| `pipp_core_wrapper.c` | pipp_core wrapper source — stdin relay with RAW-to-TIFF conversion |
| `install_modern_camera_support.ps1` | Installer/uninstaller script |
| `pipp_dll.dll` | Compiled proxy DLL (pre-built) |
| `pipp_core.exe` | Compiled wrapper (pre-built) |
| `pipp_raw2tiff.py` | Standalone RAW-to-TIFF converter (Python, optional) |
| `pipp_raw2tiff.c` | Standalone converter launcher source (optional) |

## Architecture

```
PIPP.exe (.NET)
  |
  |-- P/Invoke --> pipp_dll.dll (OUR PROXY)
  |                  |-- raw_decode_check() --> tries dcraw, falls back to LibRaw
  |                  |-- png_*() --> forwarded to pipp_dll_orig.dll
  |                  \-- loads raw_r.dll (LibRaw)
  |
  \-- stdin pipe --> pipp_core.exe (OUR WRAPPER)
                       |-- intercepts RAW file paths
                       |-- converts RAW -> 16-bit TIFF via LibRaw
                       \-- pipes TIFF path to pipp_core_orig.exe
```
