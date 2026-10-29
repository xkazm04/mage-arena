#!/bin/sh
# Flux 2 img2img repaints of the hand views (denoise $DN), S30 hand crop as style reference.
cd /c/Users/kazda/mage-art-staging/push-1028/gen/hands
mkdir -p painted
n=0
for f in views/view_*.png; do
  key=$(basename $f .png); v=${key#view_?_}
  [ -f painted/$key.png ] && continue
  [ $n -ge ${N:-99} ] && break
  n=$((n+1))
  cp hand_$v.prompt.txt $key.prompt.txt
  INIT=$f DENOISE=${DN:-0.6} W=768 H=768 ASSET=hands python -u ../flux_key.py $key 7 0.25 ../refs/s30-hands-crop.png 2>&1 | tail -1
  cp $key-s7.png painted/$key.png
done
