# Cppcheck

Bayou's static-analysis entry point is:

```bat
cppcheck.bat
```

The script analyzes the repository-owned C and C++ translation units under
`src/`, including explicit `.cppm` module units. It enables Cppcheck's warning,
performance, and portability checks with the Windows x64 and C++20 settings
used by Bayou. Third-party sources downloaded into `build/` are not analyzed.

Cppcheck 2.21 does not fully parse four legal `export namespace` declarations
in Bayou's module units. Those exact parser errors are documented in the
suppression file; the normal MSVC build remains authoritative for module syntax.
The other module units are analyzed normally.

Cppcheck 2.21 or newer is recommended. The script searches `PATH`, the standard
per-user installation directory, and the standard Program Files directories.
Set `CPPCHECK_EXE` to an explicit executable path when Cppcheck is installed
elsewhere.

Any unsuppressed finding makes the command fail. Existing reviewed findings are
listed narrowly in `cppcheck-suppressions.txt`; do not add broad, ID-only
suppressions. Generated reports and the incremental analysis cache are written
under `output/cppcheck/`:

- `cppcheck.xml` contains the complete machine-readable report.
- `cppcheck-summary.txt` contains the console-friendly finding summary.
- `cache/` stores incremental whole-program analysis data.

Install the official Windows build from the
[Cppcheck releases](https://github.com/cppcheck-opensource/cppcheck/releases).
