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
    private int _dayTypeRefreshInProgress;
    private bool _disposed;

    public TrayApp()
    {
        _uiContext = SynchronizationContext.Current ?? new WindowsFormsSynchronizationContext();
        _scraper = new Scraper();
        _config = ScheduleConfig.Load();

        InitializeTrayIcon();

        // Fetch day type in background. The result is applied on the UI
        // context so a popup opened during startup cannot retain a stale
        // "Loading..." state.
        RefreshDayType(DateTime.Now);
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

        // If the date has changed since we last checked, refresh the day type
        // so the schedule stays accurate when the laptop is left running overnight.
        var now = DateTime.Now;
        if (_lastCheckedDate != now.Date)
            RefreshDayType(now);

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
        var dayLetter = BellSchedule.ExtractDayLetter(_currentDayType);
        var blocks = BellSchedule.GetBlocksForDayType(_currentDayType);
        var (current, remaining, index) = BellSchedule.GetCurrentBlock(now.TimeOfDay, _currentDayType);

        string tooltip;
        if (current != null)
        {
            var configIndex = BellSchedule.GetConfigBlockIndex(index, _currentDayType);
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

    private void RefreshDayType(DateTime date)
    {
        if (Interlocked.Exchange(ref _dayTypeRefreshInProgress, 1) == 1)
            return;

        _ = Task.Run(async () =>
        {
            try
            {
                var dayType = await _scraper.GetDayTypeAsync(date).ConfigureAwait(false) ?? "Unknown";
                _uiContext.Post(_ =>
                {
                    if (_disposed)
                        return;

                    _currentDayType = dayType;
                    _lastCheckedDate = date.Date;
                    _popup?.SetDayType(_currentDayType);
                    _popup?.RefreshData();
                    UpdateTrayTooltip(date);
                }, null);
            }
            finally
            {
                Interlocked.Exchange(ref _dayTypeRefreshInProgress, 0);
            }
        });
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            _disposed = true;
            _trayIcon?.Dispose();
            _popup?.Dispose();
            _settingsForm?.Dispose();
        }
        base.Dispose(disposing);
    }
}
