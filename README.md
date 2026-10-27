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
3. Double-click **`install.bat`**. If Windows SmartScreen says "Windows protected your PC", click **More info** → **Run anyway**.
4. Click **Yes** when prompted for administrator access

If the Microsoft Visual C++ Redistributable (x64) isn't installed, the installer offers to download and install it from Microsoft. LibRaw needs it to run.

That's it. Launch PIPP normally and your modern camera RAW files will be accepted.

## Uninstall

Double-click **`uninstall.bat`**. This restores PIPP to its original state.

The `.bat` files just run `install.ps1` / `uninstall.ps1` with PowerShell's execution policy bypassed for that one run, so you don't get the "running scripts is disabled on this system" error. You can also run the scripts directly, e.g. `powershell -ExecutionPolicy Bypass -File .\install.ps1`.

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

If MinGW is installed in a path with spaces (e.g. via WinGet) and `ld` can't find `default-manifest.o`, create a junction with `mklink /J C:\mingw64 "<actual MinGW path>"` and build from there.

## Maintaining this package

### Updating LibRaw

Both binaries load `raw_r.dll` at runtime and only use LibRaw's stable C API (`libraw_init`, `libraw_open_file`, `libraw_unpack`, `libraw_dcraw_process`, `libraw_dcraw_ppm_tiff_writer`, `libraw_set_output_*`, `libraw_close`, `libraw_version`). A newer LibRaw is normally a drop-in replacement with no rebuild.

1. Get a new `raw_r.dll` (x64, thread-safe build):
   - **Easiest:** `pip install --upgrade rawpy`, then copy `raw_r.dll` (and `vcomp140.dll`) from `site-packages/rawpy/`.
   - **Or** build [LibRaw](https://github.com/LibRaw/LibRaw) from source with MSVC and use the thread-safe `raw_r` library. It must be named `raw_r.dll`.
2. Copy it into a test PIPP install and open a RAW file from each major brand (Sony, Nikon, Canon, Fujifilm, OM System), plus a file from any newly supported camera. PIPP's About box shows the loaded LibRaw version.
3. Update the version numbers in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) (LibRaw, rawpy, and the bundled zlib / libjpeg-turbo versions, which you can find with `strings raw_r.dll | grep -i copyright`).
4. Check its dependencies with `objdump -p raw_r.dll | grep "DLL Name"`. If it needs runtime DLLs other than `MSVCP140`, `VCRUNTIME140`, `VCRUNTIME140_1` and `VCOMP140`, update the check in `install.ps1`.
5. Build a new release (see below).

If LibRaw ever renames or removes one of the functions above, LibRaw support silently turns off and PIPP falls back to dcraw, because both binaries check for every function they use at load time. Testing in step 2 catches this.

`pipp_core_orig.exe` can't be run standalone from a script; it only works when driven by PIPP.exe. To test the wrapper outside the GUI, put it in a folder next to a stub `pipp_core_orig.exe` built from [`tools/stub_orig.c`](tools/stub_orig.c), which logs each line it receives on stdin to `stub_log.txt`, then pipe it an option line and some RAW paths. Each RAW path should come through as a valid `.tif` in `%TEMP%\pipp_libraw_<pid>\`.

### Adding a new RAW file extension

New camera models using an existing extension (e.g. a new `.arw` body) need no code change. For a brand-new extension, add it to the extension list in **both** `pipp_dll_proxy.c` and `pipp_core_wrapper.c`, then rebuild both binaries.

### Changing the wrapper code

- `pipp_core.exe` must never write to stderr. PIPP shows anything on pipp_core's stderr as an error dialog.
- `pipp_core_orig.exe` reads all of its input from stdin and waits until stdin closes, so the wrapper must always close its end of the pipe.
- `pipp_dll.dll` must keep exporting everything `pipp_dll_orig.dll` exports. The libpng exports are forwarded through `pipp_dll_proxy.def`; don't remove entries from it.

### Building a release

The release zip contains:

```
install.bat          uninstall.bat
install.ps1          uninstall.ps1        README.md
LICENSE              THIRD-PARTY-NOTICES.md
licenses/LibRaw-LGPL-2.1.txt
pipp_dll.dll         pipp_core.exe
raw_r.dll            vcomp140.dll
```

1. Copy those files into `dist/pipp-modern-cameras/`.
2. Build the zip with forward-slash paths, so it extracts correctly on every OS:
   ```powershell
   tar.exe -a -c -f pipp-modern-cameras.zip -C dist\pipp-modern-cameras *
   ```
3. Test it: extract to a fresh folder, run `install.ps1` against a clean PIPP 2.5.9 install, open a modern RAW file, then run `uninstall.ps1` and confirm PIPP still works.
4. Tag the commit and upload the zip as a GitHub release, noting the LibRaw version in the release notes.

`dist/` and `*.zip` are git-ignored; only the source and scripts are tracked.

## License

The code in this repository is released under the [MIT License](LICENSE).

This project patches PIPP by replacing two files with compatible wrappers. It does not contain or distribute any PIPP code or binaries. The release zip bundles LibRaw (`raw_r.dll`, used under the LGPL 2.1) and Microsoft's OpenMP runtime (`vcomp140.dll`). See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) for their licenses.
