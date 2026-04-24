#!/usr/bin/env bash
set -ou pipefail

exit_code=0

for file in $(git grep --cached -Il '' | grep -E '\.(c|h|cpp|hpp)$'); do
  diff=$(clang-format "${file}" | diff -u "${file}" -)
  if [[ -n "${diff}" ]]; then
    exit_code=1
    echo "${diff}"
  fi
done

exit "${exit_code}"
