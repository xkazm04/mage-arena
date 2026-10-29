#!/bin/sh
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock
while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 2; done
echo "Tier B (chalk hero) round 4 - taken $(date '+%Y-%m-%d %H:%M:%S') (3 keys + 6 H3 segments, ~25 min)" > $L
cd $G/brennic
for k in stepkey wardkey sunkey; do [ -f $k-s7.png ] || INIT=pose0-s7.png DENOISE=0.9 W=768 H=768 python -u ../flux_key.py $k 7 0.25 pose0-s7.png ../refs/brennic-master-384.png 2>&1 | tail -1; done
cd $G
seg() { [ -d brennic/h3-$1 ] || PROMPT=h3seg python -u h3_fl2va.py brennic $1 $2 $3 22 768 768 13 2>&1 | tail -1; }
seg step1 brennic/pose0-s7.png brennic/stepkey-s7.png
seg step2 brennic/stepkey-s7.png brennic/pose0-s7.png
seg ward1 brennic/pose0-s7.png brennic/wardkey-s7.png
seg ward2 brennic/wardkey-s7.png brennic/pose0-s7.png
seg sun1 brennic/pose0-s7.png brennic/sunkey-s7.png
seg sun2 brennic/sunkey-s7.png brennic/castkey-s7.png
rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B (round 4 keys + segments)" >> $G/lock-log.txt
echo BATCHG_DONE
