param(
    [Parameter(Mandatory = $true)][string] $PackageDirectory,
    [Parameter(Mandatory = $true)][string] $ExpectedCommit
)

$ErrorActionPreference = 'Stop'
$packageRoot = (Resolve-Path $PackageDirectory).Path
$archives = @(Get-ChildItem -LiteralPath $packageRoot -Filter '*-Portable.zip')
if ($archives.Count -ne 1) { throw 'Expected exactly one portable ZIP' }
$archive = $archives[0]
$hash = (Get-FileHash -LiteralPath $archive.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
$manifest = Get-Content -LiteralPath (Join-Path $packageRoot 'SHA256SUMS.txt')
$matchingEntries = @(
    foreach ($line in $manifest) {
        if ($line -match '^([0-9A-Fa-f]{64}) ([ *])(.+)$') {
            $entryName = $Matches[3]
            if ($entryName.StartsWith('./', [StringComparison]::Ordinal)) { $entryName = $entryName.Substring(2) }
            if ($entryName -ceq $archive.Name) {
                [pscustomobject]@{ Hash = $Matches[1] }
            }
        }
    }
)
if ($matchingEntries.Count -ne 1 -or !$matchingEntries[0].Hash.Equals($hash, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'ZIP checksum mismatch or duplicate matching manifest entry'
}
$extractRoot = Join-Path $env:TEMP ('UTN portable test ' + [guid]::NewGuid())
Expand-Archive -LiteralPath $archive.FullName -DestinationPath $extractRoot
if ((Get-Content -LiteralPath (Join-Path $extractRoot 'SOURCE-COMMIT.txt') -Raw).Trim() -ne $ExpectedCommit) {
    throw 'Portable source revision mismatch'
}
$log = Join-Path $packageRoot 'portable-smoke.log'
Set-Content -LiteralPath $log -Value "Commit: $ExpectedCommit`nZIP SHA256: $hash"
$savedPath = $env:PATH
$env:PATH = "$env:SystemRoot\system32;$env:SystemRoot"

function Invoke-PackagedCommand([string] $Executable, [string] $Arguments) {
    $process = Start-Process -FilePath $Executable -ArgumentList $Arguments -WindowStyle Hidden -PassThru
    if (!$process.WaitForExit(60000)) {
        & "$env:SystemRoot\system32\taskkill.exe" /PID $process.Id /T /F | Out-Null
        throw "Packaged command timed out: $Arguments"
    }
    if ($process.ExitCode -ne 0) { throw "Packaged command failed ($($process.ExitCode)): $Arguments" }
    Add-Content -LiteralPath $log -Value "PASS: $Arguments"
}

try {
    $launcher = Join-Path $extractRoot 'bin/UTN.exe'
    Invoke-PackagedCommand $launcher '--version'
    foreach ($sample in @('m1-erased-stroke', 'lesson-roundtrip')) {
        $fixture = Join-Path $extractRoot "samples/$sample.xopp"
        $pdf = Join-Path $extractRoot "$sample.pdf"
        Invoke-PackagedCommand $launcher "--create-pdf=`"$pdf`" `"$fixture`""
        $bytes = [IO.File]::ReadAllBytes($pdf)
        if ($bytes.Length -lt 100 -or [Text.Encoding]::ASCII.GetString($bytes, 0, 5) -ne '%PDF-') {
            throw "Invalid PDF: $sample"
        }
        $png = Join-Path $extractRoot "$sample.png"
        Invoke-PackagedCommand $launcher "--create-img=`"$png`" --export-png-width=800 `"$fixture`""
        $images = @(Get-ChildItem -LiteralPath $extractRoot -Filter "$sample*.png")
        if ($images.Count -eq 0) { throw "Missing PNG: $sample" }
        foreach ($image in $images) {
            $bytes = [IO.File]::ReadAllBytes($image.FullName)
            if ($bytes.Length -lt 8 -or [BitConverter]::ToString($bytes, 0, 8) -ne '89-50-4E-47-0D-0A-1A-0A') {
                throw "Invalid PNG: $($image.Name)"
            }
        }
    }
    # Keep the CI window hidden and wait for the native GUI message loop.
    $app = Start-Process -FilePath (Join-Path $extractRoot 'bin/xournalpp.exe') -WindowStyle Hidden -PassThru
    try {
        if (!$app.WaitForInputIdle(30000)) { throw 'GUI message loop did not become ready' }
        $app.Refresh()
        if ($app.HasExited) { throw "GUI exited during startup: $($app.ExitCode)" }
        Add-Content -LiteralPath $log -Value 'PASS: native GUI message loop ready with system-only PATH (visual interaction unverified)'
    } finally {
        if (!$app.HasExited) { Stop-Process -Id $app.Id -Force }
    }
} finally {
    $env:PATH = $savedPath
}
