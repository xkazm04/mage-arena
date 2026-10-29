#!/bin/sh
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock
until grep -q BATCHH_DONE $G/batchH.log; do sleep 10; done
while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 2; done
echo "Tier B (chalk hero) round 4c - taken $(date '+%Y-%m-%d %H:%M:%S') (BiRefNet mattes, 3 clips)" > $L
cd $G
for c in Strafe Ward Sunfall; do t0=$(date +%s); python -u matte.py brennic/v2v/$c brennic/matte/$c 2>&1 | tail -1; echo "$c matte wall $(( $(date +%s) - t0 )) s"; done
rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B (round 4c mattes)" >> $G/lock-log.txt
echo BATCHI_DONE
