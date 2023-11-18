from PIL import Image, ImageDraw

def run(p, c, i):
	image = Image.new("RGBA", (640, 480))
	draw = ImageDraw.Draw(image)
	draw.ellipse(p, fill = c)
	image.save(f"circle{i}.png")

run((100, 100, 300, 300), "red", 1)
run((100, 200, 300, 400), "green", 2)
run((200, 100, 400, 300), "blue", 3)
