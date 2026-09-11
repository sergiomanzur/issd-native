#include "issd_script.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ENTRIES 512

typedef struct {
    int      frame;
    uint32_t mask;
} Entry;

static Entry s_entries[MAX_ENTRIES];
static int   s_count;

static const struct { const char *name; uint32_t bit; } kButtons[] = {
    { "SELECT", ISSD_BTN_SELECT },   /* longer names first so a prefix   */
    { "START",  ISSD_BTN_START  },   /* match cannot shadow them         */
    { "RIGHT",  ISSD_BTN_RIGHT  },
    { "DOWN",   ISSD_BTN_DOWN   },
    { "LEFT",   ISSD_BTN_LEFT   },
    { "NONE",   0u              },
    { "UP",     ISSD_BTN_UP     },
    { "A",      ISSD_BTN_A      },
    { "B",      ISSD_BTN_B      },
    { "X",      ISSD_BTN_X      },
    { "Y",      ISSD_BTN_Y      },
    { "L",      ISSD_BTN_L      },
    { "R",      ISSD_BTN_R      },
};

static uint32_t parse_mask(char *names) {
    uint32_t mask = 0;
    for (char *tok = strtok(names, ","); tok; tok = strtok(NULL, ",")) {
        while (*tok == ' ' || *tok == '\t') tok++;
        size_t n = strlen(tok);
        while (n && (tok[n-1] == ' ' || tok[n-1] == '\r' || tok[n-1] == '\n')) tok[--n] = 0;
        for (unsigned i = 0; i < sizeof(kButtons)/sizeof(kButtons[0]); i++) {
            if (strcmp(tok, kButtons[i].name) == 0) { mask |= kButtons[i].bit; break; }
        }
    }
    return mask;
}

int issd_script_load(const char *path) {
    s_count = 0;
    if (!path || !path[0]) return 0;

    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "[script] cannot open '%s'\n", path);
        return 0;
    }

    char line[256];
    int lineno = 0;
    while (fgets(line, sizeof(line), f) && s_count < MAX_ENTRIES) {
        lineno++;
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == 0) continue;

        char *end = NULL;
        long frame = strtol(p, &end, 10);
        if (end == p) {
            fprintf(stderr, "[script] %s:%d: expected a frame number\n", path, lineno);
            continue;
        }
        while (*end == ' ' || *end == '\t') end++;
        s_entries[s_count].frame = (int)frame;
        s_entries[s_count].mask  = parse_mask(end);
        s_count++;
    }
    fclose(f);

    /* Entries are searched linearly and the last match wins, so they have to
     * be in ascending frame order. Sorting here means a script file can be
     * written and appended to without worrying about ordering. */
    for (int i = 1; i < s_count; i++) {
        Entry e = s_entries[i];
        int j = i;
        while (j > 0 && s_entries[j-1].frame > e.frame) { s_entries[j] = s_entries[j-1]; j--; }
        s_entries[j] = e;
    }

    fprintf(stderr, "[script] loaded %d entries from %s\n", s_count, path);
    return s_count;
}

bool issd_script_active(void) { return s_count > 0; }

uint32_t issd_script_mask(uint32_t frame) {
    uint32_t mask = 0;
    for (int i = 0; i < s_count && s_entries[i].frame <= (int)frame; i++)
        mask = s_entries[i].mask;
    return mask;
}
