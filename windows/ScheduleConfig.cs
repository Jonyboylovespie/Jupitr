using System.Text.Json;

namespace JupitrApp;

public class ScheduleConfig
{
    public Dictionary<string, List<string>> Classes { get; set; } = new();
    public Dictionary<string, int> LunchWaves { get; set; } = new();
    public Dictionary<string, int> AdditionalLunchWaves { get; set; } = new();

    public int? GetLunchWave(string dayLetter)
    {
        if (LunchWaves.TryGetValue(dayLetter, out var wave))
        {
            if (wave == 0) return null; // 0 = None
            return wave;
        }
        return null;
    }

    public int? GetAdditionalLunchWave(string dayLetter)
    {
        if (!SupportsAdditionalLunch(GetClass(dayLetter, 2)))
            return null;
        if (AdditionalLunchWaves.TryGetValue(dayLetter, out var wave) && wave is >= 1 and <= 4)
            return wave;
        return null;
    }

    public static bool SupportsAdditionalLunch(string? blockThreeClass) =>
        !string.IsNullOrWhiteSpace(blockThreeClass) &&
        blockThreeClass.Contains('/') &&
        !blockThreeClass.Contains("Free", StringComparison.OrdinalIgnoreCase);

    private static string ConfigPath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "JupitrApp", "schedule.json");

    public string? GetClass(string dayLetter, int blockIndex)
    {
        if (Classes.TryGetValue(dayLetter, out var blocks))
        {
            if (blockIndex >= 0 && blockIndex < blocks.Count)
                return blocks[blockIndex];
        }
        return null;
    }

    public static ScheduleConfig Load()
    {
        var path = ConfigPath;
        try
        {
            if (File.Exists(path))
            {
                var json = File.ReadAllText(path);
                var cfg = JsonSerializer.Deserialize<ScheduleConfig>(json);
                if (cfg != null && cfg.Classes != null && cfg.LunchWaves != null)
                {
                    cfg.AdditionalLunchWaves ??= new();
                    EnsureDays(cfg);
                    return cfg;
                }
            }
        }
        catch (Exception)
        {
            // A partially written or hand-edited config should not prevent
            // the tray application from starting with usable defaults.
        }

        return CreateDefault();
    }

    public void Save()
    {
        var path = ConfigPath;
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        var json = JsonSerializer.Serialize(this, new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(path, json);
    }

    public static ScheduleConfig CreateDefault()
    {
        var cfg = new ScheduleConfig();
        foreach (var day in new[] { "A", "B", "C", "D", "E", "F", "G", "H" })
        {
            cfg.Classes[day] = ["", "", "", ""];
            cfg.LunchWaves[day] = 1; // Default to Wave 1
            cfg.AdditionalLunchWaves[day] = 0;
        }
        cfg.Save();
        return cfg;
    }

    private static void EnsureDays(ScheduleConfig cfg)
    {
        foreach (var day in new[] { "A", "B", "C", "D", "E", "F", "G", "H" })
        {
            if (!cfg.Classes.ContainsKey(day))
                cfg.Classes[day] = ["", "", "", ""];
            if (!cfg.LunchWaves.ContainsKey(day))
                cfg.LunchWaves[day] = 1;
            if (!cfg.AdditionalLunchWaves.ContainsKey(day))
                cfg.AdditionalLunchWaves[day] = 0;
        }
    }
}
