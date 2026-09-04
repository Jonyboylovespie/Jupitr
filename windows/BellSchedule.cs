using System.Globalization;
using System.Reflection;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace JupitrApp;

/// <summary>
/// Reads and interprets the platform-neutral bell schedule specification.
/// The JSON is embedded in the Windows binary and copied beside published
/// builds for easier inspection and packaging.
/// </summary>
public static class BellSchedule
{
    public record TimeBlock(string Name, TimeSpan Start, TimeSpan End);
    public record ClassSegment(TimeSpan Start, TimeSpan End);
    public record LunchInfo(string Label, TimeSpan Start, TimeSpan End, IReadOnlyList<ClassSegment> ClassSegments);
    public record MiniBlock(string Name, TimeSpan Start, TimeSpan End);

    private sealed class ScheduleDocument
    {
        [JsonPropertyName("regular")]
        public ScheduleDefinition Regular { get; set; } = new();

        [JsonPropertyName("advisory")]
        public ScheduleDefinition Advisory { get; set; } = new();

        [JsonPropertyName("allPeriods")]
        public ScheduleDefinition AllPeriods { get; set; } = new();
    }

    private sealed class ScheduleDefinition
    {
        [JsonPropertyName("blocks")]
        public List<BlockDefinition> Blocks { get; set; } = [];

        [JsonPropertyName("lunchWaves")]
        public Dictionary<string, LunchDefinition> LunchWaves { get; set; } = new();
    }

    private sealed class BlockDefinition
    {
        [JsonPropertyName("name")]
        public string Name { get; set; } = string.Empty;

        [JsonPropertyName("start")]
        public string Start { get; set; } = string.Empty;

        [JsonPropertyName("end")]
        public string End { get; set; } = string.Empty;

        [JsonPropertyName("minis")]
        public List<MiniDefinition> Minis { get; set; } = [];
    }

    private sealed class MiniDefinition
    {
        [JsonPropertyName("name")]
        public string Name { get; set; } = string.Empty;

        [JsonPropertyName("start")]
        public string Start { get; set; } = string.Empty;

        [JsonPropertyName("end")]
        public string End { get; set; } = string.Empty;
    }

    private sealed class LunchDefinition
    {
        [JsonPropertyName("name")]
        public string Name { get; set; } = "Lunch";

        [JsonPropertyName("start")]
        public string Start { get; set; } = string.Empty;

        [JsonPropertyName("end")]
        public string End { get; set; } = string.Empty;

        [JsonPropertyName("classSegments")]
        public List<ClassSegmentDefinition> ClassSegments { get; set; } = [];
    }

    private sealed class ClassSegmentDefinition
    {
        [JsonPropertyName("start")]
        public string Start { get; set; } = string.Empty;

        [JsonPropertyName("end")]
        public string End { get; set; } = string.Empty;
    }

    private static readonly ScheduleDocument Document = LoadDocument();
    private static readonly TimeBlock[] RegularDay = ConvertBlocks(Document.Regular);
    private static readonly TimeBlock[] AdvisoryDay = ConvertBlocks(Document.Advisory);
    private static readonly TimeBlock[] AllPeriodsDay = ConvertBlocks(Document.AllPeriods);
    private static readonly MiniBlock[][] RegularDayMinis = ConvertMinis(Document.Regular);
    private static readonly MiniBlock[][] AdvisoryDayMinis = ConvertMinis(Document.Advisory);
    private static readonly MiniBlock[][] AllPeriodsDayMinis = ConvertMinis(Document.AllPeriods);

    public static TimeBlock[] GetBlocksForDayType(string? dayType)
    {
        if (IsAllPeriodsDay(dayType))
            return AllPeriodsDay;
        return IsAdvisoryDay(dayType) ? AdvisoryDay : RegularDay;
    }

    public static MiniBlock[]? GetMinisForBlock(int blockIndex, bool isAdvisory)
    {
        if (blockIndex < 0)
            return null;
        var definition = isAdvisory ? Document.Advisory : Document.Regular;
        var block = definition.Blocks
            .Where(item => !item.Name.Contains("Advisory", StringComparison.OrdinalIgnoreCase))
            .ElementAtOrDefault(blockIndex);
        if (block == null)
            return null;
        return ConvertMinis(block);
    }

    public static MiniBlock[]? GetMinisForApplicationBlock(int applicationBlockIndex, string? dayType)
    {
        var minis = IsAllPeriodsDay(dayType)
            ? AllPeriodsDayMinis
            : IsAdvisoryDay(dayType) ? AdvisoryDayMinis : RegularDayMinis;
        if (applicationBlockIndex < 0 || applicationBlockIndex >= minis.Length)
            return null;
        return minis[applicationBlockIndex];
    }

    public static (TimeBlock? Current, TimeSpan Remaining, int Index) GetCurrentBlock(TimeSpan now, string? dayType)
    {
        var blocks = GetBlocksForDayType(dayType);
        for (int i = 0; i < blocks.Length; i++)
        {
            var block = blocks[i];
            if (now >= block.Start && now < block.End)
                return (block, block.End - now, i);
        }

        return (null, TimeSpan.Zero, -1);
    }

    public static (MiniBlock? CurrentMini, TimeSpan Remaining)? GetCurrentMini(TimeSpan now, string? dayType)
    {
        var blocks = GetBlocksForDayType(dayType);
        for (int i = 0; i < blocks.Length; i++)
        {
            var block = blocks[i];
            if (now < block.Start || now >= block.End)
                continue;

            var minis = GetMinisForApplicationBlock(i, dayType);

            if (minis == null)
                return null;

            foreach (var mini in minis)
            {
                if (now >= mini.Start && now < mini.End)
                    return (mini, mini.End - now);
            }
        }

        return null;
    }

    public static LunchInfo? GetLunchInfo(int? wave, bool isAdvisory)
    {
        if (!wave.HasValue || wave.Value < 1 || wave.Value > 4)
            return null;

        var definition = (isAdvisory ? Document.Advisory : Document.Regular).LunchWaves
            .GetValueOrDefault(wave.Value.ToString(CultureInfo.InvariantCulture));
        return definition == null
            ? null
            : ConvertLunch(definition);
    }

    public static LunchInfo? GetLunchInfo(int? wave, string? dayType)
    {
        if (!wave.HasValue || wave.Value < 1 || wave.Value > 4)
            return null;

        var schedule = IsAllPeriodsDay(dayType)
            ? Document.AllPeriods
            : IsAdvisoryDay(dayType) ? Document.Advisory : Document.Regular;
        var definition = schedule.LunchWaves
            .GetValueOrDefault(wave.Value.ToString(CultureInfo.InvariantCulture));
        return definition == null
            ? null
            : ConvertLunch(definition);
    }

    public static bool IsAdvisoryDay(string? dayType) =>
        !string.IsNullOrWhiteSpace(dayType) &&
        dayType.Contains("Advisory", StringComparison.OrdinalIgnoreCase);

    public static bool IsAllPeriodsDay(string? dayType) =>
        !string.IsNullOrWhiteSpace(dayType) &&
        (dayType.Contains("All periods meet", StringComparison.OrdinalIgnoreCase) ||
         dayType.Contains("First Day of Classes", StringComparison.OrdinalIgnoreCase));

    public static string? ExtractDayLetter(string? dayType)
    {
        if (string.IsNullOrWhiteSpace(dayType))
            return null;

        foreach (var letter in new[] { "A", "B", "C", "D", "E", "F", "G", "H" })
        {
            if (dayType.Contains($"{letter} Day", StringComparison.OrdinalIgnoreCase))
                return letter;
        }

        return null;
    }

    public static string? GetConfigDayLetter(int applicationBlockIndex, string? dayType)
    {
        if (IsAllPeriodsDay(dayType))
        {
            if (applicationBlockIndex is >= 0 and < 4)
                return "A";
            if (applicationBlockIndex is >= 4 and < 8)
                return "B";
            return null;
        }
        return ExtractDayLetter(dayType);
    }

    public static string? GetLunchDayLetter(string? dayType) =>
        IsAllPeriodsDay(dayType) ? "B" : ExtractDayLetter(dayType);

    /// <summary>
    /// Maps an application block index to the four academic blocks stored in
    /// the user's configuration. Advisory itself intentionally has no class
    /// slot.
    /// </summary>
    public static int? GetConfigBlockIndex(int appBlockIndex, string? dayType)
    {
        var blocks = GetBlocksForDayType(dayType);
        if (appBlockIndex < 0 || appBlockIndex >= blocks.Length)
            return null;
        if (IsAllPeriodsDay(dayType))
            return appBlockIndex % 4;
        if (blocks[appBlockIndex].Name.Contains("Advisory", StringComparison.OrdinalIgnoreCase))
            return null;

        var skipped = 0;
        for (var i = 0; i < appBlockIndex; i++)
        {
            if (blocks[i].Name.Contains("Advisory", StringComparison.OrdinalIgnoreCase))
                skipped++;
        }

        return appBlockIndex - skipped;
    }

    public static string FormatRemaining(TimeSpan remaining)
    {
        if (remaining <= TimeSpan.Zero)
            return "0s";
        if (remaining.Hours > 0)
            return $"{remaining.Hours}h {remaining.Minutes}m {remaining.Seconds}s";
        if (remaining.Minutes > 0)
            return $"{remaining.Minutes}m {remaining.Seconds}s";
        return $"{remaining.Seconds}s";
    }

    public static string FormatTime(TimeSpan time)
    {
        var dt = new DateTime(1, 1, 1, time.Hours, time.Minutes, 0);
        return dt.ToString("h:mm tt", CultureInfo.InvariantCulture);
    }

    private static ScheduleDocument LoadDocument()
    {
        using var embedded = Assembly.GetExecutingAssembly()
            .GetManifestResourceStream("JupitrApp.bell-schedule.json");

        if (embedded != null)
        {
            var document = JsonSerializer.Deserialize<ScheduleDocument>(embedded);
            if (document != null)
                return document;
        }

        var packagedPath = Path.Combine(AppContext.BaseDirectory, "shared", "schedule", "bell-schedule.json");
        if (File.Exists(packagedPath))
        {
            var document = JsonSerializer.Deserialize<ScheduleDocument>(File.ReadAllText(packagedPath));
            if (document != null)
                return document;
        }

        throw new InvalidOperationException("The embedded Jupitr bell schedule could not be loaded.");
    }

    private static TimeBlock[] ConvertBlocks(ScheduleDefinition definition) =>
        definition.Blocks
            .Select(block => new TimeBlock(block.Name, ParseTime(block.Start), ParseTime(block.End)))
            .ToArray();

    private static MiniBlock[][] ConvertMinis(ScheduleDefinition definition) =>
        definition.Blocks
            .Select(ConvertMinis)
            .ToArray();

    private static MiniBlock[] ConvertMinis(BlockDefinition block) =>
        block.Minis
            .Select(mini => new MiniBlock(mini.Name, ParseTime(mini.Start), ParseTime(mini.End)))
            .ToArray();

    private static LunchInfo ConvertLunch(LunchDefinition lunch) =>
        new(
            lunch.Name,
            ParseTime(lunch.Start),
            ParseTime(lunch.End),
            lunch.ClassSegments
                .Select(segment => new ClassSegment(ParseTime(segment.Start), ParseTime(segment.End)))
                .ToArray());

    private static TimeSpan ParseTime(string value) =>
        TimeSpan.ParseExact(value, "hh\\:mm", CultureInfo.InvariantCulture);
}
