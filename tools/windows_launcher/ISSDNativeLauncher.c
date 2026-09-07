#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CFG_NAME "issd_native.cfg"
#define CORE_NAME "ISSDNativeCore.exe"
#define MAX_PATH_TEXT 4096

static bool file_exists(const char *path) {
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static void dirname_of(char *path) {
    char *slash = strrchr(path, '\\');
    char *fwd = strrchr(path, '/');
    if (!slash || (fwd && fwd > slash)) slash = fwd;
    if (slash) *slash = '\0';
}

static void join_path(char *out, size_t out_size, const char *dir, const char *name) {
    snprintf(out, out_size, "%s\\%s", dir, name);
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        *--end = '\0';
    }
    return s;
}

static bool read_rom_path(const char *cfg_path, char *out, size_t out_size) {
    FILE *f = fopen(cfg_path, "r");
    if (!f) return false;
    char line[MAX_PATH_TEXT];
    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        char *s = trim(line);
        if (strncmp(s, "rom_path=", 9) != 0) continue;
        s = trim(s + 9);
        snprintf(out, out_size, "%s", s);
        found = out[0] != '\0';
        break;
    }
    fclose(f);
    return found;
}

static bool write_rom_path(const char *cfg_path, const char *rom_path) {
    FILE *f = fopen(cfg_path, "w");
    if (!f) return false;
    fprintf(f, "# ISSD Native launcher configuration\n");
    fprintf(f, "# ROM files are not distributed with this project.\n");
    fprintf(f, "rom_path=%s\n", rom_path ? rom_path : "");
    fclose(f);
    return true;
}

static void append_quoted(char *cmd, size_t cmd_size, const char *arg) {
    strncat(cmd, "\"", cmd_size - strlen(cmd) - 1);
    for (const char *p = arg; *p && strlen(cmd) + 3 < cmd_size; p++) {
        if (*p == '"' || *p == '\\') strncat(cmd, "\\", cmd_size - strlen(cmd) - 1);
        char c[2] = {*p, '\0'};
        strncat(cmd, c, cmd_size - strlen(cmd) - 1);
    }
    strncat(cmd, "\"", cmd_size - strlen(cmd) - 1);
}

static bool pick_rom(char *out, size_t out_size) {
    char path[MAX_PATH_TEXT] = "";
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = path;
    ofn.nMaxFile = sizeof(path);
    ofn.lpstrTitle = "Select your International Superstar Soccer Deluxe SNES ROM";
    ofn.lpstrFilter =
        "SNES ROMs (*.sfc;*.smc)\0*.sfc;*.smc\0"
        "All files (*.*)\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameA(&ofn)) return false;
    snprintf(out, out_size, "%s", path);
    return true;
}

static int launch_core(const char *core_path, const char *work_dir, int argc, char **argv, const char *rom_path) {
    char cmd[32768] = "";
    append_quoted(cmd, sizeof(cmd), core_path);
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            strncat(cmd, " ", sizeof(cmd) - strlen(cmd) - 1);
            append_quoted(cmd, sizeof(cmd), argv[i]);
        }
    } else {
        strncat(cmd, " ", sizeof(cmd) - strlen(cmd) - 1);
        append_quoted(cmd, sizeof(cmd), rom_path);
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);

    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, work_dir, &si, &pi)) {
        MessageBoxA(NULL, "Failed to start ISSDNativeCore.exe.", "ISSD Native", MB_ICONERROR | MB_OK);
        return 1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)exit_code;
}

int main(int argc, char **argv) {
    char exe_path[MAX_PATH_TEXT];
    if (!GetModuleFileNameA(NULL, exe_path, sizeof(exe_path))) return 1;
    char exe_dir[MAX_PATH_TEXT];
    snprintf(exe_dir, sizeof(exe_dir), "%s", exe_path);
    dirname_of(exe_dir);

    char core_path[MAX_PATH_TEXT];
    char cfg_path[MAX_PATH_TEXT];
    char default_rom_path[MAX_PATH_TEXT];
    join_path(core_path, sizeof(core_path), exe_dir, CORE_NAME);
    join_path(cfg_path, sizeof(cfg_path), exe_dir, CFG_NAME);
    join_path(default_rom_path, sizeof(default_rom_path), exe_dir, "International Superstar Soccer Deluxe (USA).sfc");

    if (argc > 1 && strcmp(argv[1], "--launcher-self-test") == 0) {
        char got[MAX_PATH_TEXT] = "";
        if (argc != 3) return 11;
        if (!write_rom_path(argv[2], "C:\\Games\\issd.sfc")) return 12;
        if (!read_rom_path(argv[2], got, sizeof(got))) return 13;
        return strcmp(got, "C:\\Games\\issd.sfc") == 0 ? 0 : 14;
    }

    if (!file_exists(core_path)) {
        MessageBoxA(NULL, "ISSDNativeCore.exe was not found next to ISSDNative.exe.", "ISSD Native", MB_ICONERROR | MB_OK);
        return 1;
    }

    char rom_path[MAX_PATH_TEXT] = "";
    if (argc == 1) {
        if (!read_rom_path(cfg_path, rom_path, sizeof(rom_path)) || !file_exists(rom_path)) {
            if (file_exists(default_rom_path)) {
                snprintf(rom_path, sizeof(rom_path), "%s", default_rom_path);
            } else if (!pick_rom(rom_path, sizeof(rom_path))) {
                return 1;
            }
            write_rom_path(cfg_path, rom_path);
        }
    }

    return launch_core(core_path, argc > 1 ? NULL : exe_dir, argc, argv, rom_path);
}
