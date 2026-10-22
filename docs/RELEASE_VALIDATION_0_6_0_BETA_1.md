# 0.6.0-beta.1 validation

- Windows Release CMake/Ninja build: passed.
- Linux x86-64 CMake/Ninja build under WSL Ubuntu 24.04: passed.
- Android Gradle release: arm64-v8a and x86_64 passed; release metadata rebuilt
  for 0.6.0-beta.1 / versionCode 11.
- Feature checks: six suites passed (match policies/config, hook integration,
  all-team ratings, launcher model/ROM validation and AppRun writable-state paths).
- Integration checks: ten passed (real 1/20-minute Exhibition initialization,
  headless gameplay, bug-fix integration, config and snapshot compatibility).
- Additional save-context check passed for both optional rules and duration.
- Independent source review completed; findings corrected and retested.
- AppImage executable/SDL/libdecor glibc requirements <=2.34; samplerate <=2.14,
  Xss <=2.4. Packager rejects dependencies exceeding glibc 2.36.
- Generated AppImage completed a 900-frame no-FUSE smoke test with clean exit.

Release assembly uses an explicit public file list and produces SHA256SUMS.
Release archive integrity, packaged startup, APK identity/signing and uploaded
asset checksums are checked before publication.

Physical Steam Deck Gaming Mode/suspend/resume, Android document providers and
touch, four physical controllers and complete Cup/World Series playthroughs
remain pending. Automated headless checks do not establish device acceptance.

Publication checks: merged-master focused run 14 passed; release packaging/AppRun
run 9 passed. Packaged Windows completed a 900-frame dummy-driver run. Rebuilt
frozen Mod Studio selftest passed with both authored stadium profiles and 12/12
captures. Android APK signature verified and matches beta.4 certificate SHA-256
4259e5033f3128b6cce50ba4fe9f43d966af9add3ddcc4d2e9fdc5baff390fa0.
Windows checkout line-ending expansion in binary stadium assets was detected
by release validation and fixed with an explicit Git binary attribute.
AppImage stages tracked public resources, including binary stadium resources.
