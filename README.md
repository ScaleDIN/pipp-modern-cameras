# PIPP Modern Camera Support

Adds support for modern camera RAW files to [PIPP](https://sites.google.com/site/astaborern/pipp) (Planetary Imaging PreProcessor) v2.5.9.

PIPP was last updated in 2017 and ships with dcraw 9.27, which can't decode RAW files from cameras released after ~2015. This patch adds [LibRaw](https://www.libraw.org/) 0.22 as a fallback, enabling support for hundreds of modern cameras — no changes to PIPP itself required.

## Supported cameras

Any camera supported by LibRaw 0.22, including:

| Brand | Models |
|-------|--------|
| Sony | Alpha 1 II, A7R V, A7 IV, A7C II, ZV-E1 |
| Nikon | Z 9, Z 8, Z 7 II, Z 6 III, Z f, Zfc |
| Canon | EOS R5 II, R6 III, R1, R7, R10, R50 |
| Fujifilm | X-H2S, X-H2, X-T5, X-T50, GFX 100 II |
| OM System | OM-1, OM-1 Mark II, OM-5 |
| Panasonic | S5 II, S5 IIX, GH6, G9 II |
| Leica | Q3, M11, SL3 |
| And more | Pentax, Hasselblad, Phase One, DJI, GoPro, etc. |

Older cameras that already work with PIPP continue to work — the original dcraw is tried first, and LibRaw is only used as a fallback.

## Quick install

1. Download the latest release zip
2. Extract it somewhere (e.g. your Desktop)
3. Right-click **`install.ps1`** and select **Run with PowerShell**
4. Click **Yes** when prompted for administrator access

If the Microsoft Visual C++ Redistributable (x64) isn't installed, the installer offers to download and install it from Microsoft. LibRaw needs it to run.

That's it. Launch PIPP normally and your modern camera RAW files will be accepted.

## Uninstall

Right-click **`uninstall.ps1`** and select **Run with PowerShell**. This restores PIPP to its original state.

Or from a PowerShell prompt:

```powershell
.\install_modern_camera_support.ps1 -Uninstall
```

## How it works

Two small drop-in files replace their originals in the PIPP directory (the originals are backed up):

1. **pipp_dll.dll** — A proxy that intercepts PIPP's `raw_decode_check` call. It tries the original dcraw first; if that fails and the file has a known RAW extension, it checks with LibRaw instead. All other DLL functions (libpng exports, etc.) pass through unchanged.

2. **pipp_core.exe** — A wrapper around the original processing engine. When PIPP sends a modern RAW file for processing, the wrapper converts it to a 16-bit TIFF via LibRaw and passes the TIFF to the original engine. Temp files are cleaned up automatically.

Both load **raw_r.dll** (LibRaw) at runtime from the PIPP directory. The original files are preserved as `pipp_dll_orig.dll` and `pipp_core_orig.exe`.

```
PIPP.exe
  ├── pipp_dll.dll (proxy)          ← can this camera's RAW be decoded?
  │     ├── try dcraw 9.27 first
  │     └── fall back to LibRaw
  │
  └── pipp_core.exe (wrapper)       ← process the image
        ├── convert RAW → 16-bit TIFF via LibRaw
        └── pass TIFF to pipp_core_orig.exe
```

## Building from source

Requires [MinGW-w64](https://www.mingw-w64.org/) GCC.

```bash
# Proxy DLL
gcc -shared -O2 -o pipp_dll.dll pipp_dll_proxy.c pipp_dll_proxy.def -lkernel32

# pipp_core wrapper
gcc -O2 -o pipp_core.exe pipp_core_wrapper.c -lkernel32
```

## License

The code in this repository is released under the [MIT License](LICENSE).

This project patches PIPP by replacing two files with compatible wrappers. It does not contain or distribute any PIPP code or binaries. The release zip bundles LibRaw (`raw_r.dll`, used under the LGPL 2.1) and Microsoft's OpenMP runtime (`vcomp140.dll`). See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) for their licenses.
