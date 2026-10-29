#!/bin/sh
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock
while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 2; done
echo "Tier B (chalk hero) round 5 - taken $(date '+%Y-%m-%d %H:%M:%S') (death matte)" > $L
cd $G; python -u matte.py brennic/v2v/Death-r5 brennic/matte/DeathR5 2>&1 | tail -1
rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B (round 5 death matte)" >> $G/lock-log.txt
echo BATCHO_DONE
