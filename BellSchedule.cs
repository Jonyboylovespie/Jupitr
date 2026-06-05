namespace JupitrApp;

public static class BellSchedule
{
    public record TimeBlock(string Name, TimeSpan Start, TimeSpan End);
    public record LunchInfo(string Label, TimeSpan Start, TimeSpan End);
    public record MiniBlock(string Name, TimeSpan Start, TimeSpan End);

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

    // Mini schedules per block (same for all lunch waves)
    public static readonly MiniBlock[][] RegularDayMinis =
    [
        [new MiniBlock("M1", new TimeSpan(7, 40, 0), new TimeSpan(8, 20, 0)), new MiniBlock("M2", new TimeSpan(8, 26, 0), new TimeSpan(9, 6, 0))],
        [new MiniBlock("M1", new TimeSpan(9, 14, 0), new TimeSpan(9, 54, 0)), new MiniBlock("M2", new TimeSpan(10, 0, 0), new TimeSpan(10, 40, 0))],
        [new MiniBlock("M1", new TimeSpan(10, 48, 0), new TimeSpan(11, 28, 0)), new MiniBlock("M2", new TimeSpan(11, 36, 0), new TimeSpan(12, 16, 0))],
        [new MiniBlock("M1", new TimeSpan(12, 54, 0), new TimeSpan(13, 34, 0)), new MiniBlock("M2", new TimeSpan(13, 40, 0), new TimeSpan(14, 20, 0))],
    ];

    public static readonly MiniBlock[][] AdvisoryDayMinis =
    [
        [new MiniBlock("M1", new TimeSpan(7, 40, 0), new TimeSpan(8, 17, 0)), new MiniBlock("M2", new TimeSpan(8, 23, 0), new TimeSpan(9, 0, 0))],
        [new MiniBlock("M1", new TimeSpan(9, 36, 0), new TimeSpan(10, 13, 0)), new MiniBlock("M2", new TimeSpan(10, 19, 0), new TimeSpan(10, 56, 0))],
        [new MiniBlock("M1", new TimeSpan(11, 4, 0), new TimeSpan(11, 41, 0)), new MiniBlock("M2", new TimeSpan(11, 47, 0), new TimeSpan(12, 24, 0))],
        [new MiniBlock("M1", new TimeSpan(13, 0, 0), new TimeSpan(13, 37, 0)), new MiniBlock("M2", new TimeSpan(13, 43, 0), new TimeSpan(14, 20, 0))],
    ];

    public static TimeBlock[] GetBlocksForDayType(string dayType)
    {
        if (dayType.Contains("Advisory", StringComparison.OrdinalIgnoreCase))
            return AdvisoryDay;
        return RegularDay;
    }

    public static MiniBlock[]? GetMinisForBlock(int blockIndex, bool isAdvisory)
    {
        var minis = isAdvisory ? AdvisoryDayMinis : RegularDayMinis;
        if (blockIndex < 0 || blockIndex >= minis.Length)
            return null;
        return minis[blockIndex];
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

    public static (MiniBlock? CurrentMini, TimeSpan Remaining)? GetCurrentMini(TimeSpan now, string dayType)
    {
        var blocks = GetBlocksForDayType(dayType);
        bool isAdvisory = dayType.Contains("Advisory", StringComparison.OrdinalIgnoreCase);

        for (int i = 0; i < blocks.Length; i++)
        {
            var b = blocks[i];
            if (now >= b.Start && now < b.End)
            {
                var minis = GetMinisForBlock(i, isAdvisory);
                if (minis != null)
                {
                    foreach (var m in minis)
                    {
                        if (now >= m.Start && now < m.End)
                            return (m, m.End - now);
                    }
                }
            }
        }
        return null;
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
        if (remaining.Hours > 0)
            return $"{remaining.Hours}h {remaining.Minutes}m {remaining.Seconds}s";
        if (remaining.Minutes > 0)
            return $"{remaining.Minutes}m {remaining.Seconds}s";
        return $"{remaining.Seconds}s";
    }

    public static string FormatTime(TimeSpan time)
    {
        var dt = new DateTime(1, 1, 1, time.Hours, time.Minutes, 0);
        return dt.ToString("h:mm tt");
    }
}
