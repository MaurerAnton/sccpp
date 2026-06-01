# sccpp - Sloc Cloc and Code (Pure C++ Port)

A zero-dependency (beyond libc++ and pthreads) port of
[scc](https://github.com/boyter/scc) — a fast and accurate source code line
counter with complexity estimation.

## Why sccpp?

The original [scc](https://github.com/boyter/scc) requires the Go toolchain
plus dozens of downloaded modules to build. sccpp compiles with a single
`make` command using only:

- **C++17** (GCC or Clang)
- **nlohmann/json** (vendored, single header, zero build dependency)
- **pthreads** (standard on all Unix-like systems)

No Go, no Python, no Rust, no npm, no Docker required.

## Features

- Counts lines of code, comments, blanks, and complexity
- Supports 300+ languages (same language database as scc v3.8)
- Multi-threaded file processing
- Language detection by extension, filename, and shebang
- Tabular output (wide and compact modes)
- Per-file output (`--by-file`)
- Exclude/include extensions and directories

## Quick Start

```bash
# Build
make

# Count lines in current directory
./sccpp

# Count lines in specific directory
./sccpp src/

# Wider output with more stats
./sccpp -w src/

# Per-file output
./sccpp --by-file src/

# List supported languages
./sccpp -l

# Output to file
./sccpp -o stats.txt ~/project
```

## Building

Requirements: C++17 compiler (GCC ≥8 or Clang ≥7), GNU Make.

```bash
make          # Release build
make debug    # Debug build with -g -O0
make clean    # Remove build artifacts
```

The resulting binary is a single ~1MB executable.

## Differences from scc

| Feature | sccpp | scc (Go) |
|---------|-------|----------|
| Language | C++17 | Go |
| Dependencies | nlohmann/json (vendored) | Go toolchain + many modules |
| Build | `make` (single binary) | `go build` + module download |
| Language DB | Embedded JSON (same as scc v3.8) | Compiled Go map |
| Output formats | tabular, wide | 12+ formats (tabular, json, csv, html, sql, ...) |
| COCOMO/LOCOMO | Not implemented | Built-in cost estimation |
| Git history | Not implemented | Hotspots, author timeline |
| Minified/Generated detection | Not implemented | Built-in |
| ULOC/Duplicates | Not implemented | Built-in |

sccpp focuses on the core line-counting functionality — fast, accurate,
and easy to build.

## Architecture

```
sccpp/
├── src/
│   ├── main.cpp        # CLI entry point, argument parsing
│   ├── common.hpp      # Data structures (Trie, FileJob, LanguageFeature)
│   ├── trie.cpp        # Trie implementation for fast token matching
│   ├── language.hpp/cpp # Language database (from embedded JSON)
│   ├── detector.hpp/cpp # Language detection by extension/filename/shebang
│   ├── counter.hpp/cpp  # Core state machine for line counting
│   ├── walker.hpp/cpp   # File system traversal
│   └── formatter.hpp/cpp # Tabular output formatting
├── third_party/
│   └── nlohmann/
│       └── json.hpp    # Vendored JSON for Modern C++ (v3.12.0)
├── languages.json       # Language definitions (from scc)
├── Makefile
└── LICENSE              # GNU GPLv3
```

## License

GNU General Public License v3.0.

Copyright (C) 2025 Anton Maurer
