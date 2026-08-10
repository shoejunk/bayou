from pathlib import Path

import numpy as np
from PIL import Image


CELL = 256
GRID = 9
ALPHA_CUT = 10

ROOT = Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
SRC = Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp")
OUT = ROOT / "assets" / "animations"


def zero_transparent_rgb(arr: np.ndarray) -> np.ndarray:
    arr = arr.copy()
    arr[arr[..., 3] <= ALPHA_CUT] = 0
    arr[arr[..., 3] == 0, :3] = 0
    return arr


def even_frames(start: int, end: int, count: int) -> list[int]:
    frames: list[int] = []
    used: set[int] = set()
    for value in np.linspace(start, end, count):
        frame = int(round(float(value)))
        while frame in used and frame < end:
            frame += 1
        while frame in used and frame > start:
            frame -= 1
        frames.append(frame)
        used.add(frame)
    return frames


def premultiplied_resize(img: Image.Image, size: tuple[int, int]) -> Image.Image:
    arr = np.asarray(img.convert("RGBA"), dtype=np.float32)
    alpha = arr[..., 3:4] / 255.0
    premul = arr[..., :3] * alpha
    premul_img = Image.fromarray(np.clip(premul, 0, 255).astype(np.uint8), "RGB")
    alpha_img = Image.fromarray(np.clip(arr[..., 3], 0, 255).astype(np.uint8), "L")
    premul_resized = np.asarray(premul_img.resize(size, Image.Resampling.LANCZOS), dtype=np.float32)
    alpha_resized = np.asarray(alpha_img.resize(size, Image.Resampling.LANCZOS), dtype=np.float32)
    a = alpha_resized[..., None] / 255.0
    rgb = np.zeros_like(premul_resized)
    mask = a[..., 0] > 0.001
    rgb[mask] = premul_resized[mask] / a[mask]
    out = np.dstack([np.clip(rgb, 0, 255), alpha_resized])
    return Image.fromarray(zero_transparent_rgb(np.clip(out, 0, 255).astype(np.uint8)), "RGBA")


def crop_frame(sheet: Image.Image, frame: int) -> Image.Image:
    fw, fh = sheet.width // GRID, sheet.height // GRID
    idx = frame - 1
    x = (idx % GRID) * fw
    y = (idx // GRID) * fh
    return sheet.crop((x, y, x + fw, y + fh)).convert("RGBA")


def bounds(img: Image.Image) -> tuple[int, int, int, int] | None:
    alpha = np.asarray(img.convert("RGBA"))[..., 3]
    ys, xs = np.nonzero(alpha > ALPHA_CUT)
    if len(xs) == 0:
        return None
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def remove_tiny_components(img: Image.Image, min_pixels: int = 24) -> Image.Image:
    arr = zero_transparent_rgb(np.asarray(img.convert("RGBA")))
    alpha = arr[..., 3] > ALPHA_CUT
    h, w = alpha.shape
    seen = np.zeros_like(alpha, dtype=bool)
    for sy in range(h):
        for sx in range(w):
            if seen[sy, sx] or not alpha[sy, sx]:
                continue
            stack = [(sx, sy)]
            seen[sy, sx] = True
            pts: list[tuple[int, int]] = []
            while stack:
                x, y = stack.pop()
                pts.append((x, y))
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if 0 <= nx < w and 0 <= ny < h and not seen[ny, nx] and alpha[ny, nx]:
                        seen[ny, nx] = True
                        stack.append((nx, ny))
            if len(pts) < min_pixels:
                for x, y in pts:
                    arr[y, x] = 0
    return Image.fromarray(arr, "RGBA")


def foot_anchor(img: Image.Image) -> tuple[float, float]:
    arr = np.asarray(img.convert("RGBA"))
    alpha = arr[..., 3]
    ys, xs = np.nonzero(alpha > ALPHA_CUT)
    if len(xs) == 0:
        return CELL / 2, CELL - 18
    bottom = int(ys.max())
    lower = alpha[max(0, bottom - 18) : bottom + 1] > ALPHA_CUT
    _fy, fx = np.nonzero(lower)
    if len(fx) >= 6:
        return float(np.median(fx)), float(bottom)
    return float((xs.min() + xs.max()) / 2.0), float(bottom)


def primary_foot_anchor(img: Image.Image) -> tuple[float, float]:
    arr = np.asarray(img.convert("RGBA"))
    alpha = arr[..., 3]
    ys, xs = np.nonzero(alpha > ALPHA_CUT)
    if len(xs) == 0:
        return CELL / 2, CELL - 18
    bottom = int(ys.max())
    band = alpha[max(0, bottom - 36) : bottom + 1] > ALPHA_CUT
    _by, bx = np.nonzero(band)
    if len(bx) < 8:
        return foot_anchor(img)
    # Ignore stray tail/wing tips at the far edges and track the main foot mass.
    low = float(np.percentile(bx, 35))
    high = float(np.percentile(bx, 85))
    main = bx[(bx >= low) & (bx <= high)]
    if len(main) < 8:
        main = bx
    return float(np.median(main)), float(bottom)


def bbox_center_bottom_anchor(img: Image.Image) -> tuple[float, float]:
    box = bounds(img)
    if box is None:
        return CELL / 2, CELL - 18
    l, _t, r, b = box
    return float((l + r) / 2.0), float(b)


def body_core_anchor(img: Image.Image) -> tuple[float, float]:
    arr = np.asarray(img.convert("RGBA"))
    alpha = arr[..., 3]
    box = bounds(img)
    if box is None:
        return CELL / 2, CELL / 2
    l, t, r, b = box
    height = max(1, b - t + 1)
    width = max(1, r - l + 1)
    y0 = t + int(height * 0.38)
    y1 = b + 1
    x0 = l + int(width * 0.12)
    x1 = r + 1 - int(width * 0.12)
    core = alpha[y0:y1, x0:x1] > ALPHA_CUT
    ys, xs = np.nonzero(core)
    if len(xs) < 12:
        return bbox_center_bottom_anchor(img)
    return float(x0 + np.median(xs)), float(y0 + np.median(ys))


def choose_anchors(mode: str, imgs: list[Image.Image]) -> list[tuple[float, float]]:
    if mode == "fixed_start_foot":
        # Preserve the source sheet's shared cell registration. Every frame is
        # translated as one intact image around the first frame's lower foot.
        anchor = foot_anchor(imgs[0])
        return [anchor for _img in imgs]
    if mode == "feet":
        measured = [foot_anchor(img) for img in imgs]
        stable_x = float(np.median([anchor[0] for anchor in measured]))
        return [(stable_x, anchor[1]) for anchor in measured]
    if mode == "feet_exact":
        return [foot_anchor(img) for img in imgs]
    if mode == "primary_feet_exact":
        return [primary_foot_anchor(img) for img in imgs]
    if mode == "fallen":
        anchors: list[tuple[float, float]] = []
        downed_start = max(5, len(imgs) // 2)
        for i, img in enumerate(imgs):
            if i >= downed_start:
                anchors.append(bbox_center_bottom_anchor(img))
            else:
                anchors.append(foot_anchor(img))
        return anchors
    if mode == "body_core":
        return [body_core_anchor(img) for img in imgs]
    if mode == "body_grounded":
        measured_x = [body_core_anchor(img)[0] for img in imgs]
        stable_x = float(np.median(measured_x))
        return [(stable_x, foot_anchor(img)[1]) for img in imgs]
    return [bbox_center_bottom_anchor(img) for img in imgs]


def repair_white_edge(img: Image.Image, frames: int) -> Image.Image:
    arr = zero_transparent_rgb(np.asarray(img.convert("RGBA")))
    out = arr.copy()
    for f in range(frames):
        x0 = f * CELL
        frame = arr[:, x0 : x0 + CELL]
        alpha = frame[..., 3]
        for y in range(1, CELL - 1):
            for x in range(1, CELL - 1):
                p = frame[y, x]
                if p[3] == 0 or p[3] > 225:
                    continue
                if not np.any(alpha[y - 1 : y + 2, x - 1 : x + 2] == 0):
                    continue
                neigh = frame[y - 1 : y + 2, x - 1 : x + 2]
                opaque = neigh[neigh[..., 3] > 210]
                if len(opaque) == 0:
                    continue
                neutral = max(abs(int(p[0]) - int(p[1])), abs(int(p[0]) - int(p[2])), abs(int(p[1]) - int(p[2])))
                bright = float(np.mean(p[:3]))
                local = np.mean(opaque[:, :3], axis=0)
                if bright > 150 and neutral < 44 and bright > float(np.mean(local)) + 30:
                    out[y, x0 + x, :3] = np.clip(local, 0, 255).astype(np.uint8)
    return Image.fromarray(zero_transparent_rgb(out), "RGBA")


def build(
    source_name: str,
    out_name: str,
    frames: list[int],
    max_w: int,
    max_h: int,
    mode: str,
    target_x: int = 128,
    target_y: int = 238,
) -> None:
    source = Image.open(SRC / f"SS_moonshadeStalker_{source_name}.png").convert("RGBA")
    raw = [remove_tiny_components(crop_frame(source, frame)) for frame in frames]
    boxes = [bounds(img) for img in raw]
    max_box_w = max((b[2] - b[0] + 1 for b in boxes if b), default=CELL)
    max_box_h = max((b[3] - b[1] + 1 for b in boxes if b), default=CELL)
    scale = min(max_w / max_box_w, max_h / max_box_h, 1.0)
    scale = max(0.35, scale)
    resized = [premultiplied_resize(img, (round(img.width * scale), round(img.height * scale))) for img in raw]
    anchors = choose_anchors(mode, resized)

    sheet = Image.new("RGBA", (CELL * len(frames), CELL), (0, 0, 0, 0))
    for i, img in enumerate(resized):
        ax, ay = anchors[i]
        dx = round(i * CELL + target_x - ax)
        dy = round(target_y - ay)
        box = bounds(img)
        if box:
            l, t, r, b = box
            if dx + l < i * CELL + 2:
                dx += i * CELL + 2 - (dx + l)
            if dx + r > i * CELL + CELL - 3:
                dx -= dx + r - (i * CELL + CELL - 3)
            if dy + t < 2:
                dy += 2 - (dy + t)
            if dy + b > CELL - 3:
                dy -= dy + b - (CELL - 3)
        sheet.alpha_composite(img, (dx, dy))

    arr = zero_transparent_rgb(np.asarray(sheet))
    for i in range(len(frames)):
        x = i * CELL
        arr[:, x] = 0
        arr[:, x + CELL - 1] = 0
        arr[0, x : x + CELL] = 0
        arr[CELL - 1, x : x + CELL] = 0
    sheet = repair_white_edge(Image.fromarray(arr, "RGBA"), len(frames))
    out = OUT / f"moonshadeStalker-{out_name}.png"
    sheet.save(out)
    print(out.name, sheet.size, "frames=" + ",".join(str(frame) for frame in frames))


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    build("walking", "walk", even_frames(2, 80, 24), 236, 228, "body_core", 128, 158)
    build("attack", "attack", even_frames(2, 80, 12), 236, 228, "feet")
    build(
        "damaged",
        "damaged",
        [2, 3, 4, 5, 6, 7, 8, 9, 10, 8, 5, 2],
        176,
        202,
        "fixed_start_foot",
        128,
        232,
    )
    build(
        "killed",
        "killed",
        [2, 3, 4, 6, 7, 8, 9, 10, 11, 13, 14, 15],
        224,
        222,
        "fixed_start_foot",
        128,
        232,
    )


if __name__ == "__main__":
    main()
