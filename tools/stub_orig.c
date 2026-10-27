/* Test stub for pipp_core_orig.exe: logs each stdin line and, if it is a file, its size and TIFF magic. */
#include <stdio.h>
#include <string.h>

int main(void) {
    char line[4096];
    FILE *log = fopen("stub_log.txt", "a");
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\r\n")] = '\0';
        FILE *f = fopen(line, "rb");
        if (f) {
            unsigned char m[4] = {0};
            fread(m, 1, 4, f);
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fclose(f);
            fprintf(log, "%s | %ld bytes | magic %02x%02x%02x%02x\n", line, sz, m[0], m[1], m[2], m[3]);
        } else {
            fprintf(log, "%s | (not a file)\n", line);
        }
    }
    fclose(log);
    return 0;
}
