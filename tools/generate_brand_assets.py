#!/usr/bin/env python3
"""Generate the Dragon Download Manager PNG and Windows ICO app icons."""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
ICON_DIR = ROOT / "src" / "resources" / "logo"
SIZES = (16, 24, 32, 48, 64, 128, 256, 512, 1024)
CANVAS_SIZE = 1024


def vertical_gradient(top: tuple[int, int, int], bottom: tuple[int, int, int]) -> Image.Image:
    image = Image.new("RGBA", (CANVAS_SIZE, CANVAS_SIZE))
    draw = ImageDraw.Draw(image)
    for y in range(CANVAS_SIZE):
        ratio = y / (CANVAS_SIZE - 1)
        color = tuple(round(a + (b - a) * ratio) for a, b in zip(top, bottom))
        draw.line((0, y, CANVAS_SIZE, y), fill=(*color, 255))
    return image


def make_icon() -> Image.Image:
    """Load the generated artwork; packaging only resizes this master image."""
    return Image.open(ICON_DIR / "dragon-master.png").convert("RGBA")


def brand_font(size: int, bold: bool = False):
    font_names = ("seguisb.ttf", "arialbd.ttf") if bold else ("segoeui.ttf", "arial.ttf")
    font_directories = (
        Path("C:/Windows/Fonts"),
        Path("/usr/share/fonts/truetype/dejavu"),
    )
    for directory in font_directories:
        for font_name in font_names:
            font_path = directory / font_name
            if font_path.exists():
                return ImageFont.truetype(str(font_path), size=size)
    return ImageFont.load_default(size=size)


def make_installer_banners(source: Image.Image) -> None:
    header_size = Image.open(ROOT / "installer" / "windows" / "NSIS" / "images" / "header.bmp").size
    header = vertical_gradient((7, 8, 10), (19, 16, 11)).resize(header_size)
    header_draw = ImageDraw.Draw(header)
    header_icon = source.resize((42, 42), Image.Resampling.LANCZOS)
    header.alpha_composite(header_icon, (7, (header.height - 42) // 2))
    header_draw.line((0, header.height - 2, header.width, header.height - 2), fill=(218, 166, 54), width=2)
    header_draw.text((57, 19), "Dragon Download", font=brand_font(10, bold=True), fill=(232, 238, 247))
    header.convert("RGB").save(ROOT / "installer" / "windows" / "NSIS" / "images" / "header.bmp")

    banner_size = Image.open(ROOT / "installer" / "windows" / "NSIS" / "images" / "branding.bmp").size
    banner = vertical_gradient((7, 8, 10), (16, 13, 9)).resize(banner_size)
    banner_draw = ImageDraw.Draw(banner)
    banner_draw.ellipse((18, 4, banner.width - 18, banner.width - 4), outline=(91, 66, 19), width=3)
    banner_icon_size = 86
    banner_icon = source.resize((banner_icon_size, banner_icon_size), Image.Resampling.LANCZOS)
    banner.alpha_composite(banner_icon, ((banner.width - banner_icon_size) // 2, 21))
    banner_draw.line((28, 125, banner.width - 28, 125), fill=(218, 166, 54), width=3)
    text_lines = ("Dragon", "Download", "Manager")
    text_font = brand_font(15, bold=True)
    for index, line in enumerate(text_lines):
        bounds = banner_draw.textbbox((0, 0), line, font=text_font)
        line_width = bounds[2] - bounds[0]
        banner_draw.text(((banner.width - line_width) // 2, 148 + index * 23), line, font=text_font, fill=(232, 238, 247))
    group_font = brand_font(11)
    group = "Brami Group"
    group_bounds = banner_draw.textbbox((0, 0), group, font=group_font)
    banner_draw.text(((banner.width - (group_bounds[2] - group_bounds[0])) // 2, banner.height - 37), group, font=group_font, fill=(218, 166, 54))
    banner.convert("RGB").save(ROOT / "installer" / "windows" / "NSIS" / "images" / "branding.bmp")


def main() -> None:
    ICON_DIR.mkdir(parents=True, exist_ok=True)
    source = make_icon()
    for size in SIZES:
        resized = source.resize((size, size), Image.Resampling.LANCZOS)
        resized.save(ICON_DIR / f"icon{size}.png", optimize=True)

    ico_image = source.resize((256, 256), Image.Resampling.LANCZOS)
    ico_image.save(
        ICON_DIR / "icon.ico",
        format="ICO",
        sizes=[(size, size) for size in (16, 24, 32, 48, 64, 128, 256)],
    )
    source.resize((64, 64), Image.Resampling.LANCZOS).save(
        ROOT / "installer" / "unix" / "portable" / "DragonDownloadManager_64x64.png",
        optimize=True,
    )
    make_installer_banners(source)


if __name__ == "__main__":
    main()
