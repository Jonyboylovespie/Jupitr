using System.Text.Json;

namespace JupitrApp;

public class ScheduleConfig
{
    public Dictionary<string, List<string>> Classes { get; set; } = new();
    public Dictionary<string, int> LunchWaves { get; set; } = new();

    public int? GetLunchWave(string dayLetter)
    {
        if (LunchWaves.TryGetValue(dayLetter, out var wave))
        {
            if (wave == 0) return null; // 0 = None
            return wave;
        }
        return null;
    }

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
        if (File.Exists(path))
        {
            var json = File.ReadAllText(path);
            var cfg = JsonSerializer.Deserialize<ScheduleConfig>(json);
            if (cfg != null) return cfg;
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
        }
        cfg.Save();
        return cfg;
    }
}
