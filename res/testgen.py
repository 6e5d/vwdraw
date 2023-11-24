from PIL import Image, ImageDraw
from pathlib import Path

pp = Path(__file__).parent
def run(p, c, i):
	image = Image.new("RGBA", (p[2], p[3]), (0, 0, 0, 0))
	draw = ImageDraw.Draw(image)
	draw.ellipse(p, fill = c)
	image.save(pp / f"circle{i}.png")

image = Image.new("RGBA", (400, 400), (255, 255, 200, 255))
image.save(pp / f"bg.png")
run((0, 0, 200, 200), (0, 255, 0, 128), 1)
run((0, 0, 200, 200), (255, 0, 0, 255), 2)
run((0, 0, 200, 200), (0, 0, 255, 128), 3)
