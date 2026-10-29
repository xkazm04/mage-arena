#!/bin/sh
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock
take() { while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 20; done; echo "Tier B (chalk hero) round 2 - taken $(date '+%Y-%m-%d %H:%M:%S') ($1)" > $L; }
give() { rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B ($1)" >> $G/lock-log.txt; sleep 60; }
cd $G/brennic
take "living idle keys + v2v"
for f in 32 64 96; do [ -f idlek$f-s7.png ] || INIT=drive-IdleLive-$f.png DENOISE=0.8 W=768 H=768 python -u ../flux_key.py idlek$f 7 0.25 pose0-s7.png ../refs/brennic-master-384.png 2>&1 | tail -1; done
cd $G
GUIDES="32:brennic/idlek32-s7.png,64:brennic/idlek64-s7.png,96:brennic/idlek96-s7.png,-1:brennic/pose0-s7.png" python -u ltx_v2v.py IdleLive brennic/drive/IdleLive.mp4 brennic/pose0-s7.png 121 0.909375 11 g4 2>&1 | tail -1
python -u matte.py brennic/v2v/IdleLive-g4 brennic/matte/IdleLive 2>&1 | tail -1
give "living idle"
echo BATCHC_DONE
