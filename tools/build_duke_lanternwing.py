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


def even_frames(start: int, end: int, count: int, endpoint: bool = True) -> list[int]:
    frames: list[int] = []
    used: set[int] = set()
    for value in np.linspace(start, end, count, endpoint=endpoint):
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


def crop_frame(sheet: Image.Image, n: int) -> Image.Image:
    fw, fh = sheet.width // GRID, sheet.height // GRID
    idx = n - 1
    x = (idx % GRID) * fw
    y = (idx // GRID) * fh
    return sheet.crop((x, y, x + fw, y + fh)).convert("RGBA")


def bounds(img: Image.Image) -> tuple[int, int, int, int] | None:
    alpha = np.asarray(img)[..., 3]
    ys, xs = np.nonzero(alpha > ALPHA_CUT)
    if len(xs) == 0:
        return None
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def remove_tiny_components(img: Image.Image, min_pixels: int = 20) -> Image.Image:
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


def silhouette_anchor(img: Image.Image) -> tuple[float, float]:
    box = bounds(img)
    if box is None:
        return CELL / 2, CELL - 18
    l, t, r, b = box
    return (l + r) / 2.0, (t + b) / 2.0


def lower_body_anchor(img: Image.Image) -> tuple[float, float]:
    arr = np.asarray(img.convert("RGBA"))
    alpha = arr[..., 3]
    ys, xs = np.nonzero(alpha > ALPHA_CUT)
    if len(xs) == 0:
        return CELL / 2, CELL / 2
    bottom = int(ys.max())
    lower = alpha[max(0, bottom - 20) : bottom + 1] > ALPHA_CUT
    _fy, fx = np.nonzero(lower)
    if len(fx) < 4:
        return float((xs.min() + xs.max()) / 2.0), float(bottom)
    return float(np.median(fx)), float(bottom)


def left_bottom_anchor(img: Image.Image) -> tuple[float, float]:
    box = bounds(img)
    if box is None:
        return CELL / 2, CELL - 18
    l, _t, _r, b = box
    return float(l), float(b)


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


def choose_anchors(mode: str, imgs: list[Image.Image]) -> list[tuple[float, float]]:
    if mode == "center":
        measured = [silhouette_anchor(img) for img in imgs]
        stable_x = float(np.median([anchor[0] for anchor in measured]))
        stable_y = float(np.median([anchor[1] for anchor in measured]))
        return [(stable_x, stable_y) for _img in imgs]
    if mode == "lower":
        measured = [lower_body_anchor(img) for img in imgs]
        stable_x = float(np.median([anchor[0] for anchor in measured]))
        stable_y = float(np.median([anchor[1] for anchor in measured]))
        return [(stable_x, stable_y) for _img in imgs]
    if mode == "fallen":
        downed_start = max(5, len(imgs) // 2)
        measured = [left_bottom_anchor(img) for img in imgs[downed_start:]]
        stable_left = float(np.median([anchor[0] for anchor in measured]))
        anchors: list[tuple[float, float]] = []
        for i, img in enumerate(imgs):
            if i >= downed_start:
                _ax, ay = left_bottom_anchor(img)
                anchors.append((stable_left, ay))
            else:
                anchors.append(lower_body_anchor(img))
        return anchors
    return [silhouette_anchor(img) for img in imgs]


def build(
    source_name: str,
    out_name: str,
    frames: list[int],
    max_w: int,
    max_h: int,
    mode: str,
    target_x: int,
    target_y: int,
) -> None:
    source = Image.open(SRC / f"SS_dukeLanternwing_{source_name}.png").convert("RGBA")
    raw = [remove_tiny_components(crop_frame(source, frame)) for frame in frames]
    boxes = [bounds(img) for img in raw]
    max_box_w = max((b[2] - b[0] + 1 for b in boxes if b), default=CELL)
    max_box_h = max((b[3] - b[1] + 1 for b in boxes if b), default=CELL)
    scale = min(max_w / max_box_w, max_h / max_box_h, 1.0)
    scale = max(0.42, scale)
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
    out = OUT / f"dukeLanternwing-{out_name}.png"
    sheet.save(out)
    print(out.name, sheet.size, "frames=" + ",".join(str(frame) for frame in frames))


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    build("idle", "idle", even_frames(4, 78, 24), 236, 236, "center", 128, 128)
    build("attack", "attack", [8, 12, 17, 21, 25, 30, 36, 40, 44, 48, 52, 56], 246, 236, "center", 128, 130)
    build("damaged", "damaged", [5, 10, 15, 20, 26, 32, 38, 44, 50, 56, 62, 68], 232, 236, "lower", 128, 240)
    build("killed", "killed", [2, 6, 10, 15, 20, 25, 30, 36, 42, 48, 48, 48], 246, 214, "fallen", 12, 238)


if __name__ == "__main__":
    main()
