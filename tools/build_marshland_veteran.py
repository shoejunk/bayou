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


def even_frames(start: int, end: int, count: int, endpoint: bool = False) -> list[int]:
    frames: list[int] = []
    last = 0
    for value in np.linspace(start, end, count, endpoint=endpoint):
        frame = int(round(float(value)))
        if frame <= last:
            frame = last + 1
        frames.append(frame)
        last = frame
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


def remove_tiny_components(img: Image.Image, min_pixels: int = 18) -> Image.Image:
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


def bottom_component_anchor(img: Image.Image) -> tuple[float, float]:
    box = bounds(img)
    if box is None:
        return CELL / 2, CELL - 18
    l, t, r, b = box
    alpha = np.asarray(img)[..., 3]
    yy, xx = np.indices(alpha.shape)
    mask = (
        (alpha > 80)
        & (yy >= max(t, int(t + (b - t + 1) * 0.56)))
        & (xx >= l + (r - l + 1) * 0.08)
        & (xx <= l + (r - l + 1) * 0.92)
    )
    h, w = mask.shape
    seen = np.zeros_like(mask, dtype=bool)
    components: list[tuple[int, int, np.ndarray, np.ndarray]] = []
    for sy, sx in zip(*np.nonzero(mask)):
        if seen[sy, sx]:
            continue
        stack = [(int(sx), int(sy))]
        seen[sy, sx] = True
        pts: list[tuple[int, int]] = []
        while stack:
            x, y = stack.pop()
            pts.append((x, y))
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < w and 0 <= ny < h and mask[ny, nx] and not seen[ny, nx]:
                    seen[ny, nx] = True
                    stack.append((nx, ny))
        if len(pts) >= 8:
            px = np.array([p[0] for p in pts])
            py = np.array([p[1] for p in pts])
            components.append((int(py.max()), len(pts), px, py))
    if not components:
        return (l + r) / 2.0, float(b)
    components.sort(key=lambda c: (c[0], c[1]), reverse=True)
    bottom, _size, px, py = components[0]
    band = py >= bottom - 8
    return float(np.median(px[band])), float(bottom)


def center_bottom_anchor(img: Image.Image) -> tuple[float, float]:
    box = bounds(img)
    if box is None:
        return CELL / 2, CELL - 18
    l, _t, r, b = box
    return (l + r) / 2.0, float(b)


def fixed_center_anchor(img: Image.Image) -> tuple[float, float]:
    box = bounds(img)
    if box is None:
        return CELL / 2, CELL - 18
    l, t, r, b = box
    return (l + r) / 2.0, float(t + (b - t + 1) * 0.80)


def fixed_center_bottom_anchor(img: Image.Image) -> tuple[float, float]:
    box = bounds(img)
    if box is None:
        return CELL / 2, CELL - 18
    l, _t, r, b = box
    return (l + r) / 2.0, float(b)


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
                if bright > 150 and neutral < 40 and bright > float(np.mean(local)) + 35:
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
    source = Image.open(SRC / f"SS_marshlandVeteran_{source_name}.png").convert("RGBA")
    raw = [remove_tiny_components(crop_frame(source, frame)) for frame in frames]
    boxes = [bounds(img) for img in raw]
    max_box_w = max((b[2] - b[0] + 1 for b in boxes if b), default=CELL)
    max_box_h = max((b[3] - b[1] + 1 for b in boxes if b), default=CELL)
    scale = min(max_w / max_box_w, max_h / max_box_h, 1.16)
    scale = max(0.58, scale)
    resized = [premultiplied_resize(img, (round(img.width * scale), round(img.height * scale))) for img in raw]
    if mode == "fixed":
        anchors = [fixed_center_anchor(img) for img in resized]
        stable_x = float(np.median([anchor[0] for anchor in anchors]))
        anchors = [(stable_x, anchor[1]) for anchor in anchors]
    elif mode == "fixedbottom":
        anchors = [fixed_center_bottom_anchor(img) for img in resized]
        stable_x = float(np.median([anchor[0] for anchor in anchors]))
        anchors = [(stable_x, anchor[1]) for anchor in anchors]
    elif mode == "center":
        anchors = [center_bottom_anchor(img) for img in resized]
    else:
        anchors = [bottom_component_anchor(img) for img in resized]
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
    out = OUT / f"marshlandVeteran-{out_name}.png"
    sheet.save(out)
    print(out.name, sheet.size, "frames=" + ",".join(str(frame) for frame in frames))


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    build("walking", "walk", [28, 30, 32, 34, 35, 37, 39, 41, 43, 44, 46, 48, 50, 52, 54, 56, 57, 59, 61, 63, 65, 66, 68, 70], 226, 232, "fixed", target_x=128, target_y=220)
    build("attack", "attack", even_frames(2, 68, 12, endpoint=True), 238, 232, "bottomfoot", target_x=88, target_y=236)
    build("damaged", "damaged", even_frames(2, 68, 12, endpoint=True), 188, 202, "fixedbottom", target_x=128, target_y=236)
    build("killed", "killed", [2, 7, 12, 17, 22, 27, 31, 35, 38, 41, 43, 45], 252, 236, "center", target_x=128, target_y=238)


if __name__ == "__main__":
    main()
