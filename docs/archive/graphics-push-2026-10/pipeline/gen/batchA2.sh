#!/bin/sh
cd /c/Users/kazda/mage-art-staging/push-1028/gen/brennic
for k in "deathk16 drive-Death-16.png" "deathk24 drive-Death-24.png" "deathk40 drive-Death-40.png" "deathk48 drive-Death-48.png"; do set -- $k
  [ -f $1-s7.png ] && continue
  INIT=$2 DENOISE=0.8 W=768 H=768 python -u ../flux_key.py $1 7 0.25 pose0-s7.png ../refs/brennic-master-384.png 2>&1 | tail -1
done
cd ..
[ -d brennic/h3-idle1 ] || python -u h3_fl2va.py brennic idle1 brennic/pose0-s7.png brennic/pose0-s7.png 56 768 768 5 2>&1 | tail -3
echo BATCHA_DONE
