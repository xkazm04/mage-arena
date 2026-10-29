#!/bin/sh
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock


while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 2; done
echo "Tier B (chalk hero) round 5 - taken $(date '+%Y-%m-%d %H:%M:%S') (death fall to the lying key: 1 H3 segment + matte)" > $L
cd $G
[ -d brennic/h3-fall5 ] || PROMPT=h3fall python -u h3_fl2va.py brennic fall5 brennic/deathtopple-s7.png brennic/deathclean-s19.png 30 768 768 13 2>&1 | tail -1
rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B (round 5 fall)" >> $G/lock-log.txt
echo BATCHN_DONE
