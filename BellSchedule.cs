namespace JupitrApp;

public static class BellSchedule
{
    public record TimeBlock(string Name, TimeSpan Start, TimeSpan End);
    public record LunchInfo(string Label, TimeSpan Start, TimeSpan End);

    public static readonly TimeBlock[] RegularDay =
    [
        new("Block 1", new TimeSpan(7, 40, 0), new TimeSpan(9, 6, 0)),
        new("Block 2", new TimeSpan(9, 14, 0), new TimeSpan(10, 40, 0)),
        new("Block 3", new TimeSpan(11, 4, 0), new TimeSpan(12, 52, 0)),
        new("Block 4", new TimeSpan(13, 0, 0), new TimeSpan(14, 20, 0)),
    ];

    public static readonly TimeBlock[] AdvisoryDay =
    [
        new("Block 1", new TimeSpan(7, 40, 0), new TimeSpan(9, 0, 0)),
        new("Advisory", new TimeSpan(9, 8, 0), new TimeSpan(9, 28, 0)),
        new("Block 2", new TimeSpan(9, 36, 0), new TimeSpan(10, 56, 0)),
        new("Block 3", new TimeSpan(11, 4, 0), new TimeSpan(12, 52, 0)),
        new("Block 4", new TimeSpan(13, 0, 0), new TimeSpan(14, 20, 0)),
    ];

    public static TimeBlock[] GetBlocksForDayType(string dayType)
    {
        if (dayType.Contains("Advisory", StringComparison.OrdinalIgnoreCase))
            return AdvisoryDay;
        return RegularDay;
    }

    public static (TimeBlock? Current, TimeSpan Remaining, int Index) GetCurrentBlock(TimeSpan now, string dayType)
    {
        var blocks = GetBlocksForDayType(dayType);
        for (int i = 0; i < blocks.Length; i++)
        {
            var b = blocks[i];
            if (now >= b.Start && now < b.End)
            {
                return (b, b.End - now, i);
            }
        }
        return (null, TimeSpan.Zero, -1);
    }

    public static LunchInfo? GetLunchInfo(int? wave, bool isAdvisory)
    {
        if (!wave.HasValue) return null;
        var w = wave.Value;
        if (w < 1 || w > 4) return null;

        if (isAdvisory)
        {
            return w switch
            {
                1 => new LunchInfo("Lunch", new TimeSpan(11, 4, 0), new TimeSpan(11, 32, 0)),
                2 => new LunchInfo("Lunch", new TimeSpan(11, 34, 0), new TimeSpan(12, 2, 0)),
                3 => new LunchInfo("Lunch", new TimeSpan(11, 54, 0), new TimeSpan(12, 22, 0)),
                4 => new LunchInfo("Lunch", new TimeSpan(12, 24, 0), new TimeSpan(12, 52, 0)),
                _ => null
            };
        }
        else
        {
            return w switch
            {
                1 => new LunchInfo("Lunch", new TimeSpan(10, 48, 0), new TimeSpan(11, 18, 0)),
                2 => new LunchInfo("Lunch", new TimeSpan(11, 19, 0), new TimeSpan(11, 49, 0)),
                3 => new LunchInfo("Lunch", new TimeSpan(11, 45, 0), new TimeSpan(12, 15, 0)),
                4 => new LunchInfo("Lunch", new TimeSpan(12, 16, 0), new TimeSpan(12, 46, 0)),
                _ => null
            };
        }
    }

    public static string FormatRemaining(TimeSpan remaining)
    {
        if (remaining.TotalMinutes < 1)
            return $"{remaining.Seconds}s";
        return $"{remaining.TotalMinutes:0}m {remaining.Seconds}s";
    }

    /// <summary>
    /// Formats a TimeSpan (e.g. 13:00) as 12-hour clock with AM/PM (e.g. 1:00 PM).
    /// </summary>
    public static string FormatTime(TimeSpan time)
    {
        var dt = new DateTime(1, 1, 1, time.Hours, time.Minutes, 0);
        return dt.ToString("h:mm tt");
    }
}
