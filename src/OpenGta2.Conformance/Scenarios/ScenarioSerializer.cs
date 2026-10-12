using System.Text.Json;
using System.Text.Json.Serialization;

namespace OpenGta2.Conformance.Scenarios;

public static class ScenarioSerializer
{
    public static JsonSerializerOptions Options { get; } = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        DictionaryKeyPolicy = null,
        WriteIndented = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
        AllowTrailingCommas = true,
        UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow,
        Converters = { new JsonStringEnumConverter(JsonNamingPolicy.CamelCase, allowIntegerValues: false) },
    };

    public static ScenarioDefinition Parse(string json) =>
        JsonSerializer.Deserialize<ScenarioDefinition>(json, Options)
        ?? throw new JsonException("Scenario file is empty.");

    public static ScenarioDefinition Load(string path) => Parse(File.ReadAllText(path));

    /// <summary>Parses and validates; throws <see cref="InvalidScenarioException"/> listing every problem.</summary>
    public static ScenarioDefinition LoadValid(string path)
    {
        var scenario = Load(path);
        var errors = ScenarioValidator.Validate(scenario);
        return errors.Count == 0 ? scenario : throw new InvalidScenarioException(path, errors);
    }

    public static string Serialize(ScenarioDefinition scenario) => JsonSerializer.Serialize(scenario, Options);
}

public sealed class InvalidScenarioException(string source, IReadOnlyList<string> errors)
    : Exception($"Invalid scenario '{source}':{Environment.NewLine}- {string.Join(Environment.NewLine + "- ", errors)}")
{
    public IReadOnlyList<string> Errors { get; } = errors;
}
