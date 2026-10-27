/*
 * pipp_raw2tiff.exe - Thin wrapper that invokes pipp_raw2tiff.py
 * Locates Python automatically and passes all arguments through.
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int find_python(char *out, int maxlen) {
    /* Try common Python locations */
    const char *candidates[] = {
        "python.exe",
        "python3.exe",
        NULL
    };

    for (int i = 0; candidates[i]; i++) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "where %s > NUL 2>&1", candidates[i]);
        if (system(cmd) == 0) {
            strncpy(out, candidates[i], maxlen);
            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    char exe_dir[MAX_PATH];
    char script_path[MAX_PATH];
    char python[MAX_PATH];
    char cmdline[MAX_PATH * 4];

    /* Get directory of this exe */
    GetModuleFileNameA(NULL, exe_dir, MAX_PATH);
    char *sep = strrchr(exe_dir, '\\');
    if (sep) *(sep + 1) = '\0';

    snprintf(script_path, MAX_PATH, "%spipp_raw2tiff.py", exe_dir);

    if (GetFileAttributesA(script_path) == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "Cannot find %s\n", script_path);
        return 1;
    }

    if (!find_python(python, MAX_PATH)) {
        fprintf(stderr, "Cannot find Python\n");
        return 1;
    }

    /* Build command line: python -I script.py <original args> */
    int pos = snprintf(cmdline, sizeof(cmdline), "\"%s\" -I \"%s\"", python, script_path);
    for (int i = 1; i < argc && pos < (int)sizeof(cmdline) - 2; i++) {
        pos += snprintf(cmdline + pos, sizeof(cmdline) - pos, " \"%s\"", argv[i]);
    }

    /* Execute and return the exit code */
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE,
                        0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "Failed to launch Python: %lu\n", GetLastError());
        return 1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (int)exit_code;
}
