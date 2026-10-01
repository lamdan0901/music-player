# At 60 Hz, scroll for at least one second in each direction, then run this check.
param(
    [string]$LogPath = (Join-Path $env:APPDATA 'MusicPlayer2/scroll_timing.log'),
    [double]$MaxAverageGapMs = 18.5
)

$runs = @(Select-String -LiteralPath $LogPath -Pattern 'scroll: frames=(\d+),.*frame gap avg=([\d.]+)ms' | Select-Object -Last 2)
if ($runs.Count -lt 2) { throw 'Need two completed scroll runs.' }

foreach ($run in $runs) {
    $match = $run.Matches[0]
    $frames = [int]$match.Groups[1].Value
    $gap = [double]::Parse($match.Groups[2].Value, [System.Globalization.CultureInfo]::InvariantCulture)
    if ($frames -lt 30) { throw 'Scroll longer to collect at least 30 frames per run.' }
    if ($gap -le 0 -or $gap -gt $MaxAverageGapMs) {
        throw "Scroll frame gap $gap ms exceeds the $MaxAverageGapMs ms limit."
    }
    Write-Output ("PASS: {0} frames, {1} ms average gap ({2:F1} FPS)" -f $frames, $gap, (1000 / $gap))
}
