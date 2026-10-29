#!/bin/sh
# Round 5 GPU work in three short holds.
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock
take() { while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 2; done; echo "Tier B (chalk hero) round 5 - taken $(date '+%Y-%m-%d %H:%M:%S') ($1)" > $L; }
give() { rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B ($1)" >> $G/lock-log.txt; sleep 90; }
key() { [ -f $1-s7.png ] || INIT=$2 DENOISE=$3 W=768 H=768 python -u ../flux_key.py $1 7 0.25 pose0-s7.png ../refs/brennic-master-384.png 2>&1 | tail -1; }
cd $G/brennic
take "hold 1/3: clean lying key + cape-locked keys"
for s in 7 19; do [ -f deathclean-s$s.png ] || NOREFS=1 INIT=deathlying-src.png MASK=deathlying-clean-mask.png DENOISE=0.95 W=768 H=768 python -u ../flux_key.py deathclean $s 0 2>&1 | tail -1; done
key hitfacec hitface-s7.png 0.55
key stepkeyc stepkey-s7.png 0.55
key wardkeyc wardkey-s7.png 0.55
key sunkeyc sunkey-s7.png 0.55
give "hold 1/3"
take "hold 2/3: cape-locked keys"
key castkeyc castkey-s7.png 0.55
key deathk16c deathk16-s7.png 0.55
key deathmidc deathmid-s7.png 0.55
key deathtopplec deathtopple-s7.png 0.55
give "hold 2/3"
echo KEYS_DONE
