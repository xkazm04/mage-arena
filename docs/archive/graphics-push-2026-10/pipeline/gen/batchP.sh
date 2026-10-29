#!/bin/sh
# Round 6: cape-locked keys and the clips re-rendered from them, in holds of at most ~30 min.
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock
take() { while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 2; done; echo "Tier B (chalk hero) round 6 - taken $(date '+%Y-%m-%d %H:%M:%S') ($1)" > $L; }
give() { rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B ($1)" >> $G/lock-log.txt; sleep 75; }
key() { [ -f $1-s7.png ] || INIT=$2 DENOISE=$3 W=768 H=768 python -u ../flux_key.py $1 7 0.25 pose0-s7.png ../refs/brennic-master-384.png 2>&1 | tail -1; }
seg() { [ -d brennic/h3-$1 ] || PROMPT=h3cape python -u h3_fl2va.py brennic $1 $2 $3 22 768 768 13 2>&1 | tail -1; }
cd $G/brennic
take "hold 1/4: cape-locked keys"
key hitfacec hitface-s7.png 0.55
key wardkeyc wardkey-s7.png 0.55
key stepkeyc stepkey-s7.png 0.55
key sunkeyc sunkey-s7.png 0.55
key castkeyc castkey-s7.png 0.55
give "hold 1/4"
cd $G
take "hold 2/4: H3 hit, ward"
seg hitc1 brennic/pose0-s7.png brennic/hitfacec-s7.png
seg wardc1 brennic/pose0-s7.png brennic/wardkeyc-s7.png
seg wardc2 brennic/wardkeyc-s7.png brennic/pose0-s7.png
give "hold 2/4"
take "hold 3/4: H3 strafe, sunfall"
seg stepc1 brennic/pose0-s7.png brennic/stepkeyc-s7.png
seg stepc2 brennic/stepkeyc-s7.png brennic/pose0-s7.png
seg sunc1 brennic/pose0-s7.png brennic/sunkeyc-s7.png
seg sunc2 brennic/sunkeyc-s7.png brennic/castkeyc-s7.png
give "hold 3/4"
take "hold 4/4: LTX cast + mattes"
[ -d brennic/v2v/Cast-c ] || PROMPT=v2v GUIDES="24:brennic/castkeyc-s7.png,-1:brennic/pose0-s7.png" python -u ltx_v2v.py Cast brennic/drive/Cast.mp4 brennic/pose0-s7.png 49 0.909375 11 c 2>&1 | tail -1
echo SEGS_DONE
give "hold 4/4"
echo BATCHP_DONE
