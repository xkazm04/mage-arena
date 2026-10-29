#!/bin/sh
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock
while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 2; done
echo "Tier B (chalk hero) round 4f - taken $(date '+%Y-%m-%d %H:%M:%S') (one BiRefNet matte)" > $L
cd $G; python -u matte.py brennic/v2v/Death-fall brennic/matte/DeathFall 2>&1 | tail -1
rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B (round 4f matte)" >> $G/lock-log.txt
echo BATCHL_DONE
