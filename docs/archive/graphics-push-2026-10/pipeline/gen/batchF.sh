#!/bin/sh
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock
until grep -q BATCHE_DONE $G/batchE.log 2>/dev/null; do sleep 5; done
while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 2; done
echo "Tier B (chalk hero) round 3 - taken $(date '+%Y-%m-%d %H:%M:%S') (hit key + H3 hit/death segments)" > $L
cd $G/brennic
[ -f hitface-s7.png ] || INIT=drive-Hit-08.png DENOISE=0.85 W=768 H=768 python -u ../flux_key.py hitface 7 0.25 pose0-s7.png ../refs/brennic-master-384.png 2>&1 | tail -1
cd $G
seg() { [ -d brennic/h3-$1 ] || PROMPT=h3seg python -u h3_fl2va.py brennic $1 $2 $3 22 768 768 13 2>&1 | tail -1; }
seg hit1 brennic/pose0-s7.png brennic/hitface-s7.png
seg hit2 brennic/hitface-s7.png brennic/pose0-s7.png
seg death1 brennic/pose0-s7.png brennic/deathk16-s7.png
seg death2 brennic/deathk16-s7.png brennic/deathmid-s7.png
seg death3 brennic/deathmid-s7.png brennic/deathend-s7.png
rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B (round 3 H3 segments)" >> $G/lock-log.txt
echo BATCHF_DONE
