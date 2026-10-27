# Third-Party Notices

The MIT license in `LICENSE` covers only the original code in this repository
(the proxy DLL, the core wrapper, the converter and the install scripts).
The release zip also ships the third-party components below, each under its
own license. They are redistributed unmodified.

## LibRaw 0.22.1 (`raw_r.dll`)

- Copyright (C) 2008-2026 LibRaw LLC
- https://www.libraw.org/
- License: dual-licensed under the GNU Lesser General Public License 2.1
  or the Common Development and Distribution License 1.0, at the user's
  option. This project uses it under the LGPL 2.1; the full text is in
  [`licenses/LibRaw-LGPL-2.1.txt`](licenses/LibRaw-LGPL-2.1.txt).
- Source code: https://github.com/LibRaw/LibRaw
- The binary is the unmodified `raw_r.dll` built by the
  [rawpy](https://github.com/letmaik/rawpy) project (version 0.27.1).
  This project loads it dynamically at runtime, so you can replace it with
  your own build of LibRaw.

`raw_r.dll` includes the following libraries, statically linked:

### zlib 1.3.2

Copyright (C) 1995-2026 Jean-loup Gailly and Mark Adler

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the
use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software in a
   product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

### libjpeg-turbo

Copyright (C) 1991-2026 The libjpeg-turbo Project and many others.
https://libjpeg-turbo.org/

This software is based in part on the work of the Independent JPEG Group.

libjpeg-turbo is covered by the IJG License and the Modified (3-clause) BSD
License. See https://github.com/libjpeg-turbo/libjpeg-turbo/blob/main/LICENSE.md
for the full terms.

## Microsoft OpenMP runtime (`vcomp140.dll`)

Copyright (C) Microsoft Corporation. Redistributed as part of the Microsoft
Visual C++ Redistributable, under the distributable code terms of the
Microsoft Visual Studio license. It is also installed by the official
Microsoft Visual C++ Redistributable (x64):
https://aka.ms/vs/17/release/vc_redist.x64.exe

## Not included

- **PIPP**, Copyright (C) Chris Garry. This repository contains no PIPP code
  or binaries. The installer only renames the user's own installed copies to
  `pipp_dll_orig.dll` and `pipp_core_orig.exe`.
- **dcraw** is statically linked inside PIPP's own binaries and is not
  distributed here.
