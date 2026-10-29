#!/bin/sh
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock
sleep 60
while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 2; done
echo "Tier B (chalk hero) round 4e - taken $(date '+%Y-%m-%d %H:%M:%S') (lying key without refs + 1 H3 segment)" > $L
cd $G/brennic
for s in 23 31; do [ -f deathlying-s$s.png ] || NOREFS=1 INIT=deathtopple-s7.png DENOISE=0.82 W=768 H=768 python -u ../flux_key.py deathlying $s 0 2>&1 | tail -1; done
echo KEYS_DONE
rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B (round 4e keys)" >> $G/lock-log.txt
