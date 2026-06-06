#:package ProcessX@1.5.6

using Cysharp.Diagnostics;
using Zx;
using static Zx.Env;

// Build a project's C# gameplay scripts into a fixed output directory, so the
// engine can load them from a stable path (no Debug/net10.0 baked into it).
// The engine invokes this on project open and on every source change.
//
//   dotnet run --file BuildScripts.cs -- <scripts-dir>

if (args is not [var scriptsDir, ..])
{
    Console.Error.WriteLine("usage: BuildScripts.cs <scripts-dir>");
    return 1;
}

var csproj = Directory.EnumerateFiles(scriptsDir, "*.csproj").FirstOrDefault();
if (csproj is null)
{
    Console.Error.WriteLine($"[build] no .csproj in '{scriptsDir}'");
    return 1;
}

// Under bin/ (already git-ignored) but with a stable name -- no Debug/net10.0
// in the path, so the engine loads from one fixed location.
var outDir = Path.Combine(scriptsDir, "bin", "managed");
log($"[build] {Path.GetFileName(csproj)} -> {outDir}");

try
{
    // Zx mode: `await run($"...")` runs the command shell-style and
    // auto-escapes/quotes the interpolated paths.
    await run($"dotnet build {csproj} -c Debug -o {outDir} --nologo -v quiet");
    log("[build] OK");
    return 0;
}
catch (ProcessErrorException ex)
{
    log($"[build] FAILED (exit {ex.ExitCode})", ConsoleColor.Red);
    return 1;
}