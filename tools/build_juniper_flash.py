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
    frames = []
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
    return Image.fromarray(zero_transparent_rgb(np.dstack([np.clip(rgb, 0, 255), alpha_resized]).astype(np.uint8)), "RGBA")


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


def remove_tiny_components(img: Image.Image, min_pixels: int = 16) -> Image.Image:
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


def lower_foot_anchor(img: Image.Image) -> tuple[float, float]:
    box = bounds(img)
    if box is None:
        return CELL / 2, CELL - 18
    l, _t, r, b = box
    alpha = np.asarray(img)[..., 3]
    min_x = int(l + (r - l + 1) * 0.20)
    max_x = int(l + (r - l + 1) * 0.82)
    region = alpha[:, max(0, min_x) : min(alpha.shape[1], max_x + 1)] > 80
    ys, xs = np.nonzero(region)
    if len(ys) == 0:
        return (l + r) / 2.0, float(b)
    bottom = int(np.percentile(ys, 99.5))
    contact = region[max(0, bottom - 8) : bottom + 1, :]
    _cys, cxs = np.nonzero(contact)
    x = float(np.median(cxs + max(0, min_x))) if len(cxs) else (l + r) / 2.0
    return x, float(bottom)


def high_foot_anchor(img: Image.Image) -> tuple[float, float]:
    box = bounds(img)
    if box is None:
        return CELL / 2, 152.0
    l, t, r, b = box
    alpha = np.asarray(img)[..., 3]
    yy, xx = np.indices(alpha.shape)
    lower_body = (alpha > 80) & (yy >= max(t, 145)) & (yy <= min(b, 252)) & (xx >= l + (r - l + 1) * 0.18) & (xx <= l + (r - l + 1) * 0.86)
    ys, xs = np.nonzero(lower_body)
    if len(ys) == 0:
        return (l + r) / 2.0, 152.0
    y = int(np.percentile(ys, 3))
    band = lower_body & (yy >= y) & (yy <= y + 12)
    _bys, bxs = np.nonzero(band)
    x = float(np.median(bxs)) if len(bxs) else float(np.median(xs))
    return x, float(y)


def attack_lower_foot_anchor(img: Image.Image) -> tuple[float, float]:
    box = bounds(img)
    if box is None:
        return CELL / 2, 236.0
    l, t, r, b = box
    alpha = np.asarray(img)[..., 3]
    yy, xx = np.indices(alpha.shape)
    lower_body = (alpha > 80) & (yy >= max(t, 170)) & (yy <= min(b, 253)) & (xx >= l + (r - l + 1) * 0.12) & (xx <= l + (r - l + 1) * 0.82)
    ys, xs = np.nonzero(lower_body)
    if len(ys) == 0:
        return (l + r) / 2.0, float(b)
    y = int(np.percentile(ys, 99))
    band = lower_body & (yy >= y - 10) & (yy <= y)
    _bys, bxs = np.nonzero(band)
    x = float(np.median(bxs)) if len(bxs) else float(np.median(xs))
    return x, float(y)


def center_bottom_anchor(img: Image.Image) -> tuple[float, float]:
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
                if bright > 150 and neutral < 38 and bright > float(np.mean(local)) + 38:
                    out[y, x0 + x, :3] = np.clip(local, 0, 255).astype(np.uint8)
    return Image.fromarray(zero_transparent_rgb(out), "RGBA")


def build(name: str, frames: list[int], max_w: int, max_h: int, mode: str, target_x: int = 128, target_y: int = 238) -> None:
    source = Image.open(SRC / f"SS_juniperFlash_{name}.png").convert("RGBA")
    raw = [remove_tiny_components(crop_frame(source, frame)) for frame in frames]
    boxes = [bounds(img) for img in raw]
    max_box_w = max((b[2] - b[0] + 1 for b in boxes if b), default=CELL)
    max_box_h = max((b[3] - b[1] + 1 for b in boxes if b), default=CELL)
    scale = min(max_w / max_box_w, max_h / max_box_h, 1.18)
    scale = max(0.62, scale)
    resized = [premultiplied_resize(img, (round(img.width * scale), round(img.height * scale))) for img in raw]
    if mode == "fixedcrop":
        sheet = Image.new("RGBA", (CELL * len(frames), CELL), (0, 0, 0, 0))
        dx0 = round((CELL - resized[0].width) / 2)
        dy0 = round((CELL - resized[0].height) / 2) + target_y
        for i, img in enumerate(resized):
            sheet.alpha_composite(img, (i * CELL + dx0, dy0))
        arr = zero_transparent_rgb(np.asarray(sheet))
        for i in range(len(frames)):
            x = i * CELL
            arr[:, x] = 0
            arr[:, x + CELL - 1] = 0
            arr[0, x : x + CELL] = 0
            arr[CELL - 1, x : x + CELL] = 0
        sheet = repair_white_edge(Image.fromarray(arr, "RGBA"), len(frames))
        out = OUT / f"juniperFlash-walk.png"
        sheet.save(out)
        print(out.name, sheet.size, "frames=" + ",".join(str(frame) for frame in frames))
        return
    if mode == "center":
        anchors = [center_bottom_anchor(img) for img in resized]
    elif mode == "highfootlock":
        anchors = [high_foot_anchor(img) for img in resized]
    elif mode == "attacklowerfootlock":
        anchors = [attack_lower_foot_anchor(img) for img in resized]
    else:
        anchors = [lower_foot_anchor(img) for img in resized]
    stable_x = float(np.median([anchor[0] for anchor in anchors]))
    if mode in ("footlock", "highfootlock", "attacklowerfootlock"):
        stable_x = float(np.median([anchor[0] for anchor in anchors]))
    sheet = Image.new("RGBA", (CELL * len(frames), CELL), (0, 0, 0, 0))
    for i, img in enumerate(resized):
        ax, ay = anchors[i]
        dx = round(i * CELL + target_x - stable_x)
        if mode in ("footlock", "highfootlock", "attacklowerfootlock"):
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
    out_name = "walk" if name == "walking" else name
    out = OUT / f"juniperFlash-{out_name}.png"
    sheet.save(out)
    print(out.name, sheet.size, "frames=" + ",".join(str(frame) for frame in frames))


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    build("walking", even_frames(2, 80, 24), 222, 226, "fixedcrop", target_y=8)
    build("attack", [2, 8, 14, 21, 28, 31, 33, 35, 38, 41, 50, 62], 226, 232, "attacklowerfootlock", target_x=78, target_y=236)
    build("damaged", [2, 8, 14, 20, 26, 32, 38, 44, 50, 56, 62, 68], 205, 226, "footlock", target_x=130)
    build("killed", [2, 6, 10, 14, 18, 22, 26, 30, 34, 38, 42, 45], 252, 236, "center", target_x=128)


if __name__ == "__main__":
    main()
