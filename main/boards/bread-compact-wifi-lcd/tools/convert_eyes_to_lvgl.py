#!/usr/bin/env python3
# Convert eyes.jpg to LVGL RGB565 C image (little-endian), 160x160.
from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image

TARGET_SIZE = (160, 160)


def crop_center_square(img: Image.Image) -> Image.Image:
    width, height = img.size
    if width == height:
        return img
    if width > height:
        left = (width - height) // 2
        upper = 0
        right = left + height
        lower = height
    else:
        left = 0
        upper = (height - width) // 2
        right = width
        lower = upper + width
    return img.crop((left, upper, right, lower))


def rgb_to_rgb565_le(rgb_pixels):
    out = bytearray()
    for r, g, b in rgb_pixels:
        value = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        out.append(value & 0xFF)
        out.append((value >> 8) & 0xFF)
    return out


def format_c_array(data: bytes, bytes_per_line: int = 16) -> str:
    lines = []
    for i in range(0, len(data), bytes_per_line):
        chunk = data[i:i + bytes_per_line]
        line = ",".join(f"0x{b:02x}" for b in chunk)
        lines.append(f"    {line},")
    return "\n".join(lines)


def write_c_files(out_c: Path, out_h: Path, data: bytes, width: int, height: int):
    map_name = "eyes_img_map"
    symbol_name = "eyes_img"
    stride = width * 2

    c_contents = f"""
#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#elif defined(LV_BUILD_TEST)
#include "../lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_EYES_IMG
#define LV_ATTRIBUTE_EYES_IMG
#endif

static const
LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_EYES_IMG
uint8_t {map_name}[] = {{
{format_c_array(data)}

}};

const lv_image_dsc_t {symbol_name} = {{
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.cf = LV_COLOR_FORMAT_RGB565,
  .header.flags = 0,
  .header.w = {width},
  .header.h = {height},
  .header.stride = {stride},
  .data_size = {len(data)},
  .data = {map_name},
}};
""".lstrip()

    h_contents = f"""
#ifndef EYES_IMG_H
#define EYES_IMG_H

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifdef __cplusplus
extern "C" {{
#endif

extern const lv_image_dsc_t {symbol_name};

#ifdef __cplusplus
}} /* extern "C" */
#endif

#endif /* EYES_IMG_H */
""".lstrip()

    out_c.write_text(c_contents, encoding="utf-8", newline="\n")
    out_h.write_text(h_contents, encoding="utf-8", newline="\n")


def main():
    script_dir = Path(__file__).resolve().parent
    board_dir = script_dir.parent
    default_src = board_dir / "eye02.jpg"
    assets_dir = board_dir / "assets"

    parser = argparse.ArgumentParser(description="Convert eyes.jpg to LVGL RGB565 C image.")
    parser.add_argument("--src", type=Path, default=default_src, help="Source JPG path")
    parser.add_argument("--out-c", type=Path, default=assets_dir / "eyes_img.c", help="Output C file")
    parser.add_argument("--out-h", type=Path, default=assets_dir / "eyes_img.h", help="Output header file")
    parser.add_argument("--strategy", choices=["crop", "scale"], default="crop",
                        help="Resize strategy: crop (center-crop to square then scale) or scale (direct)")
    args = parser.parse_args()

    if not args.src.exists():
        raise FileNotFoundError(f"Source image not found: {args.src}")

    assets_dir.mkdir(parents=True, exist_ok=True)

    img = Image.open(args.src).convert("RGB")
    original_size = img.size

    if args.strategy == "crop":
        img = crop_center_square(img)
    img = img.resize(TARGET_SIZE, Image.LANCZOS)

    rgb_pixels = list(img.getdata())
    rgb565 = rgb_to_rgb565_le(rgb_pixels)

    write_c_files(args.out_c, args.out_h, rgb565, TARGET_SIZE[0], TARGET_SIZE[1])

    print(f"Source: {args.src} size={original_size}")
    print(f"Strategy: {args.strategy}, output size={TARGET_SIZE}")
    print(f"Generated: {args.out_c}")
    print(f"Generated: {args.out_h}")


if __name__ == "__main__":
    main()
