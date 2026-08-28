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
            _lastItems = null;
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
        var lunchDayLetter = BellSchedule.GetLunchDayLetter(_dayType);
        var nowTs = now.TimeOfDay;

        _lblDayType.Text = $"{now:ddd, MMM d}  ·  {_dayType}";

        var blocks = BellSchedule.GetBlocksForDayType(_dayType);
        var lunches = GetLunchPeriods(lunchDayLetter);

        var newItems = BuildScheduleItems(blocks, lunches);
        UpdateMainTimer(nowTs, newItems);
        bool needsRebuild = _lastItems == null || _lastDayType != _dayType || !ItemsEqual(_lastItems, newItems);

        if (needsRebuild)
        {
            BuildScheduleControls(newItems, nowTs, lunchDayLetter);
            _lastItems = newItems;
            _lastDayType = _dayType;

            int contentHeight = _scheduleFlow.Bottom + 20;
            Size = new Size(380, Math.Min(contentHeight, 520));
        }
        else
        {
            UpdateHighlights(newItems, nowTs);
        }
    }

    private List<LunchPeriod> GetLunchPeriods(string? dayLetter)
    {
        var periods = new List<LunchPeriod>();
        if (dayLetter == null)
            return periods;

        foreach (var wave in new[] { _config.GetLunchWave(dayLetter), _config.GetAdditionalLunchWave(dayLetter) })
        {
            if (!wave.HasValue || periods.Any(period => period.Wave == wave.Value))
                continue;
            var info = BellSchedule.GetLunchInfo(wave, _dayType);
            if (info != null)
                periods.Add(new LunchPeriod(info, wave.Value));
        }
        return periods;
    }

    private void UpdateMainTimer(TimeSpan now, List<ScheduleItem> items)
    {
        var current = items.FirstOrDefault(item => now >= item.Start && now < item.End);
        if (current != null)
        {
            var remaining = current.End - now;
            if (current.IsLunch)
            {
                _lblTimer.Text = BellSchedule.FormatRemaining(remaining);
                _lblTimer.ForeColor = JupiterTheme.Yellow;
                _lblSubtitle.Text = $"{current.Name}!";
                return;
            }

            _lblTimer.Text = BellSchedule.FormatRemaining(remaining);
            _lblTimer.ForeColor = remaining.TotalMinutes < 5 ? JupiterTheme.Yellow : JupiterTheme.Orange;
            var display = current.MiniName != null
                ? (string.IsNullOrWhiteSpace(current.MiniClassName) ? current.MiniName : current.MiniClassName)
                : (string.IsNullOrWhiteSpace(current.MiniClassName) ? current.Name : current.MiniClassName);
            var miniSuffix = current.MiniName == null ? "" : $" ({current.MiniName})";
            _lblSubtitle.Text = $"Current: {display}{miniSuffix} — ends {BellSchedule.FormatTime(current.End)}";
            return;
        }

        var next = items.FirstOrDefault(item => item.Start > now);
        if (next != null)
        {
            var untilStart = next.Start - now;
            _lblTimer.Text = BellSchedule.FormatRemaining(untilStart);
            _lblTimer.ForeColor = JupiterTheme.Muted;
            var display = next.MiniName != null
                ? (string.IsNullOrWhiteSpace(next.MiniClassName) ? next.MiniName : next.MiniClassName)
                : (string.IsNullOrWhiteSpace(next.MiniClassName) ? next.Name : next.MiniClassName);
            var miniSuffix = next.MiniName == null ? "" : $" ({next.MiniName})";
            _lblSubtitle.Text = $"{display}{miniSuffix} starts at {BellSchedule.FormatTime(next.Start)}";
        }
        else
        {
            _lblTimer.Text = "Done";
            _lblTimer.ForeColor = JupiterTheme.Muted;
            _lblSubtitle.Text = "School is over";
        }
    }

    private List<ScheduleItem> BuildScheduleItems(BellSchedule.TimeBlock[] blocks, List<LunchPeriod> lunches)
    {
        var items = new List<ScheduleItem>();

        for (int i = 0; i < blocks.Length; i++)
        {
            var b = blocks[i];
            var configIndex = BellSchedule.GetConfigBlockIndex(i, _dayType);
            var dayLetter = BellSchedule.GetConfigDayLetter(i, _dayType);
            string? rawClass = null;
            bool hasMinis = false;
            if (configIndex.HasValue && dayLetter != null)
            {
                rawClass = _config.GetClass(dayLetter, configIndex.Value);
                hasMinis = !string.IsNullOrWhiteSpace(rawClass) && rawClass.Contains('/');
            }

            if (!b.Name.Contains("Advisory", StringComparison.OrdinalIgnoreCase) && hasMinis)
            {
                var minis = BellSchedule.GetMinisForApplicationBlock(i, _dayType);
                if (minis != null)
                {
                    var parts = rawClass!.Split('/', 2);
                    var m1Class = parts[0].Trim();
                    var m2Class = parts[1].Trim();

                    items.Add(new ScheduleItem(minis[0].Start, minis[0].End, $"{b.Name} ({minis[0].Name})", i, false, false, m1Class, minis[0].Name));
                    items.Add(new ScheduleItem(minis[1].Start, minis[1].End, $"{b.Name} ({minis[1].Name})", i, false, false, m2Class, minis[1].Name));
                }
                else
                {
                    items.Add(new ScheduleItem(b.Start, b.End, b.Name, i, false, false, rawClass, null));
                }
            }
            else
            {
                items.Add(new ScheduleItem(b.Start, b.End, b.Name, i, false, false, rawClass, null));
            }
        }

        foreach (var lunch in lunches)
        {
            var adjusted = new List<ScheduleItem>();
            foreach (var item in items)
            {
                if (item.IsLunch || item.End <= lunch.Info.Start || item.Start >= lunch.Info.End)
                {
                    adjusted.Add(item);
                    continue;
                }
                if (item.Start < lunch.Info.Start)
                    adjusted.Add(item with { End = lunch.Info.Start });
                if (item.End > lunch.Info.End)
                    adjusted.Add(item with { Start = lunch.Info.End, IsAfterLunch = true });
            }
            adjusted.Add(new ScheduleItem(lunch.Info.Start, lunch.Info.End, $"Lunch Wave {lunch.Wave}", -1, true, false, null, null));
            items = adjusted;
        }

        return items
            .OrderBy(item => item.Start)
            .ThenByDescending(item => item.IsLunch)
            .ToList();
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

            var row = CreateScheduleRow(item, isCurrent, isAdvisory, isAfterLunch, i % 2 == 0);
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

            int labelIdx = hasAccent ? 1 : 0;
            if (labelIdx < row.Controls.Count && row.Controls[labelIdx] is Label lblName)
            {
                var targetFont = isCurrent ? JupiterTheme.FontBlock : JupiterTheme.FontSmall;
                if (lblName.Font != targetFont)
                    lblName.Font = targetFont;
            }
        }
    }

    private Panel CreateScheduleRow(ScheduleItem item, bool isCurrent, bool isAdvisory, bool isAfterLunch, bool evenRow)
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

        if (!item.IsLunch && !isAdvisory && !string.IsNullOrWhiteSpace(item.MiniClassName))
        {
            var lblClass = new Label
            {
                Text = item.MiniClassName,
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
            if (a[i].Name != b[i].Name || a[i].Start != b[i].Start || a[i].End != b[i].End || a[i].IsLunch != b[i].IsLunch || a[i].MiniClassName != b[i].MiniClassName || a[i].MiniName != b[i].MiniName)
                return false;
        }
        return true;
    }

    private record ScheduleItem(TimeSpan Start, TimeSpan End, string Name, int BlockIndex, bool IsLunch, bool IsAfterLunch, string? MiniClassName, string? MiniName);
    private record LunchPeriod(BellSchedule.LunchInfo Info, int Wave);

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        _updateTimer?.Stop();
        _updateTimer?.Dispose();
        base.OnFormClosing(e);
    }
}
