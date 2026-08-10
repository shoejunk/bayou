from collections import deque
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


CELL = 256
GRID = 9
ALPHA_CUT = 10

ROOT = Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
SOURCE_DIRS = (
    Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp"),
    Path(r"C:\Users\jarox\Downloads"),
)
OUT = ROOT / "assets" / "animations"


def source_path(animation: str) -> Path:
    name = f"SS_widowroot_{animation}.png"
    for directory in SOURCE_DIRS:
        candidate = directory / name
        if candidate.exists():
            return candidate
    raise FileNotFoundError(name)


def even_frames(start: int, end: int, count: int) -> list[int]:
    return [int(round(float(value))) for value in np.linspace(start, end, count)]


def zero_transparent_rgb(arr: np.ndarray) -> np.ndarray:
    arr = arr.copy()
    arr[arr[..., 3] <= ALPHA_CUT] = 0
    arr[arr[..., 3] == 0, :3] = 0
    return arr


def crop_frame(sheet: Image.Image, frame: int) -> Image.Image:
    frame_width = sheet.width // GRID
    frame_height = sheet.height // GRID
    index = frame - 1
    left = (index % GRID) * frame_width
    top = (index // GRID) * frame_height
    return sheet.crop((left, top, left + frame_width, top + frame_height)).convert("RGBA")


def bounds(img: Image.Image) -> tuple[int, int, int, int] | None:
    alpha = np.asarray(img.convert("RGBA"))[..., 3]
    ys, xs = np.nonzero(alpha > ALPHA_CUT)
    if len(xs) == 0:
        return None
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def remove_tiny_components(img: Image.Image, min_pixels: int = 28) -> Image.Image:
    arr = zero_transparent_rgb(np.asarray(img.convert("RGBA")))
    visible = arr[..., 3] > ALPHA_CUT
    height, width = visible.shape
    seen = np.zeros_like(visible, dtype=bool)

    for start_y in range(height):
        for start_x in range(width):
            if seen[start_y, start_x] or not visible[start_y, start_x]:
                continue
            stack = [(start_x, start_y)]
            seen[start_y, start_x] = True
            points: list[tuple[int, int]] = []
            while stack:
                x, y = stack.pop()
                points.append((x, y))
                for next_x, next_y in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if (
                        0 <= next_x < width
                        and 0 <= next_y < height
                        and not seen[next_y, next_x]
                        and visible[next_y, next_x]
                    ):
                        seen[next_y, next_x] = True
                        stack.append((next_x, next_y))
            if len(points) < min_pixels:
                for x, y in points:
                    arr[y, x] = 0

    return Image.fromarray(arr, "RGBA")


def harden_interior_alpha(img: Image.Image) -> Image.Image:
    arr = zero_transparent_rgb(np.asarray(img.convert("RGBA")))
    visible = Image.fromarray(np.where(arr[..., 3] > 20, 255, 0).astype(np.uint8), "L")
    interior = np.asarray(visible.filter(ImageFilter.MinFilter(5))) == 255
    arr[interior, 3] = 255
    return Image.fromarray(zero_transparent_rgb(arr), "RGBA")


def premultiplied_resize(img: Image.Image, size: tuple[int, int]) -> Image.Image:
    arr = np.asarray(img.convert("RGBA"), dtype=np.float32)
    alpha = arr[..., 3:4] / 255.0
    premultiplied = arr[..., :3] * alpha

    rgb_image = Image.fromarray(np.clip(premultiplied, 0, 255).astype(np.uint8), "RGB")
    alpha_image = Image.fromarray(np.clip(arr[..., 3], 0, 255).astype(np.uint8), "L")
    rgb_resized = np.asarray(rgb_image.resize(size, Image.Resampling.LANCZOS), dtype=np.float32)
    alpha_resized = np.asarray(alpha_image.resize(size, Image.Resampling.LANCZOS), dtype=np.float32)

    resized_alpha = alpha_resized[..., None] / 255.0
    rgb = np.zeros_like(rgb_resized)
    nonzero = resized_alpha[..., 0] > 0.001
    rgb[nonzero] = rgb_resized[nonzero] / resized_alpha[nonzero]
    out = np.dstack([np.clip(rgb, 0, 255), alpha_resized])
    return Image.fromarray(zero_transparent_rgb(np.clip(out, 0, 255).astype(np.uint8)), "RGBA")


def body_core_x(img: Image.Image) -> float:
    arr = np.asarray(img.convert("RGBA"))
    alpha = arr[..., 3]
    box = bounds(img)
    if box is None:
        return img.width / 2.0
    left, top, right, bottom = box
    height = max(1, bottom - top + 1)
    y0 = top + round(height * 0.22)
    y1 = top + round(height * 0.68)
    region = alpha[y0 : y1 + 1] > ALPHA_CUT
    _ys, xs = np.nonzero(region)
    if len(xs) < 20:
        return (left + right) / 2.0
    return float(np.median(xs))


def lower_contact(img: Image.Image) -> tuple[float, float]:
    arr = np.asarray(img.convert("RGBA"))
    alpha = arr[..., 3]
    ys, _xs = np.nonzero(alpha > ALPHA_CUT)
    if len(ys) == 0:
        return img.width / 2.0, img.height - 1.0
    bottom = int(ys.max())
    band = alpha[max(0, bottom - 12) : bottom + 1] > ALPHA_CUT
    _band_y, band_x = np.nonzero(band)
    if len(band_x) < 8:
        return body_core_x(img), float(bottom)
    return float(np.median(band_x)), float(bottom)


def repair_white_edge(img: Image.Image, frame_count: int) -> Image.Image:
    arr = zero_transparent_rgb(np.asarray(img.convert("RGBA")))
    out = arr.copy()
    for frame_index in range(frame_count):
        frame_left = frame_index * CELL
        frame = arr[:, frame_left : frame_left + CELL]
        alpha = frame[..., 3]
        for y in range(1, CELL - 1):
            for x in range(1, CELL - 1):
                pixel = frame[y, x]
                if pixel[3] == 0 or pixel[3] > 225:
                    continue
                if not np.any(alpha[y - 1 : y + 2, x - 1 : x + 2] == 0):
                    continue
                neighbors = frame[y - 1 : y + 2, x - 1 : x + 2]
                opaque = neighbors[neighbors[..., 3] > 210]
                if len(opaque) == 0:
                    continue
                neutral = max(
                    abs(int(pixel[0]) - int(pixel[1])),
                    abs(int(pixel[0]) - int(pixel[2])),
                    abs(int(pixel[1]) - int(pixel[2])),
                )
                brightness = float(np.mean(pixel[:3]))
                local_color = np.mean(opaque[:, :3], axis=0)
                if brightness > 145 and neutral < 48 and brightness > float(np.mean(local_color)) + 25:
                    out[y, frame_left + x, :3] = np.clip(local_color, 0, 255).astype(np.uint8)
    return Image.fromarray(zero_transparent_rgb(out), "RGBA")


def decontaminate_silhouette(img: Image.Image, frame_count: int) -> Image.Image:
    arr = zero_transparent_rgb(np.asarray(img.convert("RGBA")))
    out = arr.copy()

    for frame_index in range(frame_count):
        frame_left = frame_index * CELL
        frame = arr[:, frame_left : frame_left + CELL]
        alpha = frame[..., 3]
        visible = alpha > ALPHA_CUT
        solid = Image.fromarray(np.where(visible, 255, 0).astype(np.uint8), "L")
        inner_four = np.asarray(solid.filter(ImageFilter.MinFilter(9))) == 255
        inner_five = np.asarray(solid.filter(ImageFilter.MinFilter(11))) == 255
        edge_band = visible & ~inner_four
        trusted = inner_five & (alpha > 100)
        if not np.any(trusted):
            continue

        seed_y = np.full((CELL, CELL), -1, dtype=np.int16)
        seed_x = np.full((CELL, CELL), -1, dtype=np.int16)
        queue: deque[tuple[int, int]] = deque()
        for y, x in zip(*np.nonzero(trusted)):
            seed_y[y, x] = y
            seed_x[y, x] = x
            queue.append((x, y))

        while queue:
            x, y = queue.popleft()
            for next_x, next_y in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= next_x < CELL and 0 <= next_y < CELL and seed_y[next_y, next_x] < 0:
                    seed_y[next_y, next_x] = seed_y[y, x]
                    seed_x[next_y, next_x] = seed_x[y, x]
                    queue.append((next_x, next_y))

        for y, x in zip(*np.nonzero(edge_band)):
            source_y = int(seed_y[y, x])
            source_x = int(seed_x[y, x])
            if source_y < 0 or source_x < 0:
                continue
            y0 = max(0, source_y - 2)
            y1 = min(CELL, source_y + 3)
            x0 = max(0, source_x - 2)
            x1 = min(CELL, source_x + 3)
            local_mask = trusted[y0:y1, x0:x1]
            local_colors = frame[y0:y1, x0:x1, :3][local_mask]
            if len(local_colors) == 0:
                continue
            propagated = np.median(local_colors, axis=0) * 0.72
            out[y, frame_left + x, :3] = np.clip(propagated, 0, 255).astype(np.uint8)

    return Image.fromarray(zero_transparent_rgb(out), "RGBA")


def remove_residual_neutral_halo(img: Image.Image, frame_count: int) -> Image.Image:
    arr = zero_transparent_rgb(np.asarray(img.convert("RGBA")))
    out = arr.copy()

    for frame_index in range(frame_count):
        frame_left = frame_index * CELL
        frame = arr[:, frame_left : frame_left + CELL]
        alpha = frame[..., 3]
        visible = alpha > ALPHA_CUT
        solid = Image.fromarray(np.where(visible, 255, 0).astype(np.uint8), "L")
        inner_four = np.asarray(solid.filter(ImageFilter.MinFilter(9))) == 255
        inner_eight = np.asarray(solid.filter(ImageFilter.MinFilter(17))) == 255
        residual_band = inner_four & ~inner_eight

        rgb = frame[..., :3].astype(np.float32)
        brightness = np.mean(rgb, axis=2)
        spread = np.max(rgb, axis=2) - np.min(rgb, axis=2)
        suspicious = residual_band & (brightness > 75) & (spread < 65)

        for y, x in zip(*np.nonzero(suspicious)):
            replacement: np.ndarray | None = None
            for radius in range(1, 5):
                y0 = max(0, y - radius)
                y1 = min(CELL, y + radius + 1)
                x0 = max(0, x - radius)
                x1 = min(CELL, x + radius + 1)
                valid = (
                    visible[y0:y1, x0:x1]
                    & ~suspicious[y0:y1, x0:x1]
                    & (alpha[y0:y1, x0:x1] > 80)
                )
                local_colors = frame[y0:y1, x0:x1, :3][valid]
                if len(local_colors) < 4:
                    continue
                local_brightness = np.mean(local_colors, axis=1)
                darker = local_colors[local_brightness < brightness[y, x] - 5]
                if len(darker) >= 3:
                    local_colors = darker
                replacement = np.median(local_colors, axis=0)
                break
            if replacement is not None:
                out[y, frame_left + x, :3] = np.clip(replacement * 0.88, 0, 255).astype(np.uint8)

    return Image.fromarray(zero_transparent_rgb(out), "RGBA")


def build(
    source_name: str,
    output_name: str,
    frames: list[int],
    max_width: int,
    max_height: int,
    anchor_mode: str,
    target_x: int,
    target_y: int,
) -> None:
    source = Image.open(source_path(source_name)).convert("RGBA")
    raw = [harden_interior_alpha(remove_tiny_components(crop_frame(source, frame))) for frame in frames]
    boxes = [box for img in raw if (box := bounds(img)) is not None]
    widest = max(right - left + 1 for left, _top, right, _bottom in boxes)
    tallest = max(bottom - top + 1 for _left, top, _right, bottom in boxes)
    scale = min(max_width / widest, max_height / tallest, 1.0)

    resized_size = (round(raw[0].width * scale), round(raw[0].height * scale))
    resized = [premultiplied_resize(img, resized_size) for img in raw]
    placements: list[tuple[int, int]] = []

    if anchor_mode == "killed_fixed":
        fixed_x = body_core_x(resized[0])
        final_ground = max(bounds(img)[3] for img in resized if bounds(img) is not None)
        placement = (round(target_x - fixed_x), round(target_y - final_ground))
        placements = [placement for _img in resized]
    else:
        for img in resized:
            box = bounds(img)
            if box is None:
                placements.append((0, 0))
                continue
            if anchor_mode == "foot_ground":
                anchor_x, anchor_y = lower_contact(img)
            else:
                anchor_x = body_core_x(img)
                anchor_y = float(box[3])
            placements.append((round(target_x - anchor_x), round(target_y - anchor_y)))

    sheet = Image.new("RGBA", (CELL * len(frames), CELL), (0, 0, 0, 0))
    for index, (img, (dx, dy)) in enumerate(zip(resized, placements)):
        box = bounds(img)
        if box is not None:
            left, top, right, bottom = box
            if dx + left < 2 or dx + right > CELL - 3 or dy + top < 2 or dy + bottom > CELL - 3:
                raise RuntimeError(
                    f"{source_name} frame {frames[index]} would clip at {(dx + left, dy + top, dx + right, dy + bottom)}"
                )
        sheet.alpha_composite(img, (index * CELL + dx, dy))

    arr = zero_transparent_rgb(np.asarray(sheet))
    for index in range(len(frames)):
        left = index * CELL
        arr[:, left] = 0
        arr[:, left + CELL - 1] = 0
        arr[0, left : left + CELL] = 0
        arr[CELL - 1, left : left + CELL] = 0

    finished = repair_white_edge(Image.fromarray(arr, "RGBA"), len(frames))
    finished = decontaminate_silhouette(finished, len(frames))
    finished = remove_residual_neutral_halo(finished, len(frames))
    output = OUT / f"widowroot-{output_name}.png"
    finished.save(output)
    print(output.name, finished.size, f"scale={scale:.4f}", "frames=" + ",".join(map(str, frames)))


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    build("walking", "walk", even_frames(2, 80, 24), 228, 232, "body_ground", 128, 244)
    build("attack", "attack", even_frames(2, 80, 12), 222, 230, "foot_ground", 95, 244)
    build("damaged", "damaged", even_frames(2, 80, 12), 226, 232, "foot_ground", 104, 244)
    build("killed", "killed", even_frames(2, 45, 12), 230, 228, "killed_fixed", 128, 244)


if __name__ == "__main__":
    main()
