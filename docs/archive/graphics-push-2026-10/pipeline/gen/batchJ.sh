#!/bin/sh
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock
until grep -q BATCHI_DONE $G/batchI.log; do sleep 10; done
sleep 90
while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 2; done
echo "Tier B (chalk hero) round 4d - taken $(date '+%Y-%m-%d %H:%M:%S') (death lying key retry + 1 H3 segment)" > $L
cd $G/brennic
for s in 7 19; do [ -f deathlying-s$s.png ] || INIT=deathtopple-s7.png DENOISE=0.9 W=768 H=768 python -u ../flux_key.py deathlying $s 0.25 pose0-s7.png ../refs/brennic-master-384.png 2>&1 | tail -1; done
rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B (round 4d key)" >> $G/lock-log.txt
echo BATCHJ_DONE
