"""Normalize a cleaned 4x5 imagegen sprite sheet into 64x64 PNG frames."""
import argparse
from pathlib import Path

from PIL import Image


ASSET_DIR = Path(__file__).parent / "assets"
ROWS = ("happy", "hungry", "sleepy", "grumpy", "sleep")
SIZE = 64
ALPHA_CUTOFF = 64


def clean_transparent_edges(image: Image.Image) -> Image.Image:
    pixels = []
    for r, g, b, a in image.convert("RGBA").getdata():
        pixels.append((r, g, b, a) if a > ALPHA_CUTOFF else (0, 0, 0, 0))
    image = Image.new("RGBA", image.size)
    image.putdata(pixels)
    # Resize premultiplied colors so transparent fringe RGB cannot bleed inward.
    image = image.convert("RGBa").resize((SIZE, SIZE), Image.Resampling.LANCZOS).convert("RGBA")
    pixels = []
    for r, g, b, a in image.getdata():
        pixels.append((r, g, b, a) if a > ALPHA_CUTOFF else (0, 0, 0, 0))
    image.putdata(pixels)
    return image


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("sheet", type=Path, help="clean 4-column by 5-row transparent sprite sheet")
    args = parser.parse_args()

    sheet = Image.open(args.sheet).convert("RGBA")
    frames = Image.new("RGBA", (SIZE * 4, SIZE * 5), (0, 0, 0, 0))
    for row, prefix in enumerate(ROWS):
        top = round(row * sheet.height / 5)
        bottom = round((row + 1) * sheet.height / 5)
        for col in range(4):
            left = round(col * sheet.width / 4)
            right = round((col + 1) * sheet.width / 4)
            frame = clean_transparent_edges(sheet.crop((left, top, right, bottom)))
            frame.save(ASSET_DIR / f"{prefix}_{col + 1}.png")
            frames.alpha_composite(frame, (col * SIZE, row * SIZE))

    frames.save(ASSET_DIR / "pet_emotions_64_sheet.png")
    print(f"Wrote 20 cleaned 64x64 frames to {ASSET_DIR}")


if __name__ == "__main__":
    main()
