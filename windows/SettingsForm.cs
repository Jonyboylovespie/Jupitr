using System.Drawing;

namespace JupitrApp;

public partial class SettingsForm : Form
{
    private readonly ScheduleConfig _config;
    private readonly Dictionary<string, TextBox[]> _dayInputs = new();
    private readonly Dictionary<string, ComboBox> _lunchInputs = new();
    private readonly Dictionary<string, ComboBox> _additionalLunchInputs = new();

    public SettingsForm(ScheduleConfig config)
    {
        _config = config;
        InitializeComponent();
    }

    private void InitializeComponent()
    {
        Text = "Edit Schedule";
        Size = new Size(850, 560);
        StartPosition = FormStartPosition.CenterScreen;
        FormBorderStyle = FormBorderStyle.Sizable;
        MaximizeBox = false;
        MinimizeBox = false;
        BackColor = JupiterTheme.DarkBg;
        ForeColor = JupiterTheme.Cream;
        MinimumSize = new Size(760, 450);

        // Title
        var lblTitle = new Label
        {
            Text = "Edit Schedule",
            Font = new Font("Segoe UI", 16, FontStyle.Bold),
            ForeColor = JupiterTheme.Cream,
            AutoSize = true,
            Location = new Point(16, 16)
        };
        Controls.Add(lblTitle);

        // Subtitle
        var lblSub = new Label
        {
            Text = "Enter your classes and lunch waves for each day",
            Font = JupiterTheme.FontSmall,
            ForeColor = JupiterTheme.Muted,
            AutoSize = true,
            Location = new Point(16, 48)
        };
        Controls.Add(lblSub);

        // Table
        var table = new TableLayoutPanel
        {
            Location = new Point(16, 80),
            Size = new Size(802, 380),
            ColumnCount = 7,
            RowCount = 9,
            BackColor = JupiterTheme.DarkBg
        };
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 70));
        for (int c = 0; c < 4; c++)
            table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 20));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 90));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 130));

        // Header row
        table.Controls.Add(MakeHeaderCell(""), 0, 0);
        for (int b = 0; b < 4; b++)
            table.Controls.Add(MakeHeaderCell($"Block {b + 1}"), b + 1, 0);
        table.Controls.Add(MakeHeaderCell("Lunch"), 5, 0);
        table.Controls.Add(MakeHeaderCell("Additional Lunch\n(optional)"), 6, 0);

        var days = new[] { "A", "B", "C", "D", "E", "F", "G", "H" };
        for (int d = 0; d < days.Length; d++)
        {
            var day = days[d];
            table.Controls.Add(MakeDayLabel(day), 0, d + 1);

            var inputs = new TextBox[4];
            var classes = _config.Classes.GetValueOrDefault(day, new List<string> { "", "", "", "" });
            for (int b = 0; b < 4; b++)
            {
                var tb = new TextBox
                {
                    Text = b < classes.Count ? classes[b] : "",
                    Dock = DockStyle.Fill,
                    Margin = new Padding(4),
                    BackColor = JupiterTheme.CardBg,
                    ForeColor = JupiterTheme.Cream,
                    BorderStyle = BorderStyle.FixedSingle,
                    Font = JupiterTheme.FontSmall
                };
                inputs[b] = tb;
                table.Controls.Add(tb, b + 1, d + 1);
            }
            _dayInputs[day] = inputs;

            // Lunch wave dropdown
            var cb = new ComboBox
            {
                Dock = DockStyle.Fill,
                Margin = new Padding(4),
                BackColor = JupiterTheme.CardBg,
                ForeColor = JupiterTheme.Cream,
                FlatStyle = FlatStyle.Flat,
                Font = JupiterTheme.FontSmall,
                DropDownStyle = ComboBoxStyle.DropDownList
            };
            cb.Items.Add("None");
            cb.Items.Add("Wave 1");
            cb.Items.Add("Wave 2");
            cb.Items.Add("Wave 3");
            cb.Items.Add("Wave 4");

            var currentWave = _config.GetLunchWave(day);
            cb.SelectedIndex = currentWave.HasValue ? Math.Clamp(currentWave.Value, 1, 4) : 0;

            _lunchInputs[day] = cb;
            table.Controls.Add(cb, 5, d + 1);

            var additional = new ComboBox
            {
                Dock = DockStyle.Fill,
                Margin = new Padding(4),
                BackColor = JupiterTheme.CardBg,
                ForeColor = JupiterTheme.Cream,
                FlatStyle = FlatStyle.Flat,
                Font = JupiterTheme.FontSmall,
                DropDownStyle = ComboBoxStyle.DropDownList
            };
            additional.Items.AddRange(["None", "Wave 1", "Wave 2", "Wave 3", "Wave 4"]);
            additional.SelectedIndex = _config.GetAdditionalLunchWave(day) ?? 0;
            _additionalLunchInputs[day] = additional;
            table.Controls.Add(additional, 6, d + 1);

            void UpdateAdditionalLunchState()
            {
                additional.Enabled = ScheduleConfig.SupportsAdditionalLunch(inputs[2].Text);
                if (!additional.Enabled)
                    additional.SelectedIndex = 0;
            }
            inputs[2].TextChanged += (_, _) => UpdateAdditionalLunchState();
            UpdateAdditionalLunchState();
        }

        Controls.Add(table);

        // Save button
        var btnSave = new Button
        {
            Text = "Save",
            Font = JupiterTheme.FontLabel,
            ForeColor = JupiterTheme.DarkBg,
            BackColor = JupiterTheme.Orange,
            FlatStyle = FlatStyle.Flat,
            Size = new Size(100, 36),
            Location = new Point(718, 472),
            Cursor = Cursors.Hand
        };
        btnSave.FlatAppearance.BorderSize = 0;
        btnSave.FlatAppearance.MouseOverBackColor = JupiterTheme.OrangeLight;
        btnSave.Click += BtnSave_Click;
        Controls.Add(btnSave);
    }

    private static Label MakeHeaderCell(string text)
    {
        return new Label
        {
            Text = text,
            Font = new Font("Segoe UI", 9, FontStyle.Bold),
            ForeColor = JupiterTheme.Muted,
            Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleCenter
        };
    }

    private static Label MakeDayLabel(string day)
    {
        return new Label
        {
            Text = day + " Day",
            Font = new Font("Segoe UI", 9, FontStyle.Bold),
            ForeColor = JupiterTheme.Yellow,
            Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleLeft
        };
    }

    private void BtnSave_Click(object? sender, EventArgs e)
    {
        foreach (var kvp in _dayInputs)
        {
            var day = kvp.Key;
            var inputs = kvp.Value;
            _config.Classes[day] = inputs.Select(t => t.Text).ToList();
        }

        foreach (var kvp in _lunchInputs)
        {
            var day = kvp.Key;
            var cb = kvp.Value;
            _config.LunchWaves[day] = cb.SelectedIndex; // 0=None, 1=Wave1, etc.
        }

        foreach (var kvp in _additionalLunchInputs)
        {
            var day = kvp.Key;
            _config.AdditionalLunchWaves[day] = kvp.Value.Enabled ? kvp.Value.SelectedIndex : 0;
        }

        _config.Save();
        Close();
    }
}
