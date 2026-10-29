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
key castkeym castkey-s7.png 0.7
key flinchkeym pose0-s7.png 0.9
key wardkeym wardkey-s7.png 0.7
key step2keym pose0-s7.png 0.9
key crosskeym step2keym-s7.png 0.75
key sunkeym sunkey-s7.png 0.7
give "hold 1/5"
take "hold 2/5: death keys with the cape close"
key deathk16m deathk16-s7.png 0.7
key deathmidm deathmid-s7.png 0.7
key deathtopplem deathtopple-s7.png 0.7
[ -f deathcleanm-s7.png ] || NOREFS=1 INIT=deathclean-s19.png MASK=deathlying-cape-mask.png DENOISE=0.75 W=768 H=768 python -u ../flux_key.py deathcleanm 7 0 2>&1 | tail -1
give "hold 2/5"
cd $G
take "hold 3/5: H3 flinch, ward"
seg flinchm1 brennic/pose0-s7.png brennic/flinchkeym-s7.png h3flinch
seg flinchm2 brennic/flinchkeym-s7.png brennic/pose0-s7.png h3cape
seg wardm1 brennic/pose0-s7.png brennic/wardkeym-s7.png h3cape
seg wardm2 brennic/wardkeym-s7.png brennic/pose0-s7.png h3cape
give "hold 3/5"
take "hold 4/5: H3 sidesteps, sunfall"
seg sidem1 brennic/pose0-s7.png brennic/step2keym-s7.png h3step
seg sidem2 brennic/step2keym-s7.png brennic/crosskeym-s7.png h3step
seg sidem3 brennic/crosskeym-s7.png brennic/pose0-s7.png h3step
seg sunm1 brennic/pose0-s7.png brennic/sunkeym-s7.png h3cape
seg sunm2 brennic/sunkeym-s7.png brennic/castkeym-s7.png h3cape
give "hold 4/5"
take "hold 5/5: H3 death chain, LTX cast"
seg deathm1 brennic/pose0-s7.png brennic/deathk16m-s7.png h3fall
seg deathm2 brennic/deathk16m-s7.png brennic/deathmidm-s7.png h3fall
seg deathm3 brennic/deathmidm-s7.png brennic/deathtopplem-s7.png h3fall
seg deathm4 brennic/deathtopplem-s7.png brennic/deathcleanm-s7.png h3fall
[ -d brennic/v2v/Cast-m ] || PROMPT=v2v GUIDES="24:brennic/castkeym-s7.png,-1:brennic/pose0-s7.png" python -u ltx_v2v.py Cast brennic/drive/Cast.mp4 brennic/pose0-s7.png 49 0.909375 11 m 2>&1 | tail -1
give "hold 5/5"
echo BATCHR_DONE
