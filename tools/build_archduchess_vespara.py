from pathlib import Path

import numpy as np
from PIL import Image


CELL = 256
GRID = 9
ALPHA_CUT = 10


ROOT = Path(r"C:\Users\jarox\OneDrive\Desktop\Tandem Tales\Bayou Bonanza")
SRC = Path(r"C:\Users\jarox\OneDrive\Desktop\TT Temp")
OUT = ROOT / "assets" / "animations"


def even_frames(start: int, end: int, count: int) -> list[int]:
    vals = np.linspace(start, end, count)
    out = []
    last = 0
    for v in vals:
        n = int(round(float(v)))
        if n <= last:
            n = last + 1
        out.append(n)
        last = n
    return out


def zero_transparent_rgb(arr: np.ndarray) -> np.ndarray:
    arr = arr.copy()
    arr[arr[..., 3] <= ALPHA_CUT] = 0
    arr[(arr[..., 3] == 0), :3] = 0
    return arr


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
    a = np.asarray(img)[..., 3]
    ys, xs = np.nonzero(a > ALPHA_CUT)
    if len(xs) == 0:
        return None
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def remove_tiny_components(img: Image.Image, min_pixels: int = 14) -> Image.Image:
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
            pts = []
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


def anchor(img: Image.Image, mode: str) -> tuple[float, float]:
    arr = np.asarray(img)
    box = bounds(img)
    if box is None:
        return CELL / 2, CELL - 18
    l, t, r, b = box
    a = arr[..., 3]
    if mode == "foot":
        min_x = int(l + (r - l + 1) * 0.30)
        max_x = int(l + (r - l + 1) * 0.70)
        region = a[:, max(0, min_x) : min(a.shape[1], max_x + 1)] > 80
        ys, xs = np.nonzero(region)
        if len(ys):
            bottom = int(ys.max())
            contact = region[max(0, bottom - 8) : bottom + 1, :]
            cys, cxs = np.nonzero(contact)
            if len(cxs):
                return float(np.median(cxs + max(0, min_x))), float(bottom)
        return (l + r) / 2.0, float(b)
    return (l + r) / 2.0, float(b)


def repair_white_edge(img: Image.Image, frames: int) -> Image.Image:
    arr = zero_transparent_rgb(np.asarray(img.convert("RGBA")))
    out = arr.copy()
    h, w, _ = arr.shape
    for f in range(frames):
        x0 = f * CELL
        x1 = x0 + CELL
        frame = arr[:, x0:x1]
        alpha = frame[..., 3]
        for y in range(1, h - 1):
            for x in range(1, CELL - 1):
                p = frame[y, x]
                if p[3] == 0 or p[3] > 225:
                    continue
                near_clear = np.any(alpha[y - 1 : y + 2, x - 1 : x + 2] == 0)
                if not near_clear:
                    continue
                neigh = frame[y - 1 : y + 2, x - 1 : x + 2]
                opaque = neigh[neigh[..., 3] > 210]
                if len(opaque) == 0:
                    continue
                neutral = max(abs(int(p[0]) - int(p[1])), abs(int(p[0]) - int(p[2])), abs(int(p[1]) - int(p[2])))
                bright = float(np.mean(p[:3]))
                local = np.mean(opaque[:, :3], axis=0)
                if bright > 150 and neutral < 38 and bright > float(np.mean(local)) + 38:
                    out[y, x0 + x, :3] = np.clip(local, 0, 255).astype(np.uint8)
    return Image.fromarray(zero_transparent_rgb(out), "RGBA")


def build(name: str, frames: list[int], target_x: int, target_y: int, max_w: int, max_h: int, mode: str) -> None:
    sheet = Image.open(SRC / f"SS_archduchessVespara_{name}.png").convert("RGBA")
    raw = [remove_tiny_components(crop_frame(sheet, n)) for n in frames]
    boxes = [bounds(img) for img in raw]
    max_box_w = max((b[2] - b[0] + 1 for b in boxes if b), default=CELL)
    max_box_h = max((b[3] - b[1] + 1 for b in boxes if b), default=CELL)
    scale = min(max_w / max_box_w, max_h / max_box_h, 1.2)
    scale = max(0.55, scale)
    resized = [premultiplied_resize(img, (int(round(img.width * scale)), int(round(img.height * scale)))) for img in raw]
    anchors = [anchor(img, mode) for img in resized]
    stable_x = float(np.median([a[0] for a in anchors]))
    dest = Image.new("RGBA", (CELL * len(frames), CELL), (0, 0, 0, 0))
    for i, img in enumerate(resized):
        ax, ay = anchors[i]
        dx = int(round(i * CELL + target_x - stable_x))
        dy = int(round(target_y - ay))
        b = bounds(img)
        if b:
            l, t, r, bb = b
            if dx + l < i * CELL + 2:
                dx += i * CELL + 2 - (dx + l)
            if dx + r > i * CELL + CELL - 3:
                dx -= dx + r - (i * CELL + CELL - 3)
            if dy + t < 2:
                dy += 2 - (dy + t)
            if dy + bb > CELL - 3:
                dy -= dy + bb - (CELL - 3)
        dest.alpha_composite(img, (dx, dy))
        arr = np.asarray(dest).copy()
        arr[:, i * CELL, :] = 0
        arr[:, i * CELL + CELL - 1, :] = 0
        arr[0, i * CELL : i * CELL + CELL, :] = 0
        arr[CELL - 1, i * CELL : i * CELL + CELL, :] = 0
        dest = Image.fromarray(arr, "RGBA")
    dest = repair_white_edge(dest, len(frames))
    out_name = "walk" if name == "walking" else name
    out = OUT / f"archduchessVespara-{out_name}.png"
    dest.save(out)
    print(out.name, dest.size, "frames=" + ",".join(map(str, frames)))


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    build("walking", even_frames(2, 80, 24), 128, 238, 236, 236, "foot")
    build("attack", [2, 8, 13, 16, 19, 22, 26, 32, 40, 50, 60, 70], 128, 238, 252, 236, "foot")
    build("damaged", [2, 8, 14, 20, 26, 32, 38, 44, 50, 56, 62, 68], 128, 238, 238, 236, "foot")
    build("killed", [2, 6, 10, 14, 18, 22, 26, 30, 34, 38, 42, 46], 128, 238, 252, 236, "center")


if __name__ == "__main__":
    main()
