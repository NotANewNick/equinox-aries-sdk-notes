#!/bin/bash
# Runs equinox_diag against the simulated chip in fake_aries.c. This verifies
# software behaviour only. It says nothing about real hardware or real signals.
B=$(cd "$1" && pwd); T=$(mktemp -d); LOG=$T/bus.log
TOOL=$B/equinox_diag; FAKE=$B/fake_aries.so
pass=0; failed=0

ok()  { pass=$((pass+1)); echo "ok    $1"; }
bad() { failed=$((failed+1)); echo "FAIL  $1"; cat "$T/out" | sed 's/^/      /'; }

# run <tool> [ENV=VAL...] -- args. Output in $T/out, exit code in $code.
run() {
    local tool=$1; shift; local e=(); while [ "$1" != "--" ]; do e+=("$1"); shift; done; shift
    rm -f "$LOG"
    env LD_PRELOAD="$FAKE" FAKE_LOG="$LOG" "${e[@]}" timeout 120 "$tool" "$@" >"$T/out" 2>&1
    code=$?
}
expect() { # name wanted_code wanted_text
    if [ "$code" = "$2" ] && { [ -z "$3" ] || grep -q -- "$3" "$T/out"; }; then ok "$1"
    else echo "      exit $code, wanted $2"; bad "$1"; fi
}
no_arp() {
    if [ -f "$LOG" ] && grep -q 'addr=61' "$LOG"; then bad "$1: reached 0x61"; else ok "$1: no ARP traffic"; fi
}
# Audits every write, decoding mailbox commands so that a wide register write
# cannot hide behind the mailbox's address. Allowed: address selection, the read
# handshakes, and the SDK's eFuse read procedure (0x8ec wide, 0x8f6 direct).
no_settings_changed() {
    if python3 -I -c '
import sys
bad=[]; n=0
for l in open(sys.argv[1]):
    p=l.split()
    if p[0]=="wide":
        n+=1
        if p[1]!="dst=008ec": bad.append(l)
        continue
    if p[0]!="wr" or p[2]!="cmd=0f": continue
    b=[int(x,16) for x in p[3:]]
    a=(b[0]&1)<<16|b[1]<<8|b[2]; d=b[3:]
    mailbox=0xd99<=a<=0xda6
    path=a>=0x4200 and (a&0xfff) in (0x200,0x201,0x202)
    if a==0xd9d and d[0] not in (0x06,0x07,0x16,0x1d,0x1e,0x1f,0x20,0x24): bad.append(l)
    elif not (mailbox or path or a==0x8f6): bad.append(l)
for l in bad: print("unexpected:",l.strip())
sys.exit(1 if bad else 0)
' "$LOG" >"$T/out"; then ok "$1: only reads and the eFuse read procedure"; else bad "$1: wrote something else"; fi
}

BA="--bus 3 --addr 0x24"

# Argument checks
run $TOOL -- --help;                    expect "help" 0 "Usage"
run $TOOL -- info;                      expect "bus required" 2 "--bus is required"
run $TOOL -- --bus 3 info;              expect "addr required" 2 "--addr must"
run $TOOL -- --bus 3 --addr 0x61 info;  expect "ARP address rejected" 2 "--addr must"
run $TOOL -- --bus 3 --addr 0x50 info;  expect "address out of range" 2 "--addr must"
run $TOOL -- --bus 3 --addr zz info;    expect "bad address" 2 "--addr must"
run $TOOL -- --bus x --addr 0x24 info;  expect "bad bus" 2 "--bus is required"
run $TOOL -- $BA bogus;                 expect "unknown command" 2 "Usage"
run $TOOL -- $BA --nope info;           expect "unknown option" 2 "Unknown option"
run $TOOL -- $BA --port sideways eye;   expect "bad port" 2 "--port must"
run $TOOL -- $BA --dwell 99 --confirm eye; expect "bad dwell" 2 "--dwell must"

# Safety
run $TOOL -- $BA eye;                   expect "eye needs --confirm" 2 "--confirm"
[ -f "$LOG" ] && bad "eye without --confirm touched the bus" || ok "eye without --confirm: no bus traffic"
run $TOOL FAKE_ADDR=0x25 -- $BA info;   expect "wrong address" 1 "No response"; no_arp "wrong address"
run $TOOL -- --bus 9 --addr 0x24 info;  expect "missing bus" 1 "Cannot open"
run $TOOL FAKE_BUSY=1 -- $BA info;      expect "address busy" 1 "pt5161l driver"
run $TOOL FAKE_NO_BLOCK=1 -- $BA info;  expect "no block read" 1 "NO_BLOCK_READ"
run $TOOL FAKE_NO_HEARTBEAT=1 -- $BA info; expect "dead firmware" 1 "Firmware is not running"; no_arp "dead firmware"

# Wire format, compared with the Linux pt5161l driver
run $TOOL -- $BA info
expect "info" 0 "Firmware:      2.14.7"
expect "info ids" 0 "Vendor ID:     0xabcd"
expect "info health" 0 "Device health: OK"
[ "$(head -1 "$LOG")" = "wr addr=24 cmd=0a 00 06 05" ] && ok "read setup matches kernel driver" || bad "read setup bytes"
sed -n 2p "$LOG" | grep -q '^rd addr=24 cmd=09 0e' && ok "block read matches kernel driver" || bad "block read"
grep -q 'cmd=0f 4[0-9a-f] ' "$LOG" && ok "write framing matches kernel driver" || bad "write framing"
no_arp "info"; no_settings_changed "info"

# Links
run $TOOL -- $BA links
expect "links state" 0 "Link 0: FORWARDING (L0), Gen5, x16 of x16"
expect "links recoveries" 0 "Recoveries: 3"
expect "links FoM" 0 "Minimum FoM: 0xa0"
no_arp "links"; no_settings_changed "links"
run $TOOL FAKE_LINK_DOWN=1 -- $BA links; expect "link down" 1 "PROBLEM"

run $TOOL FAKE_TEMP_NOT_READY=1 -- $BA links; expect "temperature not ready" 1 "not ready"

# Eye. The simulator has no signal, so this checks the path and the CSV only.
run $TOOL -- $BA --out "$T/m" --dwell 0 --confirm eye
expect "eye upstream" 0 "m_0.csv"
[ "$(wc -l <"$T/m_0.csv")" = 17 ] && ok "eye CSV has 16 lanes" || bad "eye CSV size"
run $TOOL -- $BA --port down --out "$T/m" --dwell 0 --confirm eye
expect "eye downstream" 0 "m_1.csv"
run $TOOL FAKE_LINK_DOWN=1 -- $BA --out "$T/x" --dwell 0 --confirm eye
expect "eye refuses dead link" 1 "Not measuring"
[ -e "$T/x_0.csv" ] && bad "eye wrote a CSV for a dead link" || ok "eye dead link: no CSV"

# Adapters without SMBus block reads
run $B/noblock/equinox_diag FAKE_NO_BLOCK=1 -- $BA info
expect "NO_BLOCK_READ build" 0 "Firmware:      2.14.7"

rm -rf "$T"
echo; echo "$pass passed, $failed failed"
[ "$failed" = 0 ]
