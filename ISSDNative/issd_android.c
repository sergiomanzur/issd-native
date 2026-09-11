/* Android-only glue: storage locations and ROM discovery.
 *
 * Kept out of main.c so the desktop hosts stay free of Android conditionals.
 * The whole file compiles to nothing anywhere else.
 */
#ifdef ISSD_ANDROID

#include "issd_android.h"
#include <jni.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

static char s_external[1024];
static char s_internal[1024];

/* Called by ISSDActivity before SDL_main starts, so the paths are in place
 * before anything tries to read config. */
JNIEXPORT void JNICALL
Java_com_issdnative_ISSDActivity_nativeSetPaths(JNIEnv *env, jclass cls,
                                                jstring external, jstring internal) {
    (void)cls;
    const char *e = (*env)->GetStringUTFChars(env, external, NULL);
    if (e) {
        snprintf(s_external, sizeof(s_external), "%s", e);
        (*env)->ReleaseStringUTFChars(env, external, e);
    }
    const char *i = (*env)->GetStringUTFChars(env, internal, NULL);
    if (i) {
        snprintf(s_internal, sizeof(s_internal), "%s", i);
        (*env)->ReleaseStringUTFChars(env, internal, i);
    }
}

const char *issd_android_external_dir(void) { return s_external; }
const char *issd_android_internal_dir(void) { return s_internal; }

/* Android has no file picker we can call from C, and no meaningful working
 * directory. Instead of failing with "no ROM", scan the app's own external
 * directory - the folder a user can reach over USB - for a cartridge image.
 * Deliberately does not verify the dump here; the existing ROM verification
 * path already reports a bad image with a useful message. */
bool issd_android_find_rom(char *out, size_t out_size) {
    if (!out || out_size == 0 || !s_external[0]) return false;
    out[0] = '\0';

    DIR *dir = opendir(s_external);
    if (!dir) return false;

    bool found = false;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        const size_t len = strlen(name);
        if (len < 5) continue;
        const char *ext = name + len - 4;
        if (strcasecmp(ext, ".sfc") != 0 && strcasecmp(ext, ".smc") != 0) continue;

        snprintf(out, out_size, "%s/%s", s_external, name);
        found = true;
        break;
    }
    closedir(dir);
    return found;
}

#endif /* ISSD_ANDROID */
