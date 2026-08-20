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

    /// <summary>
    /// Parses a saved district calendar page without performing network I/O.
    /// Keeping this method independent makes fixture-based parser tests
    /// possible and keeps the network/cache lifecycle small.
    /// </summary>
    public static string? ParseDayTypeFromHtml(string html, DateTime targetDate)
    {
        var document = new HtmlAgilityPack.HtmlDocument();
        document.LoadHtml(html);

        var dayBoxes = document.DocumentNode.SelectNodes(
            "//div[contains(concat(' ', normalize-space(@class), ' '), ' fsCalendarDaybox ')]");
        Log($"Found {dayBoxes?.Count ?? 0} day boxes via HTML parser");

        if (dayBoxes == null)
            return null;

        foreach (var box in dayBoxes)
        {
            var monthNode = box.SelectSingleNode(
                ".//*[contains(concat(' ', normalize-space(@class), ' '), ' fsCalendarMonth ')]");
            if (monthNode == null)
                continue;

            // Extract the number immediately following the month element.
            var dateMatch = Regex.Match(
                box.InnerHtml,
                @"fsCalendarMonth[^>]*>\s*[^<]+\s*</span>\s*(?<day>\d{1,2})",
                RegexOptions.IgnoreCase | RegexOptions.Singleline);
            if (!dateMatch.Success || !int.TryParse(dateMatch.Groups["day"].Value, out var dayNum))
                continue;

            var monthName = HtmlAgilityPack.HtmlEntity.DeEntitize(monthNode.InnerText).Trim();
            DateTime boxDate;
            try
            {
                boxDate = new DateTime(targetDate.Year, ParseMonth(monthName), dayNum);
            }
            catch (ArgumentOutOfRangeException)
            {
                continue;
            }

            if (boxDate.Date != targetDate.Date)
                continue;

            // Find DHS School Calendar events in this box.
            var events = box.SelectNodes(
                ".//div[contains(concat(' ', normalize-space(@class), ' '), ' fsCalendarInfo ')]");
            if (events == null)
                continue;

            foreach (var ev in events)
            {
                var eventText = HtmlAgilityPack.HtmlEntity.DeEntitize(ev.InnerText).Trim();
                if (!eventText.Contains("DHS School Calendar", StringComparison.OrdinalIgnoreCase))
                    continue;

                var titleNode = ev.SelectSingleNode(
                    ".//a[contains(concat(' ', normalize-space(@class), ' '), ' fsCalendarEventTitle ')]");
                var title = titleNode?.GetAttributeValue("title", string.Empty)?.Trim();
                if (!string.IsNullOrWhiteSpace(title))
                    return HtmlAgilityPack.HtmlEntity.DeEntitize(title);

                var text = HtmlAgilityPack.HtmlEntity.DeEntitize(titleNode?.InnerText ?? string.Empty).Trim();
                if (!string.IsNullOrWhiteSpace(text) && text.Length < 100)
                    return text;
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
        return 0;
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
