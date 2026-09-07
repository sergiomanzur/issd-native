#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tier2_capture.h"

#define CHECK(cond, ...) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: "); fprintf(stderr, __VA_ARGS__); \
                   fputc('\n', stderr); return 1; } \
} while (0)

int main(void) {
    const char *manifest = "tier2_capture_contract.json";
    const char *journal = "tier2_capture_contract.jsonl";
    remove(manifest);
    remove(journal);

#ifdef _WIN32
    _putenv_s("SNESRECOMP_TIER2_MANIFEST", manifest);
    _putenv_s("SNESRECOMP_TIER2_JOURNAL", journal);
    _putenv_s("SNESRECOMP_TIER2_CAPTURE", "");
    _putenv_s("SNESRECOMP_TIER2", "");
#else
    setenv("SNESRECOMP_TIER2_MANIFEST", manifest, 1);
    setenv("SNESRECOMP_TIER2_JOURNAL", journal, 1);
    unsetenv("SNESRECOMP_TIER2_CAPTURE");
    unsetenv("SNESRECOMP_TIER2");
#endif

    CHECK(strcmp(tier2_capture_manifest_path("QA Game"), manifest) == 0,
          "explicit manifest path changed");
    CHECK(strcmp(tier2_capture_journal_path("QA Game"), journal) == 0,
          "explicit journal path changed");
    CHECK(!tier2_capture_enabled(), "tier2 capture should default off");
    CHECK(tier2_capture_append_discovery(
              "QA Game", 0x807000, 0xC00000, "M1X1", "dispatch", 1, 1),
          "disabled capture should be a successful no-op");
    FILE *disabled = fopen(journal, "r");
    CHECK(disabled == NULL, "disabled capture created journal");

    tier2_capture_set_default_enabled(1);
    CHECK(tier2_capture_enabled(), "tier2 capture did not enable");

    FILE *seed = fopen(journal, "w");
    CHECK(seed != NULL, "cannot seed %s", journal);
    fputs("{\"sentinel\":true}\n", seed);
    CHECK(fclose(seed) == 0, "cannot close seed journal");

    CHECK(tier2_capture_append_discovery(
              "QA Game", 0x808000, 0xC01234, "M1X1", "call_gap", 1, 12),
          "first append failed");
    tier2_capture_close();              /* model a session boundary */
    CHECK(tier2_capture_append_discovery(
              "QA Game", 0x808100, 0xC05678, "M0X1", "bank_miss", 0, 34),
          "second-session append failed");
    CHECK(tier2_capture_append_discovery(
              "QA Game", 0x808200, 0xC09ABC, "M1X0", "dispatch", -1, 56),
          "pre-execution append failed");
    tier2_capture_close();

    FILE *in = fopen(journal, "r");
    CHECK(in != NULL, "cannot reopen journal");
    char line[1024];
    int lines = 0;
    int discoveries = 0;
    int pending = 0;
    while (fgets(line, sizeof line, in)) {
        lines++;
        size_t n = strlen(line);
        CHECK(n && line[n - 1] == '\n', "line %d is torn", lines);
        if (strstr(line, "snesrecomp tier2 discovery v1")) discoveries++;
        if (strstr(line, "\"outcome_pending\":true")) pending++;
    }
    fclose(in);
    CHECK(lines == 4, "append contract lost prior data: got %d lines", lines);
    CHECK(discoveries == 3, "expected 3 complete discoveries, got %d", discoveries);
    CHECK(pending == 1, "crash-window discovery was not marked pending");
    remove(journal);

    puts("tier2_capture_test: PASS");
    return 0;
}
