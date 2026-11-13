#!/bin/sh
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd) || exit 1
printf 'Files in %s:\n' "$project_dir"
ls -1 "$project_dir"
