using System.Drawing;
using System.Drawing.Drawing2D;

namespace JupitrApp;

public class TrayApp : ApplicationContext
{
    private NotifyIcon _trayIcon = null!;
    private Scraper _scraper = null!;
    private ScheduleConfig _config = null!;
    private PopupForm? _popup;
    private SettingsForm? _settingsForm;
    private string _currentDayType = "Loading...";
    private DateTime _lastCheckedDate = DateTime.MinValue;
    private readonly SynchronizationContext _uiContext;

    public TrayApp()
    {
        _uiContext = SynchronizationContext.Current ?? new SynchronizationContext();
        _scraper = new Scraper();
        _config = ScheduleConfig.Load();

        InitializeTrayIcon();

        // Fetch day type in background
        _ = Task.Run(async () =>
        {
            var now = DateTime.Now;
            var dayType = await _scraper.GetDayTypeAsync(now);
            _currentDayType = dayType ?? "Unknown";
            _lastCheckedDate = now.Date;

            _uiContext.Post(_ => UpdateTrayTooltip(now), null);
        });
    }

    private void InitializeTrayIcon()
    {
        _trayIcon = new NotifyIcon
        {
            Icon = JupiterTheme.CreateJupiterIcon(64),
            Text = "Jupitr - School Schedule",
            Visible = true,
            ContextMenuStrip = BuildContextMenu()
        };

        _trayIcon.MouseClick += TrayIcon_MouseClick;
    }

    private ContextMenuStrip BuildContextMenu()
    {
        var menu = new ContextMenuStrip();
        menu.Items.Add("Quit", null, (s, e) => Application.Exit());
        return menu;
    }

    private void TrayIcon_MouseClick(object? sender, MouseEventArgs e)
    {
        if (e.Button == MouseButtons.Left)
        {
            ShowPopup();
        }
    }

    private void ShowPopup()
    {
        if (_popup == null || _popup.IsDisposed)
        {
            _popup = new PopupForm(_config, ShowSettingsForm);
        }

        _popup.SetDayType(_currentDayType);
        _popup.RefreshData();

        // Position near system tray (bottom-right of primary screen)
        var screen = Screen.PrimaryScreen ?? Screen.AllScreens[0];
        var trayRect = screen.WorkingArea;
        var popupSize = _popup.Size;

        int x = trayRect.Right - popupSize.Width - 12;
        int y = trayRect.Bottom - popupSize.Height - 12;

        _popup.Location = new Point(x, y);
        _popup.Show();
        _popup.Activate();
    }

    private void ShowSettingsForm()
    {
        if (_settingsForm == null || _settingsForm.IsDisposed)
        {
            _settingsForm = new SettingsForm(_config);
            _settingsForm.FormClosed += (s, e) => _settingsForm = null;
        }
        _settingsForm.Show();
        _settingsForm.Activate();
    }

    private void UpdateTrayTooltip(DateTime now)
    {
        var dayLetter = ExtractDayLetter(_currentDayType);
        var blocks = BellSchedule.GetBlocksForDayType(_currentDayType);
        var (current, remaining, index) = BellSchedule.GetCurrentBlock(now.TimeOfDay, _currentDayType);

        string tooltip;
        if (current != null)
        {
            var configIndex = GetConfigBlockIndex(index, _currentDayType);
            var className = configIndex.HasValue && dayLetter != null
                ? _config.GetClass(dayLetter, configIndex.Value)
                : null;
            var display = string.IsNullOrWhiteSpace(className) ? current.Name : className;
            tooltip = $"{display} — {BellSchedule.FormatRemaining(remaining)} left";
        }
        else if (now.TimeOfDay < blocks[0].Start)
        {
            tooltip = $"Before school — {blocks[0].Name} at {BellSchedule.FormatTime(blocks[0].Start)}";
        }
        else
        {
            tooltip = "School is over";
        }

        _trayIcon.Text = tooltip.Length > 63 ? tooltip[..63] : tooltip;
    }

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

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            _trayIcon?.Dispose();
            _popup?.Dispose();
            _settingsForm?.Dispose();
        }
        base.Dispose(disposing);
    }
}
