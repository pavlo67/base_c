#!/usr/bin/env bash
set -euo pipefail

find_build_dir() {
    local found=""
    local dir

    for dir in cmake-build-release build; do
        if [[ -f "$dir/CMakeCache.txt" && -d "$dir/CMakeFiles" ]]; then
            if [[ -n "$found" ]]; then
                printf 'ERROR: multiple active CMake build directories found: %s, %s\n' "$found" "$dir"
                return 1
            fi
            found="$dir"
        fi
    done

    if [[ -z "$found" ]]; then
        printf 'ERROR: no active CMake build directory found (cmake-build-release/ or build/)\n'
        return 1
    fi

    printf '%s\n' "$found"
}

BUILD_DIR="$(find_build_dir)"
printf '[build] %s\n' "$BUILD_DIR"
cmake --build "$BUILD_DIR"
