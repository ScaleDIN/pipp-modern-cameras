/*
 * pipp_dll proxy - intercepts raw_decode_check to support modern cameras
 * by calling LibRaw (raw_r.dll) directly for files dcraw 9.27 can't handle.
 *
 * All other exports are forwarded to the original DLL (pipp_dll_orig.dll).
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static HMODULE g_orig_dll = NULL;
static HMODULE g_libraw_dll = NULL;
static char g_dll_dir[MAX_PATH] = {0};

/* Function pointer types for forwarded functions */
typedef void (__cdecl *fn_set_abort_request)(int);
typedef int  (__cdecl *fn_get_pipp_version)(char*);
typedef int  (__cdecl *fn_get_dcraw_version)(char*);
typedef int  (__cdecl *fn_orig_raw_decode_check)(const char*);
typedef int  (__cdecl *fn_date_time_check)(const char*, const char*, char*);
typedef int  (__cdecl *fn_video_check)(const char*, int*, int*, int*, int*, char*, int*, int*);
typedef int  (__cdecl *fn_ser_check)(const char*, int*, int*, int*, int*, char*);
typedef int  (__cdecl *fn_ser_info)(const char*, const char*);
typedef int  (__cdecl *fn_avi_info)(const char*, const char*);
typedef int  (__cdecl *fn_fits_info)(const char*, const char*);
typedef int  (__cdecl *fn_stack_frames)(const char**, const char*, const char*, const char*, int);
typedef void (__cdecl *fn_set_progress_fn)(void*);
typedef void (__cdecl *fn_set_gui_print_fn)(void*);

static fn_set_abort_request      orig_set_abort_request;
static fn_get_pipp_version       orig_get_pipp_version;
static fn_get_dcraw_version      orig_get_dcraw_version;
static fn_orig_raw_decode_check  orig_raw_decode_check;
static fn_date_time_check        orig_date_time_check;
static fn_video_check            orig_video_check;
static fn_ser_check              orig_ser_check;
static fn_ser_info               orig_ser_info;
static fn_avi_info               orig_avi_info;
static fn_fits_info              orig_fits_info;
static fn_stack_frames           orig_stack_frames;
static fn_set_progress_fn        orig_set_progress_fn;
static fn_set_gui_print_fn       orig_set_gui_print_fn;

/* LibRaw C API function pointers */
typedef void* (__cdecl *fn_libraw_init)(unsigned int flags);
typedef int   (__cdecl *fn_libraw_open_file)(void* lr, const char* fname);
typedef int   (__cdecl *fn_libraw_unpack)(void* lr);
typedef void  (__cdecl *fn_libraw_close)(void* lr);
typedef void  (__cdecl *fn_libraw_recycle)(void* lr);
typedef const char* (__cdecl *fn_libraw_version)(void);

static fn_libraw_init       p_libraw_init;
static fn_libraw_open_file  p_libraw_open_file;
static fn_libraw_unpack     p_libraw_unpack;
static fn_libraw_close      p_libraw_close;
static fn_libraw_recycle    p_libraw_recycle;
static fn_libraw_version    p_libraw_version;

static int str_ends_with_ci(const char *str, const char *suffix) {
    size_t str_len = strlen(str);
    size_t suf_len = strlen(suffix);
    if (suf_len > str_len) return 0;
    return _stricmp(str + str_len - suf_len, suffix) == 0;
}

static int is_raw_extension(const char *filename) {
    const char *exts[] = {
        ".cr2", ".cr3", ".crw", ".nef", ".nrw", ".arw", ".srf", ".sr2",
        ".orf", ".raf", ".rw2", ".rwl", ".dng", ".pef", ".ptx",
        ".3fr", ".fff", ".iiq", ".mrw", ".mdc", ".dcr", ".kdc",
        ".raw", ".rwz", ".erf", ".mef", ".mos", ".srw",
        ".x3f", ".bay", ".cs1", ".gpr", NULL
    };
    for (int i = 0; exts[i]; i++) {
        if (str_ends_with_ci(filename, exts[i])) return 1;
    }
    return 0;
}

/*
 * Check if LibRaw can decode this file.
 * Uses the LibRaw C API directly - no subprocess, no TIFF intermediate.
 */
static int libraw_check_file(const char *filename) {
    if (!p_libraw_init || !p_libraw_open_file || !p_libraw_close) {
        return -1;
    }

    void *lr = p_libraw_init(0);
    if (!lr) return -1;

    int ret = p_libraw_open_file(lr, filename);
    p_libraw_close(lr);

    return ret;  /* 0 = success */
}

/*
 * Intercepted raw_decode_check:
 * First tries the original dcraw-based check. If that fails and the file
 * has a known RAW extension, tries LibRaw directly via raw_r.dll.
 */
__declspec(dllexport) int __cdecl raw_decode_check(const char *filename) {
    /* Try original dcraw first */
    if (orig_raw_decode_check) {
        int result = orig_raw_decode_check(filename);
        if (result == 0) return 0;
    }

    /* Original failed or unavailable - try LibRaw for known RAW extensions */
    if (is_raw_extension(filename)) {
        return libraw_check_file(filename);
    }

    return -1;
}

__declspec(dllexport) int __cdecl get_dcraw_version(char *buf) {
    if (buf) {
        const char *lr_ver = p_libraw_version ? p_libraw_version() : "?";
        snprintf(buf, 256, "9.27 + LibRaw %s", lr_ver);
    }
    return 0;
}

/* Forward all other exports to the original DLL */
__declspec(dllexport) void __cdecl set_abort_request(int abort) {
    if (orig_set_abort_request) orig_set_abort_request(abort);
}

__declspec(dllexport) int __cdecl get_pipp_version(char *buf) {
    if (orig_get_pipp_version) return orig_get_pipp_version(buf);
    return -1;
}

__declspec(dllexport) int __cdecl date_time_check(const char *fn, const char *mask, char *out) {
    if (orig_date_time_check) return orig_date_time_check(fn, mask, out);
    return -1;
}

__declspec(dllexport) int __cdecl video_check(const char *fn, int *w, int *h,
    int *fps_rate, int *fps_scale, char *codec, int *colour, int *bayer) {
    if (orig_video_check) return orig_video_check(fn, w, h, fps_rate, fps_scale, codec, colour, bayer);
    return -1;
}

__declspec(dllexport) int __cdecl ser_check(const char *fn, int *w, int *h,
    int *fps_rate, int *fps_scale, char *colour_id) {
    if (orig_ser_check) return orig_ser_check(fn, w, h, fps_rate, fps_scale, colour_id);
    return -1;
}

__declspec(dllexport) int __cdecl ser_info(const char *ser_fn, const char *tmp_fn) {
    if (orig_ser_info) return orig_ser_info(ser_fn, tmp_fn);
    return -1;
}

__declspec(dllexport) int __cdecl avi_info(const char *avi_fn, const char *tmp_fn) {
    if (orig_avi_info) return orig_avi_info(avi_fn, tmp_fn);
    return -1;
}

__declspec(dllexport) int __cdecl fits_info(const char *fits_fn, const char *tmp_fn) {
    if (orig_fits_info) return orig_fits_info(fits_fn, tmp_fn);
    return -1;
}

__declspec(dllexport) int __cdecl stack_frames(const char **dark_files,
    const char *tmp_frame, const char *stacked_tiff, const char *stacked_pgm, int frames) {
    if (orig_stack_frames) return orig_stack_frames(dark_files, tmp_frame, stacked_tiff, stacked_pgm, frames);
    return -1;
}

__declspec(dllexport) void __cdecl set_progress_fn(void *cb) {
    if (orig_set_progress_fn) orig_set_progress_fn(cb);
}

__declspec(dllexport) void __cdecl set_gui_print_fn(void *cb) {
    if (orig_set_gui_print_fn) orig_set_gui_print_fn(cb);
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        GetModuleFileNameA(hinstDLL, g_dll_dir, MAX_PATH);
        char *last_sep = strrchr(g_dll_dir, '\\');
        if (last_sep) *(last_sep + 1) = '\0';

        /* Load original pipp_dll */
        char orig_path[MAX_PATH];
        snprintf(orig_path, MAX_PATH, "%spipp_dll_orig.dll", g_dll_dir);
        g_orig_dll = LoadLibraryA(orig_path);
        if (g_orig_dll) {
            orig_set_abort_request = (fn_set_abort_request)GetProcAddress(g_orig_dll, "set_abort_request");
            orig_get_pipp_version  = (fn_get_pipp_version)GetProcAddress(g_orig_dll, "get_pipp_version");
            orig_get_dcraw_version = (fn_get_dcraw_version)GetProcAddress(g_orig_dll, "get_dcraw_version");
            orig_raw_decode_check  = (fn_orig_raw_decode_check)GetProcAddress(g_orig_dll, "raw_decode_check");
            orig_date_time_check   = (fn_date_time_check)GetProcAddress(g_orig_dll, "date_time_check");
            orig_video_check       = (fn_video_check)GetProcAddress(g_orig_dll, "video_check");
            orig_ser_check         = (fn_ser_check)GetProcAddress(g_orig_dll, "ser_check");
            orig_ser_info          = (fn_ser_info)GetProcAddress(g_orig_dll, "ser_info");
            orig_avi_info          = (fn_avi_info)GetProcAddress(g_orig_dll, "avi_info");
            orig_fits_info         = (fn_fits_info)GetProcAddress(g_orig_dll, "fits_info");
            orig_stack_frames      = (fn_stack_frames)GetProcAddress(g_orig_dll, "stack_frames");
            orig_set_progress_fn   = (fn_set_progress_fn)GetProcAddress(g_orig_dll, "set_progress_fn");
            orig_set_gui_print_fn  = (fn_set_gui_print_fn)GetProcAddress(g_orig_dll, "set_gui_print_fn");
        }

        /* Load LibRaw (raw_r.dll) */
        char libraw_path[MAX_PATH];
        snprintf(libraw_path, MAX_PATH, "%sraw_r.dll", g_dll_dir);
        g_libraw_dll = LoadLibraryA(libraw_path);
        if (g_libraw_dll) {
            p_libraw_init      = (fn_libraw_init)GetProcAddress(g_libraw_dll, "libraw_init");
            p_libraw_open_file = (fn_libraw_open_file)GetProcAddress(g_libraw_dll, "libraw_open_file");
            p_libraw_unpack    = (fn_libraw_unpack)GetProcAddress(g_libraw_dll, "libraw_unpack");
            p_libraw_close     = (fn_libraw_close)GetProcAddress(g_libraw_dll, "libraw_close");
            p_libraw_recycle   = (fn_libraw_recycle)GetProcAddress(g_libraw_dll, "libraw_recycle");
            p_libraw_version   = (fn_libraw_version)GetProcAddress(g_libraw_dll, "libraw_version");
        }

    } else if (fdwReason == DLL_PROCESS_DETACH) {
        if (g_orig_dll) { FreeLibrary(g_orig_dll); g_orig_dll = NULL; }
        if (g_libraw_dll) { FreeLibrary(g_libraw_dll); g_libraw_dll = NULL; }
    }
    return TRUE;
}
