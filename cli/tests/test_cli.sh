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
failures=0

cleanup() {
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

"$nabu" > /dev/null 2>&1
expect "$?" 2 "no arguments"
"$nabu" bogus > /dev/null 2>&1
expect "$?" 2 "unknown command"
"$nabu" list extra > /dev/null 2>&1
expect "$?" 2 "extra argument"

[ "$failures" -eq 0 ]
