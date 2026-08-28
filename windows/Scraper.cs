using System.Globalization;
using System.Reflection;
using System.Text.Json;

namespace JupitrApp;

public class Scraper
{
    private const string CalendarResourceName = "JupitrApp.letter-day-calendar.json";
    private static readonly IReadOnlyDictionary<string, string> Days = LoadDays();

    public Task<string?> GetDayTypeAsync(DateTime date)
    {
        Days.TryGetValue(date.ToString("yyyy-MM-dd", CultureInfo.InvariantCulture), out var dayType);
        return Task.FromResult(dayType);
    }

    internal static IReadOnlyDictionary<string, string> ParseDayTypes(Stream stream)
    {
        using var document = JsonDocument.Parse(stream);
        if (!document.RootElement.TryGetProperty("days", out var days) ||
            days.ValueKind != JsonValueKind.Object)
        {
            return new Dictionary<string, string>();
        }

        return days.EnumerateObject()
            .Where(day => day.Value.ValueKind == JsonValueKind.String)
            .ToDictionary(day => day.Name, day => day.Value.GetString()!);
    }

    private static IReadOnlyDictionary<string, string> LoadDays()
    {
        using var stream = Assembly.GetExecutingAssembly()
            .GetManifestResourceStream(CalendarResourceName);
        return stream == null
            ? new Dictionary<string, string>()
            : ParseDayTypes(stream);
    }
}
