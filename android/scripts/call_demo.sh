#!/usr/bin/env bash
# Incoming-call test: a contact "Amma" (9876543210) calls three times during a
# demo ride against fake_rotos.py --reply-calls:
#   1. rejected from ROTOS (CALL_ACTION REJECT_REPLY) -> endCall + SMS -> IDLE REPLIED
#   2. answered on the phone                          -> ACTIVE -> IDLE
#   3. caller gives up                                -> IDLE MISSED
# Needs a running emulator (scripts/emulator.sh) and a built APK.
# Use --sim to skip fake_rotos (something else listens on 7400).
set -euo pipefail
source "$(dirname "$0")/common.sh"
NUM=9876543210
LOG="$OUT_DIR/call_demo.log"
E() { adb_emu shell "$@"; }

"$ANDROID_DIR/scripts/install.sh" >/dev/null
if ! E content query --uri "content://com.android.contacts/phone_lookup/$NUM" --projection display_name | grep -q Amma; then
    E content insert --uri content://com.android.contacts/raw_contacts --bind account_type:n: --bind account_name:n:
    RID=$(E content query --uri content://com.android.contacts/raw_contacts --projection _id | tail -1 | sed -E 's/.*_id=([0-9]+).*/\1/')
    E content insert --uri content://com.android.contacts/data --bind raw_contact_id:i:"$RID" \
        --bind mimetype:s:vnd.android.cursor.item/name --bind data1:s:Amma
    E content insert --uri content://com.android.contacts/data --bind raw_contact_id:i:"$RID" \
        --bind mimetype:s:vnd.android.cursor.item/phone_v2 --bind data1:s:"$NUM" --bind data2:i:2
fi

FAKE_PID=""
if [ "${1:-}" != "--sim" ]; then
    python3 -u "$ANDROID_DIR/tools/fake_rotos.py" --reply-calls --accept-timeout 300 > "$LOG" 2>&1 &
    FAKE_PID=$!
    sleep 1
fi
adb_emu shell am force-stop dev.rotos.app
adb_emu logcat -c
adb_emu shell am start -n dev.rotos.app/.ui.MainActivity --ez demo true --ez sample true --ez calls true \
    --es link tcp --es host 10.0.2.2 --ei port 7400 --ef speedup 1 --ez autostart true >/dev/null
for i in $(seq 1 120); do                              # wait for the ride (NAV_STATE flowing)
    adb_emu logcat -d -s ROTOS | grep -q "NAV seq=" && break
    sleep 1
done
sleep 5
echo "call 1: reject + reply from ROTOS"; adb_emu emu gsm call $NUM >/dev/null; sleep 3
"$ANDROID_DIR/scripts/screenshot.sh" call-ringing >/dev/null; sleep 8
echo "call 2: answered";                  adb_emu emu gsm call $NUM >/dev/null; sleep 2
adb_emu emu gsm accept $NUM >/dev/null; sleep 4; adb_emu emu gsm cancel $NUM >/dev/null; sleep 3
echo "call 3: missed";                    adb_emu emu gsm call $NUM >/dev/null; sleep 2
adb_emu emu gsm cancel $NUM >/dev/null; sleep 3
"$ANDROID_DIR/scripts/screenshot.sh" call-after >/dev/null
adb_emu logcat -d -s ROTOS | grep -iE "call|sms" > "$OUT_DIR/call_logcat.txt" || true
echo "--- app"; cat "$OUT_DIR/call_logcat.txt"
echo "--- content://sms/sent"; E content query --uri content://sms/sent --projection address:body
if [ -n "$FAKE_PID" ]; then
    kill -TERM "$FAKE_PID"; wait "$FAKE_PID" || true
    echo "--- fake ROTOS"; grep -E "CALL_|^call " "$LOG"
fi
