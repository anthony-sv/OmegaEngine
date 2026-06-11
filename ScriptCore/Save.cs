using System.Text.Json;

namespace OmegaEngine;

// Persistent save data: a flat key -> value store written to the PROJECT's
// saves/save.json (the engine runs with the project root as the working
// directory). Pure managed code -- no engine interop. Loaded lazily on first
// access; every Set writes through to disk (save files are tiny, and a crash
// right after a checkpoint should not lose it).
public static class Save
{
    const string Path = "saves/save.json";

    static Dictionary<string, JsonElement>? _data;

    static Dictionary<string, JsonElement> Data
    {
        get
        {
            if (_data is null)
            {
                try
                {
                    _data = File.Exists(Path)
                        ? JsonSerializer.Deserialize<Dictionary<string, JsonElement>>(File.ReadAllText(Path))
                        : null;
                }
                catch (Exception)   // corrupt / unreadable file -> start fresh
                {
                    _data = null;
                }
                _data ??= new Dictionary<string, JsonElement>();
            }
            return _data;
        }
    }

    public static bool Has(string key) => Data.ContainsKey(key);

    public static float  GetFloat(string key, float fallback = 0f)
        => Data.TryGetValue(key, out var v) && v.ValueKind == JsonValueKind.Number ? v.GetSingle() : fallback;
    public static int    GetInt(string key, int fallback = 0)
        => Data.TryGetValue(key, out var v) && v.ValueKind == JsonValueKind.Number ? v.GetInt32() : fallback;
    public static bool   GetBool(string key, bool fallback = false)
        => Data.TryGetValue(key, out var v) && (v.ValueKind == JsonValueKind.True || v.ValueKind == JsonValueKind.False) ? v.GetBoolean() : fallback;
    public static string GetString(string key, string fallback = "")
        => Data.TryGetValue(key, out var v) && v.ValueKind == JsonValueKind.String ? (v.GetString() ?? fallback) : fallback;

    public static void SetFloat(string key, float value)   => Write(key, value);
    public static void SetInt(string key, int value)       => Write(key, value);
    public static void SetBool(string key, bool value)     => Write(key, value);
    public static void SetString(string key, string value) => Write(key, value);

    public static void Delete(string key)
    {
        if (Data.Remove(key))
            Flush();
    }

    public static void Clear()
    {
        Data.Clear();
        Flush();
    }

    static void Write<T>(string key, T value)
    {
        Data[key] = JsonSerializer.SerializeToElement(value);
        Flush();
    }

    static void Flush()
    {
        Directory.CreateDirectory("saves");
        File.WriteAllText(Path, JsonSerializer.Serialize(Data, new JsonSerializerOptions { WriteIndented = true }));
    }
}