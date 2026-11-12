#!/usr/bin/env python3
"""Generate PetCard-branded replacements for the stock power animations."""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parent
ASSETS = ROOT / "assets"
OUTPUT = ASSETS / "boot"
SIZE = 128
BACKGROUND = (8, 12, 24, 255)
MINT = (120, 225, 170, 255)
BLUE = (90, 210, 255, 255)
YELLOW = (255, 220, 90, 255)
DIM = (150, 160, 185, 255)


def font(size: int) -> ImageFont.ImageFont:
    for name in ("arial.ttf", "DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def text_center(draw: ImageDraw.ImageDraw, y: int, text: str,
                face: ImageFont.ImageFont, color: tuple[int, ...]) -> None:
    box = draw.textbbox((0, 0), text, font=face)
    x = (SIZE - (box[2] - box[0])) // 2
    draw.text((x, y), text, font=face, fill=color)


def screen(label: str, phase: float, sleeping: bool) -> Image.Image:
    canvas = Image.new("RGBA", (SIZE, SIZE), BACKGROUND)
    draw = ImageDraw.Draw(canvas)

    for x, y in ((18, 27), (108, 23), (15, 96), (112, 92), (26, 112), (102, 108)):
        draw.point((x, y), fill=(75, 100, 125, 255))

    draw.ellipse((9, 8, 119, 118), outline=(38, 46, 68, 255), width=2)
    end = -90 + int(360 * phase)
    draw.arc((9, 8, 119, 118), -90, end, fill=YELLOW if sleeping else MINT, width=3)

    text_center(draw, 13, "PETCARD", font(11), MINT)
    sprite_path = ASSETS / ("sleep_1.png" if sleeping else "happy_1.png")
    sprite = Image.open(sprite_path).convert("RGBA")
    if sprite.size != (64, 64):
        sprite = sprite.resize((64, 64), Image.Resampling.NEAREST)
    canvas.alpha_composite(sprite, (32, 34))
    text_center(draw, 105, label, font(9), YELLOW if sleeping else DIM)
    return canvas


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    base = screen("PETCARD", 0.25, False)
    base.save(OUTPUT / "background.png")
    base.save(OUTPUT / "first_background.png")
    base.save(OUTPUT / "second_background.png")
    base.save(OUTPUT / "adol.png")
    screen("GOOD NIGHT", 0.75, True).save(OUTPUT / "background-poweroff.png")

    for index in range(11):
        screen("HELLO", index / 10, False).save(OUTPUT / f"image_poweron_{index}.png")
    for index in range(10):
        screen("GOOD NIGHT", index / 9, True).save(OUTPUT / f"image_poweroff_{index}.png")


if __name__ == "__main__":
    main()
