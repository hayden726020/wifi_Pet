"""Convert the supplied transparent PNG frames to raw RGBA for PetCore."""
from pathlib import Path

from PIL import Image


ASSET_DIR = Path(__file__).parent / "assets"
PREFIXES = ("happy", "hungry", "sleepy", "grumpy", "sleep")
SIZE = (64, 64)


def main() -> None:
    for prefix in PREFIXES:
        for frame in range(1, 5):
            path = ASSET_DIR / f"{prefix}_{frame}.png"
            image = Image.open(path).convert("RGBA")
            if image.size != SIZE:
                raise ValueError(f"{path} must be 64x64, got {image.size}")
            raw_path = path.with_suffix(".rgba")
            raw_path.write_bytes(image.tobytes())
            print(f"{raw_path.name}: {raw_path.stat().st_size} bytes")


if __name__ == "__main__":
    main()
