from PIL import Image, ImageDraw

def run(p, c, i):
	image = Image.new("RGBA", (800, 600))
	draw = ImageDraw.Draw(image)
	draw.ellipse(p, fill = c)
	image.save(f"circle{i}.png")

image = Image.new("RGBA", (800, 600), "black")
image.save(f"bg.png")
run((100, 100, 300, 300), (255, 0, 0, 128), 1)
run((100, 200, 300, 400), (0, 255, 0, 128), 2)
run((200, 100, 400, 300), (0, 0, 255, 128), 3)
