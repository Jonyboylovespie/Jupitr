using System.Drawing;
using System.Drawing.Drawing2D;

namespace JupitrApp;

public static class JupiterTheme
{
    // Backgrounds
    public static readonly Color DarkBg = Color.FromArgb(26, 15, 10);      // #1A0F0A
    public static readonly Color CardBg = Color.FromArgb(45, 31, 22);      // #2D1F16
    public static readonly Color CardBgAlt = Color.FromArgb(55, 39, 28);   // #37271C
    public static readonly Color HoverBg = Color.FromArgb(70, 50, 35);     // #463223

    // Accents
    public static readonly Color Orange = Color.FromArgb(255, 107, 53);    // #FF6B35
    public static readonly Color OrangeLight = Color.FromArgb(255, 140, 66); // #FF8C42
    public static readonly Color Yellow = Color.FromArgb(255, 210, 63);   // #FFD23F
    public static readonly Color YellowLight = Color.FromArgb(255, 230, 120);

    // Text
    public static readonly Color Cream = Color.FromArgb(255, 248, 231);   // #FFF8E7
    public static readonly Color Muted = Color.FromArgb(184, 169, 154);   // #B8A99A

    public static readonly Font FontHeader = new("Segoe UI", 11, FontStyle.Bold);
    public static readonly Font FontTimer = new("Segoe UI Variable Display", 32, FontStyle.Bold);
    public static readonly Font FontLabel = new("Segoe UI", 10, FontStyle.Regular);
    public static readonly Font FontSmall = new("Segoe UI", 9, FontStyle.Regular);
    public static readonly Font FontBlock = new("Segoe UI", 9, FontStyle.Bold);

    public static Icon CreateJupiterIcon(int size = 64)
    {
        using var bmp = new Bitmap(size, size);
        using (var g = Graphics.FromImage(bmp))
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.InterpolationMode = InterpolationMode.HighQualityBicubic;

            var rect = new Rectangle(0, 0, size - 1, size - 1);

            // Dark space background
            g.Clear(Color.Transparent);

            // Jupiter bands
            var bands = new (Color color, float y1, float y2)[]
            {
                (Color.FromArgb(220, 80, 20), 0.00f, 0.18f),
                (Color.FromArgb(255, 140, 40), 0.18f, 0.35f),
                (Color.FromArgb(255, 200, 80), 0.35f, 0.50f),
                (Color.FromArgb(255, 120, 30), 0.50f, 0.65f),
                (Color.FromArgb(255, 180, 60), 0.65f, 0.82f),
                (Color.FromArgb(200, 70, 15), 0.82f, 1.00f),
            };

            // Clip to circle
            using var path = new GraphicsPath();
            path.AddEllipse(rect);
            g.SetClip(path);

            foreach (var (color, y1, y2) in bands)
            {
                using var brush = new SolidBrush(color);
                g.FillRectangle(brush, 0, (int)(y1 * size), size, (int)((y2 - y1) * size) + 1);
            }

            // Great Red Spot
            using var spotBrush = new SolidBrush(Color.FromArgb(200, 50, 20));
            g.FillEllipse(spotBrush, (int)(size * 0.55f), (int)(size * 0.35f), (int)(size * 0.22f), (int)(size * 0.15f));

            // Reset clip
            g.ResetClip();

            // Subtle border glow
            using var borderPen = new Pen(Color.FromArgb(255, 140, 60), 1.5f);
            g.DrawEllipse(borderPen, rect);
        }

        return Icon.FromHandle(bmp.GetHicon());
    }

    public static void PaintRoundedPanel(Graphics g, Rectangle bounds, Color fillColor, int radius = 12)
    {
        using var path = RoundedRect(bounds, radius);
        using var brush = new SolidBrush(fillColor);
        g.FillPath(brush, path);
    }

    public static GraphicsPath RoundedRect(Rectangle bounds, int radius)
    {
        var path = new GraphicsPath();
        int d = radius * 2;
        path.AddArc(bounds.X, bounds.Y, d, d, 180, 90);
        path.AddArc(bounds.Right - d, bounds.Y, d, d, 270, 90);
        path.AddArc(bounds.Right - d, bounds.Bottom - d, d, d, 0, 90);
        path.AddArc(bounds.X, bounds.Bottom - d, d, d, 90, 90);
        path.CloseFigure();
        return path;
    }
}
