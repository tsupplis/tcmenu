#!/bin/sh
# Interactive menu that invokes a script for the selected action.
set -u
# Preserve the terminal output before command substitution captures stdout.
exec 4>&1
example_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tcmenu=${TCMENU_BIN:-"$example_dir/../tcmenu"}

# Keep curses on the terminal; capture only the numeric result on descriptor 3.
# A selection can have a nonzero exit status, so do not use `set -e` here.
selection=$("$tcmenu" --fd 3 --exit --title 'Choose a local action' \
    --entry 'Show system information' 'Show project files' \
    3>&1 1>&4)
status=$?
case "$selection" in
    0) exec sh "$example_dir/scripts/system-info.sh" ;;
    1) exec sh "$example_dir/scripts/list-project.sh" ;;
    -3) printf '%s\n' 'Cancelled.' ;;
    *) printf 'Menu failed (status %s, result %s).\n' "$status" "$selection" >&2
       [ "$status" -ne 0 ] || status=1
       exit "$status" ;;
esac
