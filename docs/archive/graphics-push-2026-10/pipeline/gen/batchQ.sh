#!/bin/sh
# Round 6 (with the S5 additions): cape-locked keys, a real flinch, a sidestep cycle, the death with the cape kept close.
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock
take() { while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 2; done; echo "Tier B (chalk hero) round 6 - taken $(date '+%Y-%m-%d %H:%M:%S') ($1)" > $L; }
give() { rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B ($1)" >> $G/lock-log.txt; sleep 75; }
key() { [ -f $1-s7.png ] || INIT=$2 DENOISE=$3 W=768 H=768 python -u ../flux_key.py $1 7 0.25 pose0-s7.png ../refs/brennic-master-384.png 2>&1 | tail -1; }
seg() { [ -d brennic/h3-$1 ] || PROMPT=$4 python -u h3_fl2va.py brennic $1 $2 $3 22 768 768 13 2>&1 | tail -1; }
cd $G/brennic
take "hold 1/5: keys"
key castkeyc castkey-s7.png 0.55
key flinchkey pose0-s7.png 0.9
key crosskey stepkeyc-s7.png 0.75
key step2key pose0-s7.png 0.9
give "hold 1/5"
take "hold 2/5: death keys with the cape close"
key deathk16c deathk16-s7.png 0.55
key deathmidc deathmid-s7.png 0.55
key deathtopplec deathtopple-s7.png 0.55
[ -f deathcleanc-s7.png ] || NOREFS=1 INIT=deathclean-s19.png MASK=deathlying-cape-mask.png DENOISE=0.75 W=768 H=768 python -u ../flux_key.py deathcleanc 7 0 2>&1 | tail -1
give "hold 2/5"
cd $G
take "hold 3/5: H3 flinch, ward"
seg flinch1 brennic/pose0-s7.png brennic/flinchkey-s7.png h3flinch
seg flinch2 brennic/flinchkey-s7.png brennic/pose0-s7.png h3cape
seg wardc1 brennic/pose0-s7.png brennic/wardkeyc-s7.png h3cape
seg wardc2 brennic/wardkeyc-s7.png brennic/pose0-s7.png h3cape
give "hold 3/5"
take "hold 4/5: H3 sidesteps, sunfall"
seg side1 brennic/pose0-s7.png brennic/step2key-s7.png h3step
seg side2 brennic/step2key-s7.png brennic/crosskey-s7.png h3step
seg side3 brennic/crosskey-s7.png brennic/pose0-s7.png h3step
seg sunc1 brennic/pose0-s7.png brennic/sunkeyc-s7.png h3cape
seg sunc2 brennic/sunkeyc-s7.png brennic/castkeyc-s7.png h3cape
give "hold 4/5"
take "hold 5/5: H3 death chain, LTX cast"
seg deathc1 brennic/pose0-s7.png brennic/deathk16c-s7.png h3fall
seg deathc2 brennic/deathk16c-s7.png brennic/deathmidc-s7.png h3fall
seg deathc3 brennic/deathmidc-s7.png brennic/deathtopplec-s7.png h3fall
seg deathc4 brennic/deathtopplec-s7.png brennic/deathcleanc-s7.png h3fall
[ -d brennic/v2v/Cast-c ] || PROMPT=v2v GUIDES="24:brennic/castkeyc-s7.png,-1:brennic/pose0-s7.png" python -u ltx_v2v.py Cast brennic/drive/Cast.mp4 brennic/pose0-s7.png 49 0.909375 11 c 2>&1 | tail -1
give "hold 5/5"
echo BATCHQ_DONE
