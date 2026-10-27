/*
 * pipp_core wrapper - sits between PIPP.exe and pipp_core_orig.exe.
 * Converts RAW files that old dcraw can't handle to 16-bit TIFF via LibRaw.
 *
 * pipp_core_orig.exe receives ALL input (arguments + file paths) via stdin.
 * This wrapper:
 *   1. When stdin is a pipe (PIPP.exe GUI): relays lines, converting RAW paths
 *   2. When CLI args present: sends them via stdin pipe to pipp_core_orig
 *   3. Cleans up temp TIFFs after processing
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef void* (__cdecl *fn_libraw_init)(unsigned int);
typedef int   (__cdecl *fn_libraw_open_file)(void*, const char*);
typedef int   (__cdecl *fn_libraw_unpack)(void*);
typedef int   (__cdecl *fn_libraw_dcraw_process)(void*);
typedef int   (__cdecl *fn_libraw_dcraw_ppm_tiff_writer)(void*, const char*);
typedef void  (__cdecl *fn_libraw_close)(void*);
typedef void  (__cdecl *fn_libraw_set_output_bps)(void*, int);
typedef void  (__cdecl *fn_libraw_set_no_auto_bright)(void*, int);
typedef void  (__cdecl *fn_libraw_set_output_tif)(void*, int);

static HMODULE g_libraw = NULL;
static fn_libraw_init                  p_init;
static fn_libraw_open_file             p_open_file;
static fn_libraw_unpack                p_unpack;
static fn_libraw_dcraw_process         p_dcraw_process;
static fn_libraw_dcraw_ppm_tiff_writer p_tiff_writer;
static fn_libraw_close                 p_close;
static fn_libraw_set_output_bps        p_set_output_bps;
static fn_libraw_set_no_auto_bright    p_set_no_auto_bright;
static fn_libraw_set_output_tif        p_set_output_tif;

static char g_exe_dir[MAX_PATH];
static char g_temp_dir[MAX_PATH];
static char *g_temp_files[4096];
static int g_temp_count = 0;
static int g_have_libraw = 0;

static int str_ends_with_ci(const char *str, const char *suffix) {
    size_t slen = strlen(str), xlen = strlen(suffix);
    if (xlen > slen) return 0;
    return _stricmp(str + slen - xlen, suffix) == 0;
}

static int is_raw_extension(const char *fn) {
    const char *exts[] = {
        ".cr2",".cr3",".crw",".nef",".nrw",".arw",".srf",".sr2",
        ".orf",".raf",".rw2",".rwl",".dng",".pef",".ptx",
        ".3fr",".fff",".iiq",".mrw",".mdc",".dcr",".kdc",
        ".raw",".rwz",".erf",".mef",".mos",".srw",
        ".x3f",".bay",".cs1",".gpr", NULL
    };
    for (int i = 0; exts[i]; i++)
        if (str_ends_with_ci(fn, exts[i])) return 1;
    return 0;
}

static int load_libraw(void) {
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "%sraw_r.dll", g_exe_dir);
    g_libraw = LoadLibraryA(path);
    if (!g_libraw) return 0;
    p_init             = (fn_libraw_init)GetProcAddress(g_libraw, "libraw_init");
    p_open_file        = (fn_libraw_open_file)GetProcAddress(g_libraw, "libraw_open_file");
    p_unpack           = (fn_libraw_unpack)GetProcAddress(g_libraw, "libraw_unpack");
    p_dcraw_process    = (fn_libraw_dcraw_process)GetProcAddress(g_libraw, "libraw_dcraw_process");
    p_tiff_writer      = (fn_libraw_dcraw_ppm_tiff_writer)GetProcAddress(g_libraw, "libraw_dcraw_ppm_tiff_writer");
    p_close            = (fn_libraw_close)GetProcAddress(g_libraw, "libraw_close");
    p_set_output_bps   = (fn_libraw_set_output_bps)GetProcAddress(g_libraw, "libraw_set_output_bps");
    p_set_no_auto_bright = (fn_libraw_set_no_auto_bright)GetProcAddress(g_libraw, "libraw_set_no_auto_bright");
    p_set_output_tif   = (fn_libraw_set_output_tif)GetProcAddress(g_libraw, "libraw_set_output_tif");
    return (p_init && p_open_file && p_unpack && p_dcraw_process && p_tiff_writer && p_close);
}

static char* convert_raw(const char *rawpath) {
    if (!p_init) return NULL;
    void *lr = p_init(0);
    if (!lr) return NULL;
    if (p_open_file(lr, rawpath) != 0) { p_close(lr); return NULL; }
    if (p_unpack(lr) != 0) { p_close(lr); return NULL; }
    p_set_output_bps(lr, 16);
    p_set_no_auto_bright(lr, 1);
    p_set_output_tif(lr, 1);
    if (p_dcraw_process(lr) != 0) { p_close(lr); return NULL; }

    char *bn = strrchr(rawpath, '\\');
    if (!bn) bn = strrchr(rawpath, '/');
    bn = bn ? bn + 1 : (char*)rawpath;
    char outpath[MAX_PATH];
    snprintf(outpath, MAX_PATH, "%s\\%s.tif", g_temp_dir, bn);
    if (p_tiff_writer(lr, outpath) != 0) { p_close(lr); return NULL; }
    p_close(lr);

    char *r = _strdup(outpath);
    if (g_temp_count < 4096) g_temp_files[g_temp_count++] = r;
    return r;
}

/* Write a line to a pipe handle */
static void pipe_write_line(HANDLE hPipe, const char *line) {
    DWORD written;
    WriteFile(hPipe, line, (DWORD)strlen(line), &written, NULL);
    WriteFile(hPipe, "\n", 1, &written, NULL);
}

/* Try to convert a line if it's a RAW file path, return converted or original */
static const char* maybe_convert(const char *line) {
    size_t len = strlen(line);
    if (g_have_libraw && len > 4 && line[0] != '-' && is_raw_extension(line) &&
        GetFileAttributesA(line) != INVALID_FILE_ATTRIBUTES) {
        char *tiff = convert_raw(line);
        if (tiff) return tiff;
    }
    return line;
}

static void cleanup(void) {
    for (int i = 0; i < g_temp_count; i++) {
        if (g_temp_files[i]) { DeleteFileA(g_temp_files[i]); free(g_temp_files[i]); }
    }
    g_temp_count = 0;
    RemoveDirectoryA(g_temp_dir);
    if (g_libraw) FreeLibrary(g_libraw);
}

int main(int argc, char *argv[]) {
    GetModuleFileNameA(NULL, g_exe_dir, MAX_PATH);
    char *sep = strrchr(g_exe_dir, '\\');
    if (sep) *(sep + 1) = '\0';

    char temp_base[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_base);
    snprintf(g_temp_dir, MAX_PATH, "%spipp_libraw_%lu", temp_base, GetCurrentProcessId());
    CreateDirectoryA(g_temp_dir, NULL);

    g_have_libraw = load_libraw();

    char orig_path[MAX_PATH];
    snprintf(orig_path, MAX_PATH, "%spipp_core_orig.exe", g_exe_dir);

    /* Always create a pipe for pipp_core_orig's stdin */
    HANDLE hChildStdinRd, hChildStdinWr;
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0);
    SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hChildStdinRd;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    /* pipp_core_orig gets no CLI args - everything goes via stdin */
    char cmdline[MAX_PATH + 4];
    snprintf(cmdline, sizeof(cmdline), "\"%s\"", orig_path);

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        cleanup();
        return 1;
    }
    CloseHandle(hChildStdinRd);

    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD stdin_type = GetFileType(hStdin);
    int have_pipe_stdin = (stdin_type == FILE_TYPE_PIPE);

    if (have_pipe_stdin) {
        /*
         * PIPP.exe GUI mode: relay stdin lines through, converting RAW paths.
         * pipp_core_orig expects arguments and file paths all via stdin.
         */
        char line[4096];
        while (fgets(line, sizeof(line), stdin)) {
            size_t len = strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
                line[--len] = '\0';
            pipe_write_line(hChildStdinWr, maybe_convert(line));
        }
    } else if (argc > 1) {
        /*
         * CLI mode: send all argv as stdin lines to pipp_core_orig,
         * converting RAW file paths along the way.
         */
        for (int i = 1; i < argc; i++) {
            pipe_write_line(hChildStdinWr, maybe_convert(argv[i]));
        }
    }

    /* Close the write end so pipp_core_orig gets EOF on stdin */
    CloseHandle(hChildStdinWr);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    cleanup();
    return (int)exit_code;
}
