#!/bin/sh
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock
until grep -q BATCHG_DONE $G/batchG.log; do sleep 10; done
sleep 120
while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 2; done
echo "Tier B (chalk hero) round 4b - taken $(date '+%Y-%m-%d %H:%M:%S') (death fall: 2 keys + 2 H3 segments)" > $L
cd $G/brennic
[ -f deathtopple-s7.png ] || INIT=deathmid-s7.png DENOISE=0.85 W=768 H=768 python -u ../flux_key.py deathtopple 7 0.25 pose0-s7.png ../refs/brennic-master-384.png 2>&1 | tail -1
[ -f deathlying-s7.png ] || INIT=deathmid-s7.png DENOISE=0.95 W=768 H=768 python -u ../flux_key.py deathlying 7 0.25 pose0-s7.png ../refs/brennic-master-384.png 2>&1 | tail -1
cd $G
seg() { [ -d brennic/h3-$1 ] || PROMPT=h3fall python -u h3_fl2va.py brennic $1 $2 $3 22 768 768 13 2>&1 | tail -1; }
seg fall3 brennic/deathmid-s7.png brennic/deathtopple-s7.png
seg fall4 brennic/deathtopple-s7.png brennic/deathlying-s7.png
rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B (round 4b death fall)" >> $G/lock-log.txt
echo BATCHH_DONE
