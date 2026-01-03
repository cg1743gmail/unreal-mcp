using UnrealMCP.Sidecar;
using Xunit;

namespace UnrealMCP.Sidecar.Tests;

public class ProgramTests
{
    [Fact]
    public void GetVersion_ReturnsValidVersion()
    {
        var version = Program.GetVersion();

        Assert.NotNull(version);
        Assert.NotEmpty(version);
        Assert.Matches(@"^\d+\.\d+\.\d+", version); // Starts with semver pattern
    }

    [Fact]
    public void GetVersion_DoesNotContainBuildMetadata()
    {
        var version = Program.GetVersion();

        Assert.DoesNotContain("+", version);
    }
}
