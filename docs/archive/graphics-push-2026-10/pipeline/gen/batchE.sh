#!/bin/sh
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock
while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 2; done
echo "Tier B (chalk hero) round 3 - taken $(date '+%Y-%m-%d %H:%M:%S') (hands repaint, 12 views)" > $L
cd $G
N=12 DN=0.62 sh hands_batch.sh 2>&1
rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B (round 3 hands)" >> lock-log.txt
echo BATCHE_DONE
