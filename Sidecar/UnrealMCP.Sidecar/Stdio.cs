using System.Text;
using System.Text.Json.Nodes;

namespace UnrealMCP.Sidecar;

internal static class Stdio
{
    private static readonly Stream StdIn = Console.OpenStandardInput();
    private static readonly Stream StdOut = Console.OpenStandardOutput();
    private static readonly SemaphoreSlim WriteLock = new(1, 1);

    // Tighten fallback: by default we require Content-Length framing (MCP/LSP style).
    // Set UNREAL_MCP_STDIO_ALLOW_LINE_JSON=1 to enable dev-only line-delimited JSON input.
    private static readonly bool AllowLineDelimitedJson =
        string.Equals(Environment.GetEnvironmentVariable("UNREAL_MCP_STDIO_ALLOW_LINE_JSON"), "1", StringComparison.OrdinalIgnoreCase);

    public static async Task<string?> ReadJsonAsync(CancellationToken ct)
    {
        int first = await ReadFirstNonWhitespaceByteAsync(ct);
        if (first < 0)
            return null;

        if (first == '{' || first == '[')
        {
            if (!AllowLineDelimitedJson)
                throw new InvalidOperationException("Line-delimited JSON on stdio is disabled. Use Content-Length framing or set UNREAL_MCP_STDIO_ALLOW_LINE_JSON=1 for dev.");

            // Dev fallback mode: read until newline.
            var ms = new MemoryStream();
            ms.WriteByte((byte)first);
            while (true)
            {
                var b = new byte[1];
                var r = await StdIn.ReadAsync(b, 0, 1, ct);
                if (r <= 0)
                    break;
                ms.WriteByte(b[0]);
                if (b[0] == (byte)'\n')
                    break;
            }
            return Encoding.UTF8.GetString(ms.ToArray()).Trim();
        }

        // Header-framed; accumulate until \r\n\r\n or \n\n.
        var header = new MemoryStream();
        header.WriteByte((byte)first);

        int contentLength = -1;
        while (true)
        {
            var b = new byte[1];
            var r = await StdIn.ReadAsync(b, 0, 1, ct);
            if (r <= 0)
                return null;

            header.WriteByte(b[0]);

            var bytes = header.ToArray();
            var headerText = Encoding.ASCII.GetString(bytes);

            var splitIdx = headerText.IndexOf("\r\n\r\n", StringComparison.Ordinal);
            var delimLen = 4;
            if (splitIdx < 0)
            {
                splitIdx = headerText.IndexOf("\n\n", StringComparison.Ordinal);
                delimLen = 2;
            }

            if (splitIdx < 0)
                continue;

            var headerPart = headerText.Substring(0, splitIdx);
            foreach (var line in headerPart.Split(new[] { "\r\n", "\n" }, StringSplitOptions.RemoveEmptyEntries))
            {
                var colon = line.IndexOf(':');
                if (colon <= 0)
                    continue;

                var key = line.Substring(0, colon).Trim();
                var val = line.Substring(colon + 1).Trim();
                if (key.Equals("Content-Length", StringComparison.OrdinalIgnoreCase) && int.TryParse(val, out var n))
                    contentLength = n;
            }

            if (contentLength < 0)
                throw new InvalidOperationException("Missing Content-Length header");

            var bodyPrefix = bytes[(splitIdx + delimLen)..];

            var body = new byte[contentLength];
            var offset = 0;

            var toCopy = Math.Min(bodyPrefix.Length, contentLength);
            Array.Copy(bodyPrefix, 0, body, 0, toCopy);
            offset += toCopy;

            while (offset < contentLength)
            {
                var rr = await StdIn.ReadAsync(body, offset, contentLength - offset, ct);
                if (rr <= 0)
                    throw new EndOfStreamException("stdin closed while reading body");
                offset += rr;
            }

            return Encoding.UTF8.GetString(body);
        }
    }

    private static async Task<int> ReadFirstNonWhitespaceByteAsync(CancellationToken ct)
    {
        var buf = new byte[1];
        while (true)
        {
            var r = await StdIn.ReadAsync(buf, 0, 1, ct);
            if (r <= 0)
                return -1;

            var b = buf[0];
            if (b is (byte)' ' or (byte)'\t' or (byte)'\r' or (byte)'\n')
                continue;

            return b;
        }
    }

    public static Task WriteJsonAsync(JsonObject response, CancellationToken ct)
        => WriteAnyAsync(response, ct);

    public static async Task WriteAnyAsync(JsonObject message, CancellationToken ct)
    {
        var json = message.ToJsonString(JsonUtil.JsonOptions);
        var payload = Encoding.UTF8.GetBytes(json);
        var header = Encoding.ASCII.GetBytes($"Content-Length: {payload.Length}\r\n\r\n");

        await WriteLock.WaitAsync(ct);
        try
        {
            await StdOut.WriteAsync(header, 0, header.Length, ct);
            await StdOut.WriteAsync(payload, 0, payload.Length, ct);
            await StdOut.FlushAsync(ct);
        }
        finally
        {
            WriteLock.Release();
        }
    }
}
