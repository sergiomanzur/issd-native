#ifndef _WIN32
#define _XOPEN_SOURCE 700
#endif
#include "issd_asset_path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

#define ASSET_PATH_SIZE 4096
static bool physical_path(const char *path, char output[ASSET_PATH_SIZE], bool directory) {
#ifdef _WIN32
    HANDLE handle = CreateFileA(path, FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
        directory ? FILE_FLAG_BACKUP_SEMANTICS : 0, NULL);
    if (handle == INVALID_HANDLE_VALUE) return false;
    BY_HANDLE_FILE_INFORMATION information;
    bool valid = GetFileInformationByHandle(handle, &information) != 0;
    if (valid) valid = ((information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) == directory;
    DWORD count = valid ? GetFinalPathNameByHandleA(handle, output, ASSET_PATH_SIZE,
                                                   FILE_NAME_NORMALIZED | VOLUME_NAME_DOS) : 0;
    CloseHandle(handle);
    return count != 0 && count < ASSET_PATH_SIZE;
#else
    char *resolved = realpath(path, NULL);
    if (!resolved) return false;
    struct stat information;
    bool valid = strlen(resolved) < ASSET_PATH_SIZE && stat(resolved, &information) == 0 &&
        (directory ? S_ISDIR(information.st_mode) : S_ISREG(information.st_mode));
    if (valid) strcpy(output, resolved);
    free(resolved);
    return valid;
#endif
}
bool issd_asset_resolve(const char *directory, const char *relative,
                        char *output, size_t output_size) {
    if (!directory || !relative || !*relative || !output || !output_size ||
        *relative == '/' || strchr(relative, '\\') || strchr(relative, ':')) return false;
    for (const char *part = relative; *part;) {
        const char *end = strchr(part, '/');
        size_t count = end ? (size_t)(end-part) : strlen(part);
        if (!count || (count == 2 && part[0] == '.' && part[1] == '.') ||
            (end && !end[1])) return false;
        part = end ? end+1 : part+count;
    }
    char root[ASSET_PATH_SIZE], joined[ASSET_PATH_SIZE], target[ASSET_PATH_SIZE];
    if (!physical_path(directory, root, true)) return false;
    size_t length = strlen(root);
    while (length && (root[length-1] == '/' || root[length-1] == '\\')) --length;
    int count = snprintf(joined, sizeof joined, "%.*s/%s", (int)length, root, relative);
#ifdef _WIN32
    /* Extended Win32 paths require backslash separators, including nested
     * resources whose portable manifest spelling uses forward slashes. */
    for (char *character = joined; *character; ++character)
        if (*character == '/') *character = '\\';
#endif
    if (count < 0 || (size_t)count >= sizeof joined || !physical_path(joined, target, false))
        return false;
    if (strlen(target) <= length || strlen(target) >= output_size) return false;
#ifdef _WIN32
    if (_strnicmp(root, target, length) || target[length] != '\\') return false;
#else
    if (strncmp(root, target, length) || target[length] != '/') return false;
#endif
    strcpy(output, target);
    return true;
}
