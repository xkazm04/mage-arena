#!/bin/sh
G=/c/Users/kazda/mage-art-staging/push-1028/gen
L=/c/Users/kazda/mage-art-staging/push-1028/COMFY.lock
while [ -f $L ] && ! grep -q "Tier B" $L; do sleep 2; done
echo "Tier B (chalk hero) round 8 - taken $(date '+%Y-%m-%d %H:%M:%S') (cast key, LTX cast, H3 sunfall end, 2 mattes)" > $L
cd $G/brennic
[ -f castkeyn-s7.png ] || INIT=castkeym-s7.png DENOISE=0.8 W=768 H=768 python -u ../flux_key.py castkeyn 7 0.25 pose0-s7.png 2>&1 | tail -1
cd $G
[ -d brennic/v2v/Cast-n ] || PROMPT=v2v GUIDES="24:brennic/castkeyn-s7.png,-1:brennic/pose0-s7.png" python -u ltx_v2v.py Cast brennic/drive/Cast.mp4 brennic/pose0-s7.png 49 0.909375 11 n 2>&1 | tail -1
[ -d brennic/h3-sunn3 ] || PROMPT=h3nocape python -u h3_fl2va.py brennic sunn3 brennic/sunkeyn-s7.png brennic/castkeyn-s7.png 22 768 768 13 2>&1 | tail -1
cd $G/brennic && python - <<'PY'
import os, shutil, json
out='v2v/Sunfall-r8'; shutil.rmtree(out,ignore_errors=True); os.makedirs(out); i=0
for n,d in enumerate(['h3-sunn1','h3-sunn3']):
    fs=sorted(f for f in os.listdir(d) if f.endswith('.png'))
    for f in (fs if n==0 else fs[1:]): shutil.copy(f'{d}/{f}',f'{out}/f_{i:04d}.png'); i+=1
json.dump({"fps":24,"rig":False,"source":"H3 pose0 -> sunkeyn -> castkeyn (no cape)"},open(out+'/source.json','w'))
PY
cd $G
python -u matte.py brennic/v2v/Cast-n brennic/matte/Cast-n 2>&1 | tail -1
python -u matte.py brennic/v2v/Sunfall-r8 brennic/matte/Sunfall-r8 2>&1 | tail -1
curl -s -X POST http://127.0.0.1:8188/free -H "Content-Type: application/json" -d '{"unload_models": true, "free_memory": true}'; echo " free sent"
rm -f $L; echo "released $(date '+%H:%M:%S') by Tier B (round 8, models freed)" >> $G/lock-log.txt
echo BATCHV_DONE
