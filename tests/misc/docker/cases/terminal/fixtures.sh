#!/bin/sh
# Build the terminal cases under $1.
#
# What the embedded terminal got in this milestone: a keybar of its own, the
# panels back on Ctrl-F1 from the command line, Ctrl-Alt-L clearing the
# scrollback along with the screen, and insert mode so that editing in the
# middle of a shell line draws right.
set -e

dir="${1:-/home/mc/cases/terminal}"
rm -rf "$dir"
mkdir -p "$dir"
cd "$dir"

mkdir -p 01-shell
printf 'a file for the cursor to stand on\n' > 01-shell/readme.txt

cat > 01-shell/cases.tsv <<'EOF'
file	key	expect	why	transports
readme.txt	key C-o,type echo mc-marker-42,key Enter	text: mc-marker-42	a command runs in the terminal	local
readme.txt	key C-o,type echo mc-marker-42,key Enter,key C-M-l	no text: mc-marker-42	Ctrl-Alt-L takes the scrollback with the screen	local
readme.txt	key C-o,type echo mc-marker-42,key Enter,key C-l	no text: mc-marker-42	Ctrl-L scrolls the screen into the history	local
readme.txt	key C-o,type echo mc-marker-42,key Enter,key C-l,key PageUp	text: mc-marker-42	and the history still has it	local
readme.txt	key C-o	text: 6ClrAll	the terminal puts up a keybar of its own	local
readme.txt	key C-o,type 11111,key Home,type 222	text: 22211111	insert mode: the line is not overwritten	local
readme.txt	key C-o,key C-F1	text: cases.tsv	Ctrl-F1 at the command line brings the panels back; the sequence this terminal sends is in the learned keys	local
readme.txt	key C-o,type printf 'HEAD%.0s-' $(seq 1 40); echo TAILMARK,key Enter,width 60	text: TAILMARK	a line longer than the screen is laid out again when the terminal narrows, and keeps its tail	local
readme.txt	type echo RANFROMPANEL,key Enter	text: cases.tsv	a command run from the panels brings them back on its own; the option says at once in a real terminal	local
readme.txt	type echo RANFROMPANEL,key Enter	no text: Press any key	and with the option left alone there is no pause to press through	local
readme.txt	key C-o	with Pause after run set to Always, the prompt row says Press any key to continue	for a person: the option is per run, not per case (test -o pause_after_run=2)	local
readme.txt	key C-o,type echo COPYMARK,key Enter,key F3,key F2	clipfile: COPYMARK	F3 marks the whole output and F2 puts it in the clipfile	local
readme.txt	key C-o,type echo COPYMARK,key Enter,key F3,key C-Insert	clipfile: COPYMARK	Ctrl-Insert stores it as well, which is what the keymap binds	local
readme.txt	key C-o	a mark made with Shift-arrows is stored the same way	for a person: pressed from a script it leaves the clipfile empty, so what it needs is not the key alone	local
readme.txt	key C-o	the output marked with the mouse lands in the clipfile the same way	for a person: the mouse is not something this script presses	local
EOF

echo "terminal cases in $dir"
