using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;

class PngAlphaRepair
{
    const byte AlphaZero = 0;
    const int DefaultFrame = 256;
    const int AltFrame = 128;

    class Options
    {
        public string Mode = "scan";
        public string Root = Directory.GetCurrentDirectory();
        public string Report = "";
        public string PreviewDir = "";
        public string Include = "";
        public int PreviewLimit = 12;
        public bool SampleOnly = false;
        public bool AggressiveEdge = false;
    }

    class FileStats
    {
        public string Path = "";
        public bool Skipped;
        public string SkipReason = "";
        public int Width;
        public int Height;
        public int FrameCount;
        public int SuspiciousBefore;
        public int SuspiciousAfter;
        public int Changed;
        public int ZeroAlphaRgbFixed;
        public int OpaqueChanged;
        public bool Reopened;
        public bool Idempotent;
        public string BackupPath = "";
    }

    struct Region
    {
        public int X, Y, W, H;
    }

    static int Main(string[] args)
    {
        var opt = Parse(args);
        Directory.CreateDirectory(Path.Combine(opt.Root, "tools"));
        if (string.IsNullOrWhiteSpace(opt.Report))
            opt.Report = Path.Combine(opt.Root, "tools", "png-alpha-repair-report.csv");
        if (string.IsNullOrWhiteSpace(opt.PreviewDir))
            opt.PreviewDir = Path.Combine(opt.Root, "tools", "png-alpha-repair-previews");

        var files = FindTargets(opt.Root).ToList();
        if (!string.IsNullOrWhiteSpace(opt.Include))
            files = files.Where(f => Path.GetFileName(f).IndexOf(opt.Include, StringComparison.OrdinalIgnoreCase) >= 0).ToList();
        if (opt.SampleOnly)
            files = files.Take(8).ToList();

        var backupRoot = Path.Combine(opt.Root, "tools", "png-alpha-repair-backups", DateTime.Now.ToString("yyyyMMdd-HHmmss"));
        var stats = new List<FileStats>();
        int previewCount = 0;

        foreach (var file in files)
        {
            var s = ProcessFile(file, opt, backupRoot, ref previewCount);
            stats.Add(s);
            Console.WriteLine(string.Format("{0}: {1} suspicious {2}->{3}, changed {4}{5}",
                opt.Mode,
                Rel(opt.Root, file),
                s.SuspiciousBefore,
                s.SuspiciousAfter,
                s.Changed,
                s.Skipped ? " SKIPPED " + s.SkipReason : ""));
        }

        WriteReport(opt.Report, opt.Root, stats);

        int failed = stats.Count(s => !s.Skipped && (!s.Reopened || s.OpaqueChanged > 0 || (opt.Mode == "validate" && !s.Idempotent)));
        Console.WriteLine("Report: " + opt.Report);
        if (opt.Mode == "repair")
            Console.WriteLine("Backups: " + backupRoot);
        if (Directory.Exists(opt.PreviewDir))
            Console.WriteLine("Previews: " + opt.PreviewDir);
        return failed == 0 ? 0 : 2;
    }

    static Options Parse(string[] args)
    {
        var opt = new Options();
        for (int i = 0; i < args.Length; i++)
        {
            string a = args[i];
            if (a == "--mode" && i + 1 < args.Length) opt.Mode = args[++i].ToLowerInvariant();
            else if (a == "--root" && i + 1 < args.Length) opt.Root = Path.GetFullPath(args[++i]);
            else if (a == "--report" && i + 1 < args.Length) opt.Report = Path.GetFullPath(args[++i]);
            else if (a == "--preview-dir" && i + 1 < args.Length) opt.PreviewDir = Path.GetFullPath(args[++i]);
            else if (a == "--include" && i + 1 < args.Length) opt.Include = args[++i];
            else if (a == "--preview-limit" && i + 1 < args.Length) opt.PreviewLimit = int.Parse(args[++i]);
            else if (a == "--sample") opt.SampleOnly = true;
            else if (a == "--aggressive-edge") opt.AggressiveEdge = true;
        }
        if (opt.Mode != "scan" && opt.Mode != "repair" && opt.Mode != "validate")
            throw new ArgumentException("--mode must be scan, repair, or validate");
        return opt;
    }

    static IEnumerable<string> FindTargets(string root)
    {
        var dirs = new[]
        {
            Path.Combine(root, "assets", "animations")
        };

        foreach (var dir in dirs)
        {
            if (!Directory.Exists(dir)) continue;
            foreach (var file in Directory.EnumerateFiles(dir, "*.png", SearchOption.AllDirectories))
            {
                string lower = file.ToLowerInvariant();
                if (lower.Contains("\\build\\") || lower.Contains("\\_deps\\") || lower.Contains("\\tt temp\\"))
                    continue;
                yield return file;
            }
        }
    }

    static FileStats ProcessFile(string path, Options opt, string backupRoot, ref int previewCount)
    {
        var stats = new FileStats { Path = path };
        Bitmap input;
        try { input = LoadUnlocked(path); }
        catch (Exception e)
        {
            stats.Skipped = true;
            stats.SkipReason = "open failed: " + e.Message;
            return stats;
        }

        using (input)
        {
            stats.Width = input.Width;
            stats.Height = input.Height;
            var regions = InferRegions(path, input.Width, input.Height);
            if (regions.Count == 0)
            {
                stats.Skipped = true;
                stats.SkipReason = "could not infer safe frame boundaries";
                return stats;
            }
            stats.FrameCount = regions.Count;

            using (var rgba = ToArgb(input))
            using (var before = (Bitmap)rgba.Clone())
            {
                stats.SuspiciousBefore = CountSuspicious(rgba, regions, opt.AggressiveEdge);
                RepairBitmap(rgba, regions, stats, opt.AggressiveEdge);
                stats.SuspiciousAfter = CountSuspicious(rgba, regions, opt.AggressiveEdge);

                if ((stats.SuspiciousBefore > 0 || stats.Changed > 0) && previewCount < opt.PreviewLimit)
                {
                    Directory.CreateDirectory(opt.PreviewDir);
                    string baseName = SafeName(Path.GetFileNameWithoutExtension(path));
                    SavePreview(before, Path.Combine(opt.PreviewDir, baseName + "-before.png"), regions);
                    SavePreview(rgba, Path.Combine(opt.PreviewDir, baseName + "-after.png"), regions);
                    previewCount++;
                }

                stats.OpaqueChanged = CountOpaqueColorChanges(before, rgba);

                if (opt.Mode == "repair" && stats.Changed > 0)
                {
                    string backup = Path.Combine(backupRoot, Rel(opt.Root, path));
                    Directory.CreateDirectory(Path.GetDirectoryName(backup));
                    File.Copy(path, backup, true);
                    stats.BackupPath = backup;

                    string temp = path + ".tmp-alpha-repair.png";
                    rgba.Save(temp, ImageFormat.Png);
                    File.Delete(path);
                    File.Move(temp, path);
                }

                stats.Reopened = CanReopen(path);

                if (opt.Mode == "validate")
                {
                    using (var copy = (Bitmap)rgba.Clone())
                    {
                        var s2 = new FileStats();
                        RepairBitmap(copy, regions, s2, opt.AggressiveEdge);
                        stats.Idempotent = s2.Changed <= Math.Max(1, stats.Changed / 1000);
                    }
                }
                else
                {
                    stats.Idempotent = true;
                }
            }
        }
        return stats;
    }

    static Bitmap ToArgb(Bitmap source)
    {
        var copy = new Bitmap(source.Width, source.Height, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(copy))
        {
            g.Clear(Color.Transparent);
            g.DrawImageUnscaled(source, 0, 0);
        }
        return copy;
    }

    static Bitmap LoadUnlocked(string path)
    {
        using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
        using (var loaded = new Bitmap(fs))
        {
            return loaded.Clone(new Rectangle(0, 0, loaded.Width, loaded.Height), PixelFormat.Format32bppArgb);
        }
    }

    static List<Region> InferRegions(string path, int w, int h)
    {
        var regions = new List<Region>();
        string lower = Path.GetFileName(path).ToLowerInvariant();

        int frame = 0;
        if (w % DefaultFrame == 0 && h == DefaultFrame) frame = DefaultFrame;
        else if (h % DefaultFrame == 0 && w == DefaultFrame) frame = DefaultFrame;
        else if (w % AltFrame == 0 && h == AltFrame) frame = AltFrame;
        else if (h % AltFrame == 0 && w == AltFrame) frame = AltFrame;
        else if (w % DefaultFrame == 0 && h % DefaultFrame == 0 && lower.Contains("animation")) frame = DefaultFrame;
        else if (w % AltFrame == 0 && h % AltFrame == 0 && lower.Contains("animation")) frame = AltFrame;

        if (frame == 0)
        {
            if (lower.Contains("screenshot") || lower.Contains("preview"))
                regions.Add(new Region { X = 0, Y = 0, W = w, H = h });
            return regions;
        }

        for (int y = 0; y < h; y += frame)
            for (int x = 0; x < w; x += frame)
                regions.Add(new Region { X = x, Y = y, W = frame, H = frame });
        return regions;
    }

    static void RepairBitmap(Bitmap bmp, List<Region> regions, FileStats stats, bool aggressiveEdge)
    {
        foreach (var r in regions)
        {
            for (int pass = 0; pass < 4; pass++)
            {
                int passChanges = 0;
                var suspicious = FindSuspicious(bmp, r, aggressiveEdge);
                foreach (var p in suspicious)
                {
                    var old = bmp.GetPixel(p.X, p.Y);
                    if (old.A <= 16)
                    {
                        bmp.SetPixel(p.X, p.Y, Color.Transparent);
                        stats.Changed++;
                        passChanges++;
                        continue;
                    }
                    var replacement = NeighborColor(bmp, r, p.X, p.Y);
                    if (!replacement.HasValue) continue;
                    var nr = replacement.Value;
                    var next = Color.FromArgb(old.A, nr.R, nr.G, nr.B);
                    if (next.ToArgb() != old.ToArgb())
                    {
                        bmp.SetPixel(p.X, p.Y, next);
                        stats.Changed++;
                        passChanges++;
                    }
                }
                if (passChanges == 0)
                    break;
            }

            for (int y = r.Y; y < r.Y + r.H; y++)
                for (int x = r.X; x < r.X + r.W; x++)
                {
                    var c = bmp.GetPixel(x, y);
                    if (c.A == AlphaZero && (c.R != 0 || c.G != 0 || c.B != 0))
                    {
                        bmp.SetPixel(x, y, Color.FromArgb(0, 0, 0, 0));
                        stats.Changed++;
                        stats.ZeroAlphaRgbFixed++;
                    }
                }
        }
    }

    static List<Point> FindSuspicious(Bitmap bmp, Region r, bool aggressiveEdge)
    {
        var pts = new List<Point>();
        for (int y = r.Y; y < r.Y + r.H; y++)
            for (int x = r.X; x < r.X + r.W; x++)
            {
                var c = bmp.GetPixel(x, y);
                if (c.A == 0 || (!aggressiveEdge && c.A == 255))
                    continue;
                if (!IsBrightNeutral(c))
                    continue;
                if (!(c.A < 220 || aggressiveEdge) || !AdjacentToTransparency(bmp, r, x, y))
                    continue;
                var neighbor = NeighborColor(bmp, r, x, y);
                if (!neighbor.HasValue)
                    continue;
                var n = neighbor.Value;
                int cb = (c.R + c.G + c.B) / 3;
                int nb = (n.R + n.G + n.B) / 3;
                if (cb - nb >= 45)
                    pts.Add(new Point(x, y));
            }
        return pts;
    }

    static int CountSuspicious(Bitmap bmp, List<Region> regions, bool aggressiveEdge)
    {
        int count = 0;
        foreach (var r in regions)
            count += FindSuspicious(bmp, r, aggressiveEdge).Count;
        return count;
    }

    static bool IsBrightNeutral(Color c)
    {
        int max = Math.Max(c.R, Math.Max(c.G, c.B));
        int min = Math.Min(c.R, Math.Min(c.G, c.B));
        return max >= 190 && max - min <= 42;
    }

    static bool AdjacentToTransparency(Bitmap bmp, Region r, int x, int y)
    {
        for (int yy = Math.Max(r.Y, y - 1); yy <= Math.Min(r.Y + r.H - 1, y + 1); yy++)
            for (int xx = Math.Max(r.X, x - 1); xx <= Math.Min(r.X + r.W - 1, x + 1); xx++)
                if ((xx != x || yy != y) && bmp.GetPixel(xx, yy).A == 0)
                    return true;
        return false;
    }

    static Color? NeighborColor(Bitmap bmp, Region r, int x, int y)
    {
        long rr = 0, gg = 0, bb = 0, weight = 0;
        for (int radius = 1; radius <= 5; radius++)
        {
            rr = gg = bb = weight = 0;
            for (int yy = Math.Max(r.Y, y - radius); yy <= Math.Min(r.Y + r.H - 1, y + radius); yy++)
                for (int xx = Math.Max(r.X, x - radius); xx <= Math.Min(r.X + r.W - 1, x + radius); xx++)
                {
                    if (xx == x && yy == y) continue;
                    var c = bmp.GetPixel(xx, yy);
                    if (c.A < 170 || IsBrightNeutral(c)) continue;
                    int w = c.A;
                    rr += c.R * w;
                    gg += c.G * w;
                    bb += c.B * w;
                    weight += w;
                }
            if (weight > 0)
                return Color.FromArgb((int)(rr / weight), (int)(gg / weight), (int)(bb / weight));
        }
        return null;
    }

    static int CountOpaqueColorChanges(Bitmap before, Bitmap after)
    {
        int changed = 0;
        for (int y = 0; y < before.Height; y++)
            for (int x = 0; x < before.Width; x++)
            {
                var a = before.GetPixel(x, y);
                var b = after.GetPixel(x, y);
                if (a.A == 255 && (a.R != b.R || a.G != b.G || a.B != b.B || b.A != 255))
                    changed++;
            }
        return changed;
    }

    static bool CanReopen(string path)
    {
        try
        {
            using (var b = new Bitmap(path))
                return b.PixelFormat == PixelFormat.Format32bppArgb || Image.IsAlphaPixelFormat(b.PixelFormat);
        }
        catch { return false; }
    }

    static void SavePreview(Bitmap source, string path, List<Region> regions)
    {
        var sample = regions.Take(Math.Min(6, regions.Count)).ToList();
        if (sample.Count == 0) return;
        Color[] backgrounds =
        {
            Color.Black,
            Color.FromArgb(36, 20, 58),
            Color.FromArgb(0, 220, 60),
            Color.FromArgb(128, 128, 128)
        };
        int cell = 96;
        using (var preview = new Bitmap(sample.Count * cell, backgrounds.Length * cell, PixelFormat.Format32bppArgb))
        using (var g = Graphics.FromImage(preview))
        {
            g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.NearestNeighbor;
            g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.Half;
            for (int row = 0; row < backgrounds.Length; row++)
            {
                using (var brush = new SolidBrush(backgrounds[row]))
                    g.FillRectangle(brush, 0, row * cell, preview.Width, cell);
                for (int col = 0; col < sample.Count; col++)
                {
                    var r = sample[col];
                    g.DrawImage(source,
                        new Rectangle(col * cell, row * cell, cell, cell),
                        new Rectangle(r.X, r.Y, r.W, r.H),
                        GraphicsUnit.Pixel);
                }
            }
            preview.Save(path, ImageFormat.Png);
        }
    }

    // Correct premultiplied-alpha nearest/bilinear-safe resize utility for future exporters.
    // This tool does not need to resize repaired assets, but generated pipelines should use
    // this pattern instead of interpolating straight-alpha RGBA pixels.
    public static Bitmap ResizePremultipliedAlpha(Bitmap source, int width, int height)
    {
        var premul = new Bitmap(source.Width, source.Height, PixelFormat.Format32bppArgb);
        for (int y = 0; y < source.Height; y++)
            for (int x = 0; x < source.Width; x++)
            {
                var c = source.GetPixel(x, y);
                float a = c.A / 255f;
                premul.SetPixel(x, y, Color.FromArgb(c.A, (int)(c.R * a), (int)(c.G * a), (int)(c.B * a)));
            }

        var resized = new Bitmap(width, height, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(resized))
        {
            g.Clear(Color.Transparent);
            g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
            g.DrawImage(premul, new Rectangle(0, 0, width, height));
        }
        premul.Dispose();

        for (int y = 0; y < resized.Height; y++)
            for (int x = 0; x < resized.Width; x++)
            {
                var c = resized.GetPixel(x, y);
                if (c.A == 0)
                {
                    resized.SetPixel(x, y, Color.Transparent);
                    continue;
                }
                float invA = 255f / c.A;
                resized.SetPixel(x, y, Color.FromArgb(c.A, Clamp(c.R * invA), Clamp(c.G * invA), Clamp(c.B * invA)));
            }
        return resized;
    }

    static int Clamp(float v)
    {
        return Math.Max(0, Math.Min(255, (int)Math.Round(v)));
    }

    static void WriteReport(string path, string root, List<FileStats> stats)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path));
        using (var w = new StreamWriter(path))
        {
            w.WriteLine("file,skipped,skip_reason,width,height,frames,suspicious_before,suspicious_after,changed,zero_alpha_rgb_fixed,opaque_changed,reopened,idempotent,backup");
            foreach (var s in stats)
                w.WriteLine(string.Join(",",
                    Csv(Rel(root, s.Path)),
                    s.Skipped ? "true" : "false",
                    Csv(s.SkipReason),
                    s.Width,
                    s.Height,
                    s.FrameCount,
                    s.SuspiciousBefore,
                    s.SuspiciousAfter,
                    s.Changed,
                    s.ZeroAlphaRgbFixed,
                    s.OpaqueChanged,
                    s.Reopened ? "true" : "false",
                    s.Idempotent ? "true" : "false",
                    Csv(string.IsNullOrEmpty(s.BackupPath) ? "" : Rel(root, s.BackupPath))));
        }
    }

    static string Rel(string root, string path)
    {
        Uri r = new Uri(Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar);
        Uri p = new Uri(Path.GetFullPath(path));
        return Uri.UnescapeDataString(r.MakeRelativeUri(p).ToString()).Replace('/', Path.DirectorySeparatorChar);
    }

    static string SafeName(string name)
    {
        foreach (var c in Path.GetInvalidFileNameChars())
            name = name.Replace(c, '_');
        return name;
    }

    static string Csv(string value)
    {
        if (value == null) return "";
        return "\"" + value.Replace("\"", "\"\"") + "\"";
    }
}
