#!/bin/sh
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock
take() { while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 20; done; echo "Tier B (chalk hero) round 2 - taken $(date '+%Y-%m-%d %H:%M:%S') ($1)" > $L; }
give() { rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B ($1)" >> $G/lock-log.txt; sleep 60; }
cd $G
take "LTX hit/death + mattes"
GUIDES="8:brennic/hitkey-s7.png,16:brennic/hitkey16-s7.png,24:brennic/hitk24-s7.png,32:brennic/pose0-s7.png" python -u ltx_v2v.py Hit brennic/drive/Hit33.mp4 brennic/pose0-s7.png 33 0.909375 11 g6 2>&1 | tail -1
GUIDES="16:brennic/deathk16-s7.png,24:brennic/deathk24-s7.png,32:brennic/deathmid-s7.png,40:brennic/deathk40-s7.png,48:brennic/deathk48-s7.png,-1:brennic/deathend-s7.png" python -u ltx_v2v.py Death brennic/drive/Death.mp4 brennic/pose0-s7.png 65 0.909375 11 g6 2>&1 | tail -1
rm -rf brennic/matte/Hit6 brennic/matte/Death6
python -u matte.py brennic/v2v/Hit-g6 brennic/matte/Hit6 2>&1 | tail -1
python -u matte.py brennic/v2v/Death-g6 brennic/matte/Death6 2>&1 | tail -1
[ -d brennic/h3-idle1 ] && python -u matte.py brennic/h3-idle1 brennic/matte/IdleH3 2>&1 | tail -1
give "clip fixes"
take "hands repaint 1/2"
N=6 DN=0.6 sh hands_batch.sh 2>&1
give "hands 1/2"
take "hands repaint 2/2"
N=6 DN=0.6 sh hands_batch.sh 2>&1
give "hands 2/2"
take "water + cuff"
NOREFS=1 ASSET=water W=1280 H=720 python -u flux_key.py seed 41 0 2>&1 | tail -1
python -u ltx_i2v.py water water/seed-s41.png 5 51 1280 704 2>&1 | tail -1
NOREFS=1 ASSET=cuff W=1536 H=384 python -u flux_key.py band 61 0 2>&1 | tail -1
give "water + cuff"
echo BATCHB_DONE
