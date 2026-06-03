using System.Text.RegularExpressions;

namespace JupitrApp;

public class Scraper
{
    private const string CalendarUrl = "https://www.darienps.org/district-information/district-calendar";
    private static string CachePath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "JupitrApp", "calendar_cache.json");
    private static string LogPath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "JupitrApp", "scraper.log");

    private readonly HttpClient _httpClient;
    private DateTime _lastFetchDate = DateTime.MinValue;
    private string? _lastFetchedDayType;

    public Scraper()
    {
        _httpClient = new HttpClient();
        _httpClient.DefaultRequestHeaders.Add("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36");
    }

    public async Task<string?> GetDayTypeAsync(DateTime date)
    {
        // In-memory cache for current day to avoid network calls every second
        if (_lastFetchDate.Date == date.Date && !string.IsNullOrEmpty(_lastFetchedDayType))
            return _lastFetchedDayType;

        var cached = TryGetCached(date);
        if (cached != null)
        {
            _lastFetchDate = date.Date;
            _lastFetchedDayType = cached;
            Log($"Cache hit for {date:yyyy-MM-dd}: {cached}");
            return cached;
        }

        try
        {
            Log($"Fetching calendar for {date:yyyy-MM-dd}");
            var html = await _httpClient.GetStringAsync(CalendarUrl);
            Log($"Got HTML ({html.Length} chars)");

            var dayType = ParseDayTypeFromHtml(html, date);
            if (dayType != null)
            {
                CacheDay(date.Date, dayType);
                _lastFetchDate = date.Date;
                _lastFetchedDayType = dayType;
                Log($"Parsed day type: {dayType}");
                return dayType;
            }
            Log("No DHS day type found in HTML");
        }
        catch (Exception ex)
        {
            Log($"Error fetching calendar: {ex.Message}");
        }

        return null;
    }

    private static string? ParseDayTypeFromHtml(string html, DateTime targetDate)
    {
        // Simple regex-based parsing to avoid XPath exact-class-match issues
        // The calendar uses a grid of day boxes
        var dayBoxes = Regex.Matches(html,
            @"<div\s+class=[""]fsCalendarDaybox[^""]*[""]\s*>(.*?)</div>\s*</div>\s*</div>",
            RegexOptions.Singleline | RegexOptions.IgnoreCase);

        Log($"Found {dayBoxes.Count} day boxes via regex");

        foreach (Match box in dayBoxes)
        {
            var boxHtml = box.Groups[1].Value;

            // Extract date from the box
            var dateMatch = Regex.Match(boxHtml,
                @"<span\s+class=[""]fsCalendarMonth[""]\s*>([^<]+)</span>\s*(\d+)");
            if (!dateMatch.Success) continue;

            var monthName = dateMatch.Groups[1].Value.Trim();
            var dayNum = int.Parse(dateMatch.Groups[2].Value);
            var boxDate = new DateTime(targetDate.Year, ParseMonth(monthName), dayNum);

            if (boxDate.Date != targetDate.Date) continue;

            // Find DHS School Calendar events in this box
            var events = Regex.Matches(boxHtml,
                @"<div\s+class=[""]fsCalendarInfo[""]\s*>(.*?)</div>",
                RegexOptions.Singleline | RegexOptions.IgnoreCase);

            foreach (Match ev in events)
            {
                var evHtml = ev.Groups[1].Value;
                if (!evHtml.Contains("DHS School Calendar", StringComparison.OrdinalIgnoreCase))
                    continue;

                var titleMatch = Regex.Match(evHtml,
                    @"<a\s+[^>]*class=[""]fsCalendarEventTitle\s+fsCalendarEventLink[""]\s+title=[""]([^""]+)[""]");
                if (titleMatch.Success)
                {
                    return titleMatch.Groups[1].Value.Trim();
                }

                // Fallback: extract title from inner text
                var innerMatch = Regex.Match(evHtml,
                    @">\s*([^<]+?)\s*</a>");
                if (innerMatch.Success)
                {
                    var text = innerMatch.Groups[1].Value.Trim();
                    if (!string.IsNullOrWhiteSpace(text) && text.Length < 100)
                        return text;
                }
            }
        }

        return null;
    }

    private static int ParseMonth(string monthName)
    {
        if (DateTime.TryParseExact(monthName, "MMMM", System.Globalization.CultureInfo.InvariantCulture, System.Globalization.DateTimeStyles.None, out var dt))
            return dt.Month;
        if (DateTime.TryParseExact(monthName, "MMM", System.Globalization.CultureInfo.InvariantCulture, System.Globalization.DateTimeStyles.None, out dt))
            return dt.Month;
        return DateTime.Now.Month;
    }

    private static string? TryGetCached(DateTime date)
    {
        var path = CachePath;
        if (!File.Exists(path)) return null;

        try
        {
            var lines = File.ReadAllLines(path);
            foreach (var line in lines)
            {
                var parts = line.Split('|', 2);
                if (parts.Length == 2 && DateTime.TryParse(parts[0], out var cachedDate))
                {
                    if (cachedDate.Date == date.Date) return parts[1];
                }
            }
        }
        catch { }
        return null;
    }

    private static void CacheDay(DateTime date, string dayType)
    {
        var path = CachePath;
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            var lines = File.Exists(path) ? File.ReadAllLines(path).ToList() : new List<string>();
            lines.RemoveAll(l => l.StartsWith(date.ToString("yyyy-MM-dd")));
            lines.Add($"{date:yyyy-MM-dd}|{dayType}");
            File.WriteAllLines(path, lines);
        }
        catch { }
    }

    private static void Log(string message)
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(LogPath)!);
            File.AppendAllText(LogPath, $"[{DateTime.Now:yyyy-MM-dd HH:mm:ss}] {message}{Environment.NewLine}");
        }
        catch { }
    }
}
