#!/usr/bin/env python3
"""
pipp_raw2tiff - RAW file checker and converter for PIPP modern camera support.

Usage:
    pipp_raw2tiff.py --check <rawfile>        Check if file can be decoded (exit 0=yes)
    pipp_raw2tiff.py --convert <rawfile> <outdir>  Convert RAW to 16-bit TIFF
    pipp_raw2tiff.py --convert-list <listfile> <outdir>  Convert multiple files
"""

import sys
import os

def check_raw(filepath):
    """Check if a RAW file can be decoded. Returns 0 on success, 1 on failure."""
    try:
        import rawpy
        raw = rawpy.imread(filepath)
        raw.close()
        return 0
    except Exception:
        return 1


def convert_raw_to_tiff(filepath, outdir):
    """Convert a RAW file to 16-bit TIFF. Returns the output path or None."""
    try:
        import rawpy
        import struct

        raw = rawpy.imread(filepath)
        rgb = raw.postprocess(
            output_bps=16,
            use_camera_wb=True,
            no_auto_bright=True,
            output_color=rawpy.ColorSpace.sRGB,
        )
        raw.close()

        basename = os.path.splitext(os.path.basename(filepath))[0]
        outpath = os.path.join(outdir, basename + ".tif")
        write_tiff_16bit(rgb, outpath)
        return outpath
    except Exception as e:
        print(f"Error converting {filepath}: {e}", file=sys.stderr)
        return None


def write_tiff_16bit(rgb_array, outpath):
    """Write a 16-bit RGB TIFF file (no external dependency needed)."""
    import struct

    height, width, channels = rgb_array.shape
    assert channels == 3

    # TIFF header
    bps = 16
    rows_per_strip = height
    strip_size = width * height * channels * 2  # 2 bytes per sample

    # IFD entries
    ifd_entries = [
        (256, 3, 1, width),           # ImageWidth
        (257, 3, 1, height),          # ImageLength
        (258, 3, 3, 0),              # BitsPerSample (pointer, filled later)
        (259, 3, 1, 1),              # Compression = None
        (262, 3, 1, 2),              # PhotometricInterpretation = RGB
        (273, 4, 1, 0),              # StripOffsets (filled later)
        (277, 3, 1, 3),              # SamplesPerPixel
        (278, 3, 1, rows_per_strip), # RowsPerStrip
        (279, 4, 1, strip_size),     # StripByteCounts
        (282, 5, 1, 0),              # XResolution (pointer)
        (283, 5, 1, 0),              # YResolution (pointer)
        (296, 3, 1, 2),              # ResolutionUnit = inch
        (339, 3, 1, 1),              # SampleFormat = unsigned int
    ]

    num_entries = len(ifd_entries)

    # Calculate offsets
    header_size = 8  # TIFF header
    ifd_size = 2 + num_entries * 12 + 4  # count + entries + next IFD pointer

    # Extra data after IFD: BitsPerSample (6 bytes) + XRes (8 bytes) + YRes (8 bytes)
    extra_offset = header_size + ifd_size
    bps_offset = extra_offset
    xres_offset = bps_offset + 6
    yres_offset = xres_offset + 8
    strip_offset = yres_offset + 8

    # Update IFD entries with actual offsets
    ifd_entries[2] = (258, 3, 3, bps_offset)     # BitsPerSample
    ifd_entries[5] = (273, 4, 1, strip_offset)    # StripOffsets
    ifd_entries[9] = (282, 5, 1, xres_offset)     # XResolution
    ifd_entries[10] = (283, 5, 1, yres_offset)    # YResolution

    with open(outpath, 'wb') as f:
        # TIFF header (little-endian)
        f.write(b'II')  # Little endian
        f.write(struct.pack('<H', 42))  # TIFF magic
        f.write(struct.pack('<I', header_size))  # IFD offset

        # IFD
        f.write(struct.pack('<H', num_entries))
        for tag, dtype, count, value in ifd_entries:
            f.write(struct.pack('<HHI', tag, dtype, count))
            if dtype == 3 and count == 1:
                f.write(struct.pack('<HH', value, 0))
            else:
                f.write(struct.pack('<I', value))
        f.write(struct.pack('<I', 0))  # Next IFD = none

        # Extra data
        # BitsPerSample: 16, 16, 16
        f.write(struct.pack('<HHH', 16, 16, 16))
        # XResolution: 72/1
        f.write(struct.pack('<II', 72, 1))
        # YResolution: 72/1
        f.write(struct.pack('<II', 72, 1))

        # Image data (16-bit RGB, little-endian)
        data = rgb_array.astype('<u2').tobytes()
        f.write(data)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1

    mode = sys.argv[1]
    if mode == '--check':
        return check_raw(sys.argv[2])

    elif mode == '--convert':
        if len(sys.argv) < 4:
            print("Usage: pipp_raw2tiff.py --convert <rawfile> <outdir>")
            return 1
        outdir = sys.argv[3]
        os.makedirs(outdir, exist_ok=True)
        result = convert_raw_to_tiff(sys.argv[2], outdir)
        if result:
            print(result)
            return 0
        return 1

    elif mode == '--convert-list':
        if len(sys.argv) < 4:
            print("Usage: pipp_raw2tiff.py --convert-list <listfile> <outdir>")
            return 1
        listfile = sys.argv[2]
        outdir = sys.argv[3]
        os.makedirs(outdir, exist_ok=True)
        with open(listfile, 'r') as fh:
            files = [line.strip() for line in fh if line.strip()]
        for filepath in files:
            result = convert_raw_to_tiff(filepath, outdir)
            if result:
                print(result)
            else:
                print(f"FAILED:{filepath}")
        return 0

    else:
        print(f"Unknown mode: {mode}")
        return 1


if __name__ == '__main__':
    sys.exit(main())
