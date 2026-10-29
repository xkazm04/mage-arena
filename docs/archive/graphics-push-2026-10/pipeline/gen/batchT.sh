#!/bin/sh
# Round 7: Ward, Strafe, Hit and Sunfall keys repainted from the idle key (shawl collar only, no cape), segments re-rendered.
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock
take() { while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 2; done; echo "Tier B (chalk hero) round 7 - taken $(date '+%Y-%m-%d %H:%M:%S') ($1)" > $L; }
give() { rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B ($1)" >> $G/lock-log.txt; sleep 60; }
key() { [ -f $1-s7.png ] || INIT=pose0-s7.png DENOISE=0.92 W=768 H=768 python -u ../flux_key.py $1 7 0.25 pose0-s7.png 2>&1 | tail -1; }
seg() { [ -d brennic/h3-$1 ] || PROMPT=$4 python -u h3_fl2va.py brennic $1 $2 $3 22 768 768 13 2>&1 | tail -1; }
cd $G/brennic
take "hold 1/3: 5 keys"
for k in wardkeyn step2keyn crosskeyn flinchkeyn sunkeyn; do key $k; done
give "hold 1/3"
cd $G
take "hold 2/3: H3 ward, hit, sunfall"
seg wardn1 brennic/pose0-s7.png brennic/wardkeyn-s7.png h3nocape
seg wardn2 brennic/wardkeyn-s7.png brennic/pose0-s7.png h3nocape
seg flinchn1 brennic/pose0-s7.png brennic/flinchkeyn-s7.png h3flinchn
seg flinchn2 brennic/flinchkeyn-s7.png brennic/pose0-s7.png h3nocape
seg sunn1 brennic/pose0-s7.png brennic/sunkeyn-s7.png h3nocape
seg sunn2 brennic/sunkeyn-s7.png brennic/castkeym-s7.png h3nocape
give "hold 2/3"
take "hold 3/3: H3 strafe + mattes"
seg siden1 brennic/pose0-s7.png brennic/step2keyn-s7.png h3stepn
seg siden2 brennic/step2keyn-s7.png brennic/crosskeyn-s7.png h3stepn
seg siden3 brennic/crosskeyn-s7.png brennic/pose0-s7.png h3stepn
echo SEGS_DONE
rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B (round 7 segments)" >> $G/lock-log.txt
echo BATCHT_DONE
