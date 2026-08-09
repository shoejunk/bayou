from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter


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


def center_anchor(img: Image.Image) -> tuple[float, float]:
    box = bounds(img)
    if box is None:
        return CELL / 2, CELL / 2
    l, t, r, b = box
    return (l + r) / 2.0, (t + b) / 2.0


def left_bottom_anchor(img: Image.Image) -> tuple[float, float]:
    box = bounds(img)
    if box is None:
        return CELL / 2, CELL - 18
    l, _t, _r, b = box
    return float(l), float(b)


def top_anchor(img: Image.Image) -> tuple[float, float]:
    arr = np.asarray(img.convert("RGBA"))
    alpha = arr[..., 3]
    ys, xs = np.nonzero(alpha > ALPHA_CUT)
    if len(xs) == 0:
        return CELL / 2, 40
    top = int(ys.min())
    upper = alpha[top : min(alpha.shape[0], top + 28)] > ALPHA_CUT
    _uy, ux = np.nonzero(upper)
    if len(ux) < 4:
        return float((xs.min() + xs.max()) / 2.0), float(top)
    return float(np.median(ux)), float(top)


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


def spell_effect(frame_index: int) -> Image.Image:
    effect = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    phase = frame_index / 11.0
    intensity = np.sin(np.pi * phase)
    if intensity <= 0.02:
        return effect

    cx = 128 + round(5 * np.sin(phase * np.pi * 1.4))
    cy = 116 - round(4 * np.sin(phase * np.pi))
    radius = 16 + round(22 * intensity)
    glow = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow, "RGBA")
    gd.ellipse((cx - radius, cy - radius, cx + radius, cy + radius), fill=(118, 28, 220, int(160 * intensity)))
    gd.ellipse((cx - radius // 2, cy - radius // 2, cx + radius // 2, cy + radius // 2), fill=(220, 105, 255, int(210 * intensity)))
    glow = glow.filter(ImageFilter.GaussianBlur(4))
    effect.alpha_composite(glow)

    d = ImageDraw.Draw(effect, "RGBA")
    arc_box = (cx - 35, cy - 29, cx + 36, cy + 30)
    d.arc(arc_box, start=210 - frame_index * 12, end=340 - frame_index * 12, fill=(238, 188, 255, int(230 * intensity)), width=3)
    d.arc((cx - 26, cy - 23, cx + 28, cy + 24), start=20 + frame_index * 15, end=140 + frame_index * 15, fill=(92, 225, 255, int(210 * intensity)), width=3)
    d.ellipse((cx - 5, cy - 5, cx + 6, cy + 6), fill=(238, 222, 255, int(235 * intensity)))
    for j in range(6):
        px = cx + round(np.cos(phase * 5.0 + j) * (12 + j * 3))
        py = cy + round(np.sin(phase * 4.0 + j * 1.7) * (8 + j * 2))
        a = int((80 + j * 18) * intensity)
        d.ellipse((px - 1, py - 1, px + 2, py + 2), fill=(194, 91, 255, a))
    return effect


def choose_anchors(mode: str, imgs: list[Image.Image]) -> list[tuple[float, float]]:
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
                anchors.append(center_anchor(img))
        return anchors
    if mode == "top":
        measured = [top_anchor(img) for img in imgs]
        stable_x = float(np.median([anchor[0] for anchor in measured]))
        stable_y = float(np.median([anchor[1] for anchor in measured]))
        return [(stable_x, stable_y) for _img in imgs]
    measured = [center_anchor(img) for img in imgs]
    stable_x = float(np.median([anchor[0] for anchor in measured]))
    stable_y = float(np.median([anchor[1] for anchor in measured]))
    return [(stable_x, stable_y) for _img in imgs]


def build(
    source_name: str,
    out_name: str,
    frames: list[int],
    max_w: int,
    max_h: int,
    mode: str = "center",
    target_x: int = 128,
    target_y: int = 128,
    add_spell: bool = False,
    bob_amplitude: int = 0,
) -> None:
    source = Image.open(SRC / f"SS_gloomFairy_{source_name}.png").convert("RGBA")
    raw = [remove_tiny_components(crop_frame(source, frame)) for frame in frames]
    boxes = [bounds(img) for img in raw]
    max_box_w = max((b[2] - b[0] + 1 for b in boxes if b), default=CELL)
    max_box_h = max((b[3] - b[1] + 1 for b in boxes if b), default=CELL)
    scale = min(max_w / max_box_w, max_h / max_box_h, 1.38)
    scale = max(0.75, scale)
    resized = [premultiplied_resize(img, (round(img.width * scale), round(img.height * scale))) for img in raw]
    anchors = choose_anchors(mode, resized)

    sheet = Image.new("RGBA", (CELL * len(frames), CELL), (0, 0, 0, 0))
    for i, img in enumerate(resized):
        ax, ay = anchors[i]
        dx = round(i * CELL + target_x - ax)
        bob = round(bob_amplitude * np.sin((2.0 * np.pi * i) / len(frames))) if bob_amplitude else 0
        dy = round(target_y - ay + bob)
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
        frame_canvas = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
        frame_canvas.alpha_composite(img, (dx - i * CELL, dy))
        if add_spell:
            frame_canvas.alpha_composite(spell_effect(i))
        sheet.alpha_composite(frame_canvas, (i * CELL, 0))

    arr = zero_transparent_rgb(np.asarray(sheet))
    for i in range(len(frames)):
        x = i * CELL
        arr[:, x] = 0
        arr[:, x + CELL - 1] = 0
        arr[0, x : x + CELL] = 0
        arr[CELL - 1, x : x + CELL] = 0
    sheet = repair_white_edge(Image.fromarray(arr, "RGBA"), len(frames))
    out = OUT / f"gloomFairy-{out_name}.png"
    sheet.save(out)
    print(out.name, sheet.size, "frames=" + ",".join(str(frame) for frame in frames))


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    build("idle", "idle", even_frames(4, 78, 24), 228, 222, bob_amplitude=12)
    build("attack", "attack", [2, 9, 16, 23, 30, 37, 44, 51, 58, 65, 72, 79], 216, 212, add_spell=True)
    build("damaged", "damaged", [6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 66, 72], 216, 212)
    build("killed", "killed", list(range(2, 14)), 238, 180, mode="top", target_x=172, target_y=48)


if __name__ == "__main__":
    main()
