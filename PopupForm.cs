using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

namespace JupitrApp;

public partial class PopupForm : Form
{
    private readonly ScheduleConfig _config;
    private readonly Action _openSettings;
    private string _dayType = "Unknown";

    private Label _lblDayType = null!;
    private Label _lblTimer = null!;
    private Label _lblSubtitle = null!;
    private FlowLayoutPanel _scheduleFlow = null!;
    private Button _btnSettings = null!;
    private System.Windows.Forms.Timer _updateTimer = null!;

    // Cache to avoid rebuilding UI every second
    private List<ScheduleItem>? _lastItems;
    private string? _lastDayType;

    public PopupForm(ScheduleConfig config, Action openSettings)
    {
        _config = config;
        _openSettings = openSettings;
        InitializeComponent();
        this.Deactivate += PopupForm_Deactivate;
    }

    public void SetDayType(string dayType)
    {
        _dayType = dayType;
    }

    private void InitializeComponent()
    {
        FormBorderStyle = FormBorderStyle.None;
        StartPosition = FormStartPosition.Manual;
        BackColor = JupiterTheme.DarkBg;
        ShowInTaskbar = false;
        TopMost = true;
        AutoSize = false;
        Padding = new Padding(0);
        Size = new Size(380, 420);

        var container = new Panel
        {
            Dock = DockStyle.Fill,
            BackColor = JupiterTheme.DarkBg,
            Padding = new Padding(20),
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink
        };
        Controls.Add(container);

        _lblDayType = new Label
        {
            Text = "Loading...",
            Font = JupiterTheme.FontHeader,
            ForeColor = JupiterTheme.Cream,
            AutoSize = true,
            Location = new Point(20, 20)
        };
        container.Controls.Add(_lblDayType);

        _btnSettings = new Button
        {
            Text = "⚙",
            Font = new Font("Segoe UI", 11),
            ForeColor = JupiterTheme.Muted,
            BackColor = Color.Transparent,
            FlatStyle = FlatStyle.Flat,
            Size = new Size(30, 30),
            Location = new Point(330, 16),
            Cursor = Cursors.Hand
        };
        _btnSettings.FlatAppearance.BorderSize = 0;
        _btnSettings.FlatAppearance.MouseOverBackColor = JupiterTheme.HoverBg;
        _btnSettings.Click += (s, e) =>
        {
            Hide();
            _openSettings();
        };
        container.Controls.Add(_btnSettings);

        _lblTimer = new Label
        {
            Text = "--:--",
            Font = new Font("Segoe UI Variable Display", 26, FontStyle.Bold),
            ForeColor = JupiterTheme.Orange,
            AutoSize = true,
            Location = new Point(20, 56)
        };
        container.Controls.Add(_lblTimer);

        _lblSubtitle = new Label
        {
            Text = "",
            Font = JupiterTheme.FontLabel,
            ForeColor = JupiterTheme.Yellow,
            AutoSize = true,
            Location = new Point(20, 100)
        };
        container.Controls.Add(_lblSubtitle);

        var sep = new Panel
        {
            Height = 1,
            BackColor = JupiterTheme.HoverBg,
            Location = new Point(20, 140),
            Width = 340
        };
        container.Controls.Add(sep);

        _scheduleFlow = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            Location = new Point(20, 156),
            Width = 340,
            BackColor = Color.Transparent,
            Padding = new Padding(0),
            Margin = new Padding(0)
        };
        container.Controls.Add(_scheduleFlow);

        _updateTimer = new System.Windows.Forms.Timer { Interval = 1000 };
        _updateTimer.Tick += UpdateTimer_Tick;

        this.Shown += PopupForm_Shown;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        using var path = JupiterTheme.RoundedRect(new Rectangle(0, 0, Width - 1, Height - 1), 16);
        using var pen = new Pen(JupiterTheme.HoverBg, 1);
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        e.Graphics.DrawPath(pen, path);
        Region = new Region(path);
    }

    private void PopupForm_Shown(object? sender, EventArgs e)
    {
        _updateTimer.Start();
        RefreshData();
    }

    private void PopupForm_Deactivate(object? sender, EventArgs e)
    {
        if (!ClientRectangle.Contains(PointToClient(Cursor.Position)))
            Hide();
    }

    protected override void OnVisibleChanged(EventArgs e)
    {
        base.OnVisibleChanged(e);
        if (Visible)
        {
            _updateTimer?.Start();
            _lastItems = null; // Force rebuild on show
            RefreshData();
        }
        else
        {
            _updateTimer?.Stop();
        }
    }

    private void UpdateTimer_Tick(object? sender, EventArgs e)
    {
        RefreshData();
    }

    public void RefreshData()
    {
        var now = DateTime.Now;
        var dayLetter = ExtractDayLetter(_dayType);
        var nowTs = now.TimeOfDay;

        _lblDayType.Text = $"{now:ddd, MMM d}  ·  {_dayType}";

        var blocks = BellSchedule.GetBlocksForDayType(_dayType);
        var lunchWave = dayLetter != null ? _config.GetLunchWave(dayLetter) : null;
        var lunchInfo = BellSchedule.GetLunchInfo(lunchWave, _dayType.Contains("Advisory", StringComparison.OrdinalIgnoreCase));

        var (current, remaining, index) = BellSchedule.GetCurrentBlock(nowTs, _dayType);

        // Update timer text (no flashing - just label text changes)
        UpdateMainTimer(nowTs, blocks, current, remaining, index, lunchInfo);

        // Build schedule list only if structure changed (day type or lunch config changed)
        var newItems = BuildScheduleItems(blocks, lunchInfo, lunchWave);
        bool needsRebuild = _lastItems == null || _lastDayType != _dayType || !ItemsEqual(_lastItems, newItems);

        if (needsRebuild)
        {
            BuildScheduleControls(newItems, nowTs, dayLetter);
            _lastItems = newItems;
            _lastDayType = _dayType;

            // Resize form to fit content
            int contentHeight = _scheduleFlow.Bottom + 20;
            Size = new Size(380, Math.Min(contentHeight, 520));
        }
        else
        {
            // Just update highlight on existing controls
            UpdateHighlights(newItems, nowTs);
        }
    }

    private void UpdateMainTimer(TimeSpan now, BellSchedule.TimeBlock[] blocks, BellSchedule.TimeBlock? current, TimeSpan remaining, int index, BellSchedule.LunchInfo? lunch)
    {
        if (current != null)
        {
            if (lunch != null && now >= lunch.Start && now < lunch.End)
            {
                var lunchRemaining = lunch.End - now;
                _lblTimer.Text = BellSchedule.FormatRemaining(lunchRemaining);
                _lblTimer.ForeColor = JupiterTheme.Yellow;
                _lblSubtitle.Text = "Lunch time!";
                return;
            }

            if (lunch != null && now < lunch.Start && lunch.Start < current.End)
            {
                var untilLunch = lunch.Start - now;
                _lblTimer.Text = BellSchedule.FormatRemaining(untilLunch);
                _lblTimer.ForeColor = JupiterTheme.Yellow;
                _lblSubtitle.Text = $"Lunch starts at {BellSchedule.FormatTime(lunch.Start)}";
                return;
            }

            _lblTimer.Text = BellSchedule.FormatRemaining(remaining);
            _lblTimer.ForeColor = remaining.TotalMinutes < 5 ? JupiterTheme.Yellow : JupiterTheme.Orange;

            var dayLetter = ExtractDayLetter(_dayType);
            var configIndex = GetConfigBlockIndex(index, _dayType);
            var className = configIndex.HasValue && dayLetter != null
                ? _config.GetClass(dayLetter, configIndex.Value)
                : null;
            var display = string.IsNullOrWhiteSpace(className) ? current.Name : className;
            _lblSubtitle.Text = $"Current: {display} — ends {BellSchedule.FormatTime(current.End)}";
        }
        else if (lunch != null && now >= lunch.Start && now < lunch.End)
        {
            var lunchRemaining = lunch.End - now;
            _lblTimer.Text = BellSchedule.FormatRemaining(lunchRemaining);
            _lblTimer.ForeColor = JupiterTheme.Yellow;
            _lblSubtitle.Text = "Lunch time!";
        }
        else if (now < blocks[0].Start)
        {
            var untilStart = blocks[0].Start - now;
            _lblTimer.Text = BellSchedule.FormatRemaining(untilStart);
            _lblTimer.ForeColor = JupiterTheme.Muted;
            _lblSubtitle.Text = $"{blocks[0].Name} starts at {BellSchedule.FormatTime(blocks[0].Start)}";
        }
        else if (lunch != null && now >= lunch.End && now < blocks[^1].End)
        {
            foreach (var b in blocks)
            {
                if (now < b.End)
                {
                    var untilEnd = b.End - now;
                    _lblTimer.Text = BellSchedule.FormatRemaining(untilEnd);
                    _lblTimer.ForeColor = JupiterTheme.Orange;
                    _lblSubtitle.Text = $"Back to {b.Name} — ends {BellSchedule.FormatTime(b.End)}";
                    break;
                }
            }
        }
        else
        {
            _lblTimer.Text = "Done";
            _lblTimer.ForeColor = JupiterTheme.Muted;
            _lblSubtitle.Text = "School is over";
        }
    }

    private List<ScheduleItem> BuildScheduleItems(BellSchedule.TimeBlock[] blocks, BellSchedule.LunchInfo? lunchInfo, int? lunchWave)
    {
        var items = new List<ScheduleItem>();

        for (int i = 0; i < blocks.Length; i++)
        {
            var b = blocks[i];
            bool isAdvisory = b.Name.Contains("Advisory", StringComparison.OrdinalIgnoreCase);

            if (lunchInfo != null && !isAdvisory &&
                lunchInfo.Start > b.Start && lunchInfo.End < b.End)
            {
                items.Add(new ScheduleItem(b.Start, lunchInfo.Start, b.Name, i, false, false));
                items.Add(new ScheduleItem(lunchInfo.Start, lunchInfo.End, $"Lunch Wave {lunchWave}", -1, true, false));
                items.Add(new ScheduleItem(lunchInfo.End, b.End, b.Name, i, false, true));
            }
            else
            {
                items.Add(new ScheduleItem(b.Start, b.End, b.Name, i, false, false));

                if (lunchInfo != null && b.End <= lunchInfo.Start)
                {
                    if (i + 1 < blocks.Length)
                    {
                        if (lunchInfo.Start < blocks[i + 1].Start && lunchInfo.End <= blocks[i + 1].Start)
                        {
                            items.Add(new ScheduleItem(lunchInfo.Start, lunchInfo.End, $"Lunch Wave {lunchWave}", -1, true, false));
                        }
                    }
                    else if (i == blocks.Length - 1)
                    {
                        items.Add(new ScheduleItem(lunchInfo.Start, lunchInfo.End, $"Lunch Wave {lunchWave}", -1, true, false));
                    }
                }
            }
        }

        if (lunchInfo != null && !items.Any(item => item.IsLunch))
        {
            int insertIdx = items.FindIndex(item => item.Start > lunchInfo.Start);
            if (insertIdx == -1) insertIdx = items.Count;
            items.Insert(insertIdx, new ScheduleItem(lunchInfo.Start, lunchInfo.End, $"Lunch Wave {lunchWave}", -1, true, false));
        }

        return items;
    }

    private void BuildScheduleControls(List<ScheduleItem> items, TimeSpan now, string? dayLetter)
    {
        _scheduleFlow.SuspendLayout();
        _scheduleFlow.Controls.Clear();

        for (int i = 0; i < items.Count; i++)
        {
            var item = items[i];
            bool isCurrent = now >= item.Start && now < item.End;
            bool isAdvisory = item.Name.Contains("Advisory", StringComparison.OrdinalIgnoreCase);
            bool isAfterLunch = item.IsAfterLunch;

            string? className = null;
            if (!item.IsLunch && !isAdvisory)
            {
                var configIndex = GetConfigBlockIndex(item.BlockIndex, _dayType);
                if (configIndex.HasValue && dayLetter != null)
                    className = _config.GetClass(dayLetter, configIndex.Value);
            }

            var row = CreateScheduleRow(item, isCurrent, isAdvisory, isAfterLunch, className, i % 2 == 0);
            _scheduleFlow.Controls.Add(row);
        }

        _scheduleFlow.ResumeLayout(true);
    }

    private void UpdateHighlights(List<ScheduleItem> items, TimeSpan now)
    {
        for (int i = 0; i < items.Count && i < _scheduleFlow.Controls.Count; i++)
        {
            var item = items[i];
            var row = _scheduleFlow.Controls[i] as Panel;
            if (row == null) continue;

            bool isCurrent = now >= item.Start && now < item.End;
            bool evenRow = i % 2 == 0;
            Color targetBg = isCurrent ? JupiterTheme.HoverBg : (evenRow ? JupiterTheme.CardBg : JupiterTheme.CardBgAlt);

            if (row.BackColor != targetBg)
                row.BackColor = targetBg;

            // Toggle accent bar
            bool hasAccent = row.Controls.Count > 0 && row.Controls[0] is Panel p && p.Dock == DockStyle.Left && p.Width == 3;
            if (isCurrent && !hasAccent)
            {
                var accent = new Panel { Dock = DockStyle.Left, Width = 3, BackColor = item.IsLunch ? JupiterTheme.Yellow : JupiterTheme.Orange };
                row.Controls.Add(accent);
                accent.BringToFront();
            }
            else if (!isCurrent && hasAccent)
            {
                var accent = row.Controls[0];
                if (accent is Panel p2 && p2.Dock == DockStyle.Left && p2.Width == 3)
                    row.Controls.RemoveAt(0);
            }

            // Update bold font on name label (second control after possible accent)
            int labelIdx = hasAccent ? 1 : 0;
            if (labelIdx < row.Controls.Count && row.Controls[labelIdx] is Label lblName)
            {
                var targetFont = isCurrent ? JupiterTheme.FontBlock : JupiterTheme.FontSmall;
                if (lblName.Font != targetFont)
                    lblName.Font = targetFont;
            }
        }
    }

    private Panel CreateScheduleRow(ScheduleItem item, bool isCurrent, bool isAdvisory, bool isAfterLunch, string? className, bool evenRow)
    {
        int rowHeight = item.IsLunch ? 28 : 36;
        var row = new Panel
        {
            Size = new Size(340, rowHeight),
            BackColor = isCurrent ? JupiterTheme.HoverBg : (evenRow ? JupiterTheme.CardBg : JupiterTheme.CardBgAlt),
            Margin = new Padding(0, 1, 0, 1)
        };

        if (isCurrent)
        {
            var accent = new Panel
            {
                Dock = DockStyle.Left,
                Width = 3,
                BackColor = item.IsLunch ? JupiterTheme.Yellow : JupiterTheme.Orange
            };
            row.Controls.Add(accent);
        }

        int xOffset = isCurrent ? 14 : 10;

        var lblName = new Label
        {
            Text = item.IsLunch ? item.Name : (isAfterLunch ? item.Name + " (cont.)" : item.Name),
            Font = isCurrent ? JupiterTheme.FontBlock : JupiterTheme.FontSmall,
            ForeColor = item.IsLunch ? JupiterTheme.Yellow : (isAdvisory ? JupiterTheme.Muted : (isCurrent ? JupiterTheme.Cream : JupiterTheme.Muted)),
            AutoSize = true,
            Location = new Point(xOffset, item.IsLunch ? 4 : 8)
        };
        row.Controls.Add(lblName);

        var lblTime = new Label
        {
            Text = $"{BellSchedule.FormatTime(item.Start)} – {BellSchedule.FormatTime(item.End)}",
            Font = JupiterTheme.FontSmall,
            ForeColor = item.IsLunch ? JupiterTheme.YellowLight : JupiterTheme.Muted,
            AutoSize = true,
            Location = new Point(120, item.IsLunch ? 4 : 8)
        };
        row.Controls.Add(lblTime);

        if (!item.IsLunch && !isAdvisory && !string.IsNullOrWhiteSpace(className))
        {
            var lblClass = new Label
            {
                Text = className,
                Font = JupiterTheme.FontSmall,
                ForeColor = isCurrent ? JupiterTheme.Yellow : JupiterTheme.Cream,
                AutoSize = true,
                Location = new Point(240, 8)
            };
            row.Controls.Add(lblClass);
        }

        return row;
    }

    private static bool ItemsEqual(List<ScheduleItem> a, List<ScheduleItem> b)
    {
        if (a.Count != b.Count) return false;
        for (int i = 0; i < a.Count; i++)
        {
            if (a[i].Name != b[i].Name || a[i].Start != b[i].Start || a[i].End != b[i].End || a[i].IsLunch != b[i].IsLunch)
                return false;
        }
        return true;
    }

    private record ScheduleItem(TimeSpan Start, TimeSpan End, string Name, int BlockIndex, bool IsLunch, bool IsAfterLunch);

    private static int? GetConfigBlockIndex(int appBlockIndex, string dayType)
    {
        var blocks = BellSchedule.GetBlocksForDayType(dayType);
        if (appBlockIndex < 0 || appBlockIndex >= blocks.Length)
            return null;
        if (blocks[appBlockIndex].Name.Contains("Advisory", StringComparison.OrdinalIgnoreCase))
            return null;

        int skipCount = 0;
        for (int i = 0; i < appBlockIndex; i++)
            if (blocks[i].Name.Contains("Advisory", StringComparison.OrdinalIgnoreCase))
                skipCount++;
        return appBlockIndex - skipCount;
    }

    private static string? ExtractDayLetter(string dayType)
    {
        foreach (var letter in new[] { "A", "B", "C", "D", "E", "F", "G", "H" })
        {
            if (dayType.Contains($"{letter} Day", StringComparison.OrdinalIgnoreCase))
                return letter;
        }
        return null;
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        _updateTimer?.Stop();
        _updateTimer?.Dispose();
        base.OnFormClosing(e);
    }
}
