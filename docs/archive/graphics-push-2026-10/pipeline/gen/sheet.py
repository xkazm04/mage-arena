import os, sys
from PIL import Image
d, out = sys.argv[1], sys.argv[2]
step = int(sys.argv[3]) if len(sys.argv) > 3 else 4
fs = sorted(f for f in os.listdir(d) if f.endswith('.png'))
pick = fs[::step]
cols = 7
rows = (len(pick) + cols - 1) // cols
sheet = Image.new('RGB', (220 * cols, 220 * rows), (30, 30, 30))
for i, f in enumerate(pick):
    im = Image.open(os.path.join(d, f))
    if im.mode == 'RGBA':
        bg = Image.new('RGBA', im.size, (60, 58, 56, 255)); bg.alpha_composite(im); im = bg
    sheet.paste(im.convert('RGB').resize((220, 220)), ((i % cols) * 220, (i // cols) * 220))
sheet.save(out)
print(out, len(pick))
