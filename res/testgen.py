from PIL import Image, ImageDraw
from pathlib import Path

def run(p, c, i):
	pp = Path(__file__).parent
	image = Image.new("RGBA", (p[2], p[3]), (0, 0, 0, 0))
	draw = ImageDraw.Draw(image)
	draw.ellipse(p, fill = c)
	image.save(pp / f"circle{i}.png")

run((100, 200, 300, 400), (0, 255, 0, 128), 1)
run((200, 100, 400, 300), (0, 0, 255, 128), 2)
run((100, 100, 300, 300), (255, 0, 0, 255), 3)
