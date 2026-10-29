#!/bin/sh
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock
while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 2; done
echo "Tier B (chalk hero) round 6 - taken $(date '+%Y-%m-%d %H:%M:%S') (6 mattes)" > $L
cd $G
for c in Hit-r6 Ward-r6 Strafe-r6 Sunfall-r6 Death-r6 Cast-m; do python -u matte.py brennic/v2v/$c brennic/matte/$c 2>&1 | tail -1; done
curl -s -X POST http://127.0.0.1:8188/free -H "Content-Type: application/json" -d '{"unload_models": true, "free_memory": true}'; echo " free sent"
rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B (round 6 mattes, models freed)" >> $G/lock-log.txt
echo BATCHS_DONE
