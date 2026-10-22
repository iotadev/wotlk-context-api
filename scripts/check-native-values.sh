#!/bin/sh
set -eu
project=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=${1:-"$project/.validation/native-values"}
mkdir -p "$build"
compiler=${CXX:-c++}
for check in cache-check json-check; do
    "$compiler" -std=c++20 -Wall -Wextra -Werror -pedantic \
        -I"$project/server-module/src" "$project/native-tests/$check.cpp" -o "$build/$check"
    "$build/$check" > "$build/$check-output.txt"
done
cat "$build/cache-check-output.txt"
echo "JSON locale, escaping, numeric and invalid-state checks passed"
