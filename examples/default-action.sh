#!/bin/sh
# Run a default action unattended, or confirm/change it with --interactive.
set -u
# Preserve the terminal output before command substitution captures stdout.
exec 4>&1
example_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tcmenu=${TCMENU_BIN:-"$example_dir/../tcmenu"}

case "${1:-}" in
    '') set -- ;;
    --interactive) set -- --interactive --exit ;;
    *) printf 'Usage: %s [--interactive]\n' "$0" >&2; exit 2 ;;
esac

# Choice 1 is the default. --interactive preselects it in the menu.
if [ "$#" -gt 0 ]; then
    selection=$("$tcmenu" --fd 3 --default 1 "$@" --title 'Run a local action' \
        --entry 'Show system information' 'Show project files' \
        3>&1 1>&4)
else
    selection=$("$tcmenu" --fd 3 --default 1 \
        --entry 'Show system information' 'Show project files' 3>&1)
fi
status=$?
case "$selection" in
    0) exec sh "$example_dir/scripts/system-info.sh" ;;
    1) exec sh "$example_dir/scripts/list-project.sh" ;;
    -3) printf '%s\n' 'Cancelled.' ;;
    *) printf 'Menu failed (status %s, result %s).\n' "$status" "$selection" >&2
       [ "$status" -ne 0 ] || status=1
       exit "$status" ;;
esac
