# Runnable shell examples

Build the binary with `make` from the project root, then run:

```sh
./examples/menu-actions.sh
```

Choose an action with the arrow keys and Enter, or press X to cancel. Each
selection invokes a separate script from `examples/scripts/`. These scripts
only print system information or list the project files.

The second example runs choice **1** (list project files) without a terminal:

```sh
./examples/default-action.sh
```

To preselect that same default and let the user confirm or change it:

```sh
./examples/default-action.sh --interactive
```

Interactive examples need a terminal. You can launch these scripts from any
working directory. Set `TCMENU_BIN` to use a different built binary.

## Capturing results

Selections are zero-based. The CLI prints the selected index and also returns
it as its exit status, so a nonzero status can be a valid selection. These
examples deliberately avoid `set -e` and dispatch on the printed number.

`--fd 3` sends the numeric result to descriptor 3. Inside command substitution,
`3>&1` captures that result while `1>&4` keeps curses output on the original
stdout, saved earlier with `exec 4>&1`. Run interactive examples with stdout
connected to the terminal.
X prints `-3` (shell exit status 253). Signals exit with `128 + signal number`
and print no selection. All options must come before `--entry`.

To add your own action, add a label after `--entry` and a matching numeric
`case` branch that invokes its script. Keep script paths quoted.
