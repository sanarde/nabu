#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -u

if [ -z "${NABU_TEST_BUS:-}" ]; then
    command -v dbus-run-session > /dev/null || exit 77
    NABU_TEST_BUS=1 exec dbus-run-session -- sh "$0" "$@"
fi

nabu=$1
nabud=$2
notify=$3
tmp=$(mktemp -d /tmp/nabu-test-XXXXXX)
daemon=
watcher=
failures=0

cleanup() {
    [ -n "$watcher" ] && kill "$watcher" 2>/dev/null
    [ -n "$daemon" ] && kill "$daemon" 2>/dev/null
    rm -rf "$tmp"
}
trap cleanup EXIT

expect() {
    if [ "$1" != "$2" ]; then
        echo "FAIL: $3: got '$1', expected '$2'" >&2
        failures=$((failures + 1))
    fi
}

mkdir -p "$tmp/run" "$tmp/state"
chmod 700 "$tmp/run"
export XDG_RUNTIME_DIR="$tmp/run"
export XDG_STATE_HOME="$tmp/state"
export TZ=UTC

"$nabu" list 2> "$tmp/err"
expect "$?" 1 "list without daemon"
expect "$(cat "$tmp/err")" "nabu: the daemon is not running" "message without daemon"

"$nabud" 2> /dev/null &
daemon=$!
i=0
while ! "$nabu" list > /dev/null 2>&1; do
    i=$((i + 1))
    [ "$i" -gt 100 ] && break
    sleep 0.05
done

expect "$("$nabu" list)" "" "empty list"
expect "$("$nabu" history)" "" "empty history"

"$notify" first 0 1 0 > /dev/null
"$notify" "second	line" 0 2 0 > /dev/null
expect "$("$nabu" list)" "1	normal	nabu-test	first
2	critical	nabu-test	second line" "list"

"$notify" gone 50 1 0 > /dev/null
sleep 1
history=$("$nabu" history)
expect "$(echo "$history" | wc -l)" 1 "history lines"
if ! echo "$history" | grep -Eq '^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}	expired	normal	nabu-test	gone$'; then
    echo "FAIL: history line: '$history'" >&2
    failures=$((failures + 1))
fi

"$nabu" clear
expect "$?" 0 "clear"
expect "$("$nabu" history)" "" "history after clear"

"$notify" third 0 1 0 > /dev/null
"$nabu" dismiss 1
expect "$?" 0 "dismiss"
expect "$("$nabu" list)" "2	critical	nabu-test	second line
4	normal	nabu-test	third" "list after dismiss"
"$nabu" dismiss 99
expect "$?" 0 "dismiss of an unknown id"
"$nabu" dismiss abc > /dev/null 2>&1
expect "$?" 1 "dismiss with a bad id"

"$nabu" invoke 4 missing 2> "$tmp/err"
expect "$?" 1 "invoke with an unknown action"
expect "$(cat "$tmp/err")" "nabu: No such notification or action" "invoke message"
"$nabu" invoke 4 default
expect "$?" 0 "invoke"
expect "$("$nabu" list)" "2	critical	nabu-test	second line" "list after invoke"
expect "$("$nabu" history | cut -f2,5)" "dismissed	first
dismissed	third" "history reasons"
"$nabu" clear

expect "$("$nabu" dnd)" off "dnd default"
"$nabu" dnd on
expect "$?" 0 "dnd on"
expect "$("$nabu" dnd status)" on "dnd status on"
"$notify" hidden 0 1 0 > /dev/null
expect "$("$nabu" list)" "2	critical	nabu-test	second line" "list under dnd"
expect "$("$nabu" history | cut -f2,5)" "undefined	hidden" "history under dnd"
"$nabu" dnd off
expect "$("$nabu" dnd)" off "dnd off"
"$nabu" dnd maybe > /dev/null 2>&1
expect "$?" 2 "dnd with a bad argument"
"$nabu" dismiss > /dev/null 2>&1
expect "$?" 2 "dismiss without an argument"

"$nabu" watch > "$tmp/watch" 2> /dev/null &
watcher=$!
i=0
while [ ! -s "$tmp/watch" ]; do
    "$nabu" dnd on
    "$nabu" dnd off
    i=$((i + 1))
    [ "$i" -gt 40 ] && break
done
"$nabu" dnd on
"$nabu" dnd off
id=$("$notify" watched 0 1 0)
"$nabu" invoke "$id" default
i=0
while ! grep -q "^closed	$id	dismissed$" "$tmp/watch"; do
    i=$((i + 1))
    [ "$i" -gt 60 ] && break
    sleep 0.05
done
if ! grep -q "^posted	$id	normal	nabu-test	watched$" "$tmp/watch"; then
    echo "FAIL: watch posted line: '$(cat "$tmp/watch")'" >&2
    failures=$((failures + 1))
fi
if ! grep -q "^closed	$id	dismissed$" "$tmp/watch"; then
    echo "FAIL: watch closed line: '$(cat "$tmp/watch")'" >&2
    failures=$((failures + 1))
fi
if ! grep -q "^dnd	on$" "$tmp/watch" || ! grep -q "^dnd	off$" "$tmp/watch"; then
    echo "FAIL: watch dnd lines: '$(cat "$tmp/watch")'" >&2
    failures=$((failures + 1))
fi
kill "$watcher" 2>/dev/null
watcher=

"$nabu" > /dev/null 2>&1
expect "$?" 2 "no arguments"
"$nabu" bogus > /dev/null 2>&1
expect "$?" 2 "unknown command"
"$nabu" list extra > /dev/null 2>&1
expect "$?" 2 "extra argument"

[ "$failures" -eq 0 ]
