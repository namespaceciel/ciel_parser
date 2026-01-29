#!/bin/bash

echo "Checking if clang-format is installed..."

clang-format --version || { echo "clang-format is not installed." && exit 1; }

REPO_ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CLANG_FORMAT_FILE="${REPO_ROOT_DIR}/.clang-format"

if [ ! -f "${CLANG_FORMAT_FILE}" ]; then
    echo ".clang-format not found in the repository root."
    exit 1
fi

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 path..."
    exit 1
fi

for each_path in "$@"; do
    PATHS="${PATHS} ${each_path}"
done

echo "Formatting..."

find ${PATHS} -type f \( -name "*.cpp" -or -name "*.c" -or -name "*.cc" -or -name "*.h" -or -name "*.hpp" \) -exec clang-format -i --style=file '{}' \;

echo "All header and source files have been formatted."
