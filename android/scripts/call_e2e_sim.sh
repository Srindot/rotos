#!/usr/bin/env bash
# End-to-end call test: Android app (emulator) <-> real rotos_sim, buttons pressed on ROTOS.
# Needs a running emulator (android/scripts/emulator.sh --headless) and build/host built.
set -u
cd ~/projects/rotos
OUT=build/call_e2e; rm -rf $OUT; mkdir -p $OUT
NUM=9876543210
E() { docker exec rotos-emu adb shell "$@"; }
EMU() { docker exec rotos-emu adb emu "$@" >/dev/null; }

# contact Amma
RID=$(E content insert --uri content://com.android.contacts/raw_contacts --bind account_type:n: --bind account_name:n: >/dev/null; \
      E content query --uri content://com.android.contacts/raw_contacts --projection _id | tail -1 | sed -E 's/.*_id=([0-9]+).*/\1/')
E content insert --uri content://com.android.contacts/data --bind raw_contact_id:i:"$RID" --bind mimetype:s:vnd.android.cursor.item/name --bind data1:s:Amma
E content insert --uri content://com.android.contacts/data --bind raw_contact_id:i:"$RID" --bind mimetype:s:vnd.android.cursor.item/phone_v2 --bind data1:s:$NUM --bind data2:i:2
echo "contact: $(E content query --uri content://com.android.contacts/phone_lookup/$NUM --projection display_name)"

build/host/sim/rotos_sim --headless --listen 7400 --fresh --state-dir $OUT/state \
    --press B@44000,A@69000 --snap 42000,47000,52000,67500,70500,73500 --snap-dir $OUT --max-s 90 > $OUT/sim.log 2>&1 &
SIM=$!; T0=$(date +%s.%N)
at() { while (( $(echo "$(date +%s.%N) - $T0 < $1" | bc) )); do sleep 0.1; done; }

docker exec rotos-emu adb shell am force-stop dev.rotos.app; docker exec rotos-emu adb logcat -c
E am start -n dev.rotos.app/.ui.MainActivity --ez demo true --ez sample true --ez calls true \
    --es link tcp --es host 10.0.2.2 --ei port 7400 --ef speedup 1 --ez autostart true >/dev/null
at 40; echo "40s call 1"; EMU gsm call $NUM
at 55; echo "55s caller hangs up"; EMU gsm cancel $NUM
at 65; echo "65s call 2"; EMU gsm call $NUM
at 80; EMU gsm cancel $NUM 2>/dev/null
wait $SIM
echo "--- ROTOS (sim)"; grep -E "connected|map stored|ride started|call" $OUT/sim.log
echo "--- app"; docker exec rotos-emu adb logcat -d -s ROTOS | grep -iE "call|sms|hang" | cut -c32- | head -30
echo "--- sms sent"; E content query --uri content://sms/sent --projection address:body
