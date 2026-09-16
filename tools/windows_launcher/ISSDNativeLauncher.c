#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
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

static bool directory_exists(const char *path) {
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
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

static bool read_config_value(const char *cfg_path, const char *key, char *out, size_t out_size) {
    if (!out || out_size == 0) return false;
    out[0] = '\0';
    FILE *f = fopen(cfg_path, "r");
    if (!f) return false;
    char line[MAX_PATH_TEXT];
    bool found = false;
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        char *s = trim(line);
        if (strncmp(s, key, key_len) != 0 || s[key_len] != '=') continue;
        s = trim(s + key_len + 1);
        snprintf(out, out_size, "%s", s);
        found = out[0] != '\0';
        break;
    }
    fclose(f);
    return found;
}

static bool write_launcher_paths(const char *cfg_path, const char *rom_path, const char *mods_dir) {
    FILE *f = fopen(cfg_path, "w");
    if (!f) return false;
    fprintf(f, "# ISSD Native launcher configuration\n");
    fprintf(f, "# ROM files are not distributed with this project.\n");
    fprintf(f, "rom_path=%s\n", rom_path ? rom_path : "");
    fprintf(f, "mods_dir=%s\n", mods_dir ? mods_dir : "mods");
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

static bool pick_mods_folder(char *out, size_t out_size) {
    char path[MAX_PATH_TEXT] = "";
    BROWSEINFOA bi;
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = NULL;
    bi.pszDisplayName = path;
    bi.lpszTitle = "Select your ISSD Native mods folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (!pidl) return false;
    bool ok = SHGetPathFromIDListA(pidl, path) && path[0] != '\0';
    CoTaskMemFree(pidl);
    if (!ok) return false;
    snprintf(out, out_size, "%s", path);
    return true;
}

static int launch_core(const char *core_path, const char *work_dir, int argc, char **argv, const char *rom_path, const char *mods_dir) {
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
        if (mods_dir && mods_dir[0]) {
            strncat(cmd, " --mods-dir ", sizeof(cmd) - strlen(cmd) - 1);
            append_quoted(cmd, sizeof(cmd), mods_dir);
        }
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
    char default_mods_dir[MAX_PATH_TEXT];
    join_path(core_path, sizeof(core_path), exe_dir, CORE_NAME);
    join_path(cfg_path, sizeof(cfg_path), exe_dir, CFG_NAME);
    join_path(default_rom_path, sizeof(default_rom_path), exe_dir, "International Superstar Soccer Deluxe (USA).sfc");
    join_path(default_mods_dir, sizeof(default_mods_dir), exe_dir, "mods");

    if (argc > 1 && strcmp(argv[1], "--launcher-self-test") == 0) {
        char got_rom[MAX_PATH_TEXT] = "";
        char got_mods[MAX_PATH_TEXT] = "";
        if (argc != 3) return 11;
        if (!write_launcher_paths(argv[2], "C:\\Games\\issd.sfc", "D:\\ISSD Mods")) return 12;
        if (!read_config_value(argv[2], "rom_path", got_rom, sizeof(got_rom))) return 13;
        if (!read_config_value(argv[2], "mods_dir", got_mods, sizeof(got_mods))) return 14;
        if (strcmp(got_rom, "C:\\Games\\issd.sfc") != 0) return 15;
        return strcmp(got_mods, "D:\\ISSD Mods") == 0 ? 0 : 16;
    }

    if (!file_exists(core_path)) {
        MessageBoxA(NULL, "ISSDNativeCore.exe was not found next to ISSDNative.exe.", "ISSD Native", MB_ICONERROR | MB_OK);
        return 1;
    }

    char rom_path[MAX_PATH_TEXT] = "";
    char mods_dir[MAX_PATH_TEXT] = "";
    if (argc == 1) {
        if (!read_config_value(cfg_path, "rom_path", rom_path, sizeof(rom_path)) || !file_exists(rom_path)) {
            if (file_exists(default_rom_path)) {
                snprintf(rom_path, sizeof(rom_path), "%s", default_rom_path);
            } else if (!pick_rom(rom_path, sizeof(rom_path))) {
                return 1;
            }
        }
        if (!read_config_value(cfg_path, "mods_dir", mods_dir, sizeof(mods_dir)) || !directory_exists(mods_dir)) {
            if (directory_exists(default_mods_dir)) {
                snprintf(mods_dir, sizeof(mods_dir), "%s", default_mods_dir);
            } else if (MessageBoxA(NULL,
                                   "No mods folder was found next to ISSDNative.exe. Would you like to select one now?",
                                   "ISSD Native", MB_ICONQUESTION | MB_YESNO) == IDYES &&
                       !pick_mods_folder(mods_dir, sizeof(mods_dir))) {
                return 1;
            }
        }
        write_launcher_paths(cfg_path, rom_path, mods_dir[0] ? mods_dir : "mods");
    }

    return launch_core(core_path, argc > 1 ? NULL : exe_dir, argc, argv, rom_path, mods_dir);
}
