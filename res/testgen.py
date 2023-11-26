import numpy
from PIL import Image, ImageDraw
from pathlib import Path

pp = Path(__file__).parent
def run(p, c, o, i):
	image = Image.new("RGBA", (p[2] + 1, p[3] + 1), (0, 0, 0, 0))
	draw = ImageDraw.Draw(image)
	draw.ellipse(p, fill = c)
	image.save(pp / f"{i}_{o[0]}_{o[1]}.png")

s = 2000
a = numpy.zeros((s, s), dtype = numpy.uint8)
a += 255
a[2,:] = 0
a[s - 3,:] = 0
a[:,2] = 0
a[:,s - 3] = 0
image = Image.fromarray(a).convert("RGBA")
image.save(pp / f"0_0_0.png")
run((0, 0, 200, 200), (0, 255, 0, 128), (100, 0), 1)
run((0, 0, 200, 200), (255, 0, 0, 255), (0, 0), 2)
run((0, 0, 200, 200), (0, 0, 255, 128), (0, 200), 3)
