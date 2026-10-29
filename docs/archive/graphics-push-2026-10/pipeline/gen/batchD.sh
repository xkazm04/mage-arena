#!/bin/sh
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock
take() { while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 20; done; echo "Tier B (chalk hero) round 2 - taken $(date '+%Y-%m-%d %H:%M:%S') ($1)" > $L; }
give() { rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B ($1)" >> $G/lock-log.txt; }
cd $G
take "hands repaint (12 views, denoise 0.72) + water + cuff"
N=12 DN=0.72 sh hands_batch.sh 2>&1
NOREFS=1 ASSET=water W=1280 H=720 python -u flux_key.py seed 41 0 2>&1 | tail -1
python -u ltx_i2v.py water water/seed-s41.png 5 51 1280 704 2>&1 | tail -1
NOREFS=1 ASSET=cuff W=1536 H=384 python -u flux_key.py band 61 0 2>&1 | tail -1
give "hands + water + cuff"
echo BATCHD_DONE
