using System.Drawing;

namespace JupitrApp;

public partial class MainForm : Form
{
    private readonly ScheduleConfig _config;
    private string _dayType;
    private Label _lblHeader = null!;
    private Label _lblCurrent = null!;
    private Label _lblRemaining = null!;
    private DataGridView _dgvSchedule = null!;
    private int? _lastCurrentIndex = null;

    public MainForm(ScheduleConfig config, string dayType)
    {
        _config = config;
        _dayType = dayType;
        InitializeComponent();
        this.Shown += MainForm_Shown;
    }

    private void MainForm_Shown(object? sender, EventArgs e)
    {
        RefreshData();
    }

    public void SetDayType(string dayType)
    {
        _dayType = dayType;
    }

    private void InitializeComponent()
    {
        Text = "Jupitr - School Schedule";
        Size = new Size(480, 520);
        StartPosition = FormStartPosition.CenterScreen;
        FormBorderStyle = FormBorderStyle.Sizable;
        MinimizeBox = true;
        MaximizeBox = false;
        BackColor = Color.White;
        MinimumSize = new Size(400, 350);

        var panel = new Panel
        {
            Dock = DockStyle.Top,
            Height = 120,
            Padding = new Padding(10)
        };

        _lblHeader = new Label
        {
            Font = new Font("Segoe UI", 14, FontStyle.Bold),
            ForeColor = Color.DodgerBlue,
            AutoSize = true,
            Location = new Point(10, 10)
        };
        panel.Controls.Add(_lblHeader);

        _lblCurrent = new Label
        {
            Font = new Font("Segoe UI", 12, FontStyle.Regular),
            ForeColor = Color.Black,
            AutoSize = true,
            Location = new Point(10, 45)
        };
        panel.Controls.Add(_lblCurrent);

        _lblRemaining = new Label
        {
            Font = new Font("Segoe UI", 16, FontStyle.Bold),
            ForeColor = Color.ForestGreen,
            AutoSize = true,
            Location = new Point(10, 75)
        };
        panel.Controls.Add(_lblRemaining);

        Controls.Add(panel);

        _dgvSchedule = new DataGridView
        {
            Dock = DockStyle.Fill,
            ReadOnly = true,
            AllowUserToAddRows = false,
            AllowUserToDeleteRows = false,
            AllowUserToResizeRows = false,
            RowHeadersVisible = false,
            SelectionMode = DataGridViewSelectionMode.FullRowSelect,
            AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill,
            BackgroundColor = Color.White,
            BorderStyle = BorderStyle.None,
            Font = new Font("Segoe UI", 10),
            ColumnHeadersDefaultCellStyle = new DataGridViewCellStyle
            {
                Font = new Font("Segoe UI", 10, FontStyle.Bold),
                BackColor = Color.LightGray
            },
            EnableHeadersVisualStyles = false,
            RowTemplate = { Height = 28 }
        };

        _dgvSchedule.Columns.Add(new DataGridViewTextBoxColumn
        {
            Name = "Block",
            HeaderText = "Block",
            FillWeight = 25,
            SortMode = DataGridViewColumnSortMode.NotSortable
        });
        _dgvSchedule.Columns.Add(new DataGridViewTextBoxColumn
        {
            Name = "Time",
            HeaderText = "Time",
            FillWeight = 35,
            SortMode = DataGridViewColumnSortMode.NotSortable
        });
        _dgvSchedule.Columns.Add(new DataGridViewTextBoxColumn
        {
            Name = "Class",
            HeaderText = "Class",
            FillWeight = 40,
            SortMode = DataGridViewColumnSortMode.NotSortable
        });

        Controls.Add(_dgvSchedule);
    }

    public void RefreshData()
    {
        var now = DateTime.Now;
        var dayLetter = ExtractDayLetter(_dayType);

        _lblHeader.Text = $"{now:dddd, MMMM d} — {_dayType}";

        var blocks = BellSchedule.GetBlocksForDayType(_dayType);
        var (current, remaining, index) = BellSchedule.GetCurrentBlock(now.TimeOfDay, _dayType);

        if (current != null)
        {
            var configIndex = GetConfigBlockIndex(index, _dayType);
            var className = dayLetter != null && configIndex.HasValue ? _config.GetClass(dayLetter, configIndex.Value) : null;
            var display = string.IsNullOrWhiteSpace(className) ? current.Name : className;
            _lblCurrent.Text = $"Current: {display}";
            _lblRemaining.Text = $"{BellSchedule.FormatRemaining(remaining)} left";
        }
        else if (blocks.FirstOrDefault(block => block.Start > now.TimeOfDay) is { } next)
        {
            _lblCurrent.Text = "Up next";
            _lblRemaining.Text = $"{next.Name} at {BellSchedule.FormatTime(next.Start)}";
        }
        else
        {
            _lblCurrent.Text = "School is over";
            _lblRemaining.Text = "";
        }

        // Only rebuild grid if block count changed or current index changed (reduces flicker)
        if (_dgvSchedule.Rows.Count != blocks.Length || _lastCurrentIndex != index)
        {
            _dgvSchedule.Rows.Clear();
            for (int i = 0; i < blocks.Length; i++)
            {
                var b = blocks[i];
                var configIndex = GetConfigBlockIndex(i, _dayType);
                var className = configIndex.HasValue && dayLetter != null
                    ? _config.GetClass(dayLetter, configIndex.Value)
                    : null;

                var rowIndex = _dgvSchedule.Rows.Add(
                    b.Name,
                    $"{BellSchedule.FormatTime(b.Start)} – {BellSchedule.FormatTime(b.End)}",
                    string.IsNullOrWhiteSpace(className) ? "—" : className
                );

                if (current != null && i == index)
                {
                    _dgvSchedule.Rows[rowIndex].DefaultCellStyle.BackColor = Color.LightCyan;
                    _dgvSchedule.Rows[rowIndex].DefaultCellStyle.Font = new Font(_dgvSchedule.Font, FontStyle.Bold);
                    _dgvSchedule.Rows[rowIndex].DefaultCellStyle.SelectionBackColor = Color.SkyBlue;
                }
                else
                {
                    _dgvSchedule.Rows[rowIndex].DefaultCellStyle.BackColor = Color.White;
                    _dgvSchedule.Rows[rowIndex].DefaultCellStyle.Font = _dgvSchedule.Font;
                    _dgvSchedule.Rows[rowIndex].DefaultCellStyle.SelectionBackColor = Color.LightBlue;
                }
            }
            _lastCurrentIndex = index;
        }
        else
        {
            // Just update time remaining label; grid structure hasn't changed
            // Re-highlight in case current block changed but count stayed same
            for (int i = 0; i < blocks.Length; i++)
            {
                var isCurrent = current != null && i == index;
                var row = _dgvSchedule.Rows[i];
                row.DefaultCellStyle.BackColor = isCurrent ? Color.LightCyan : Color.White;
                row.DefaultCellStyle.Font = isCurrent ? new Font(_dgvSchedule.Font, FontStyle.Bold) : _dgvSchedule.Font;
            }
        }
    }

    /// <summary>
    /// Maps the app's block index to the user's config index.
    /// Skips non-academic blocks (e.g., Advisory) so the 4-block config aligns correctly.
    /// </summary>
    private static int? GetConfigBlockIndex(int appBlockIndex, string dayType)
    {
        var blocks = BellSchedule.GetBlocksForDayType(dayType);
        if (appBlockIndex < 0 || appBlockIndex >= blocks.Length)
            return null;

        // If this block itself is non-academic, no config mapping
        if (blocks[appBlockIndex].Name.Contains("Advisory", StringComparison.OrdinalIgnoreCase))
            return null;

        // Count how many non-academic blocks come before this one
        int skipCount = 0;
        for (int i = 0; i < appBlockIndex; i++)
        {
            if (blocks[i].Name.Contains("Advisory", StringComparison.OrdinalIgnoreCase))
                skipCount++;
        }

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
}
