param(
    [Parameter(Mandatory = $true)]
    [string[]] $Reports
)

$ErrorActionPreference = 'Stop'
foreach ($report in $Reports) {
    [xml] $document = Get-Content -LiteralPath $report -Raw
    $root = $document.testsuites
    if ($null -eq $root -or [int] $root.tests -le 0) {
        throw "No tests recorded in $report"
    }
    foreach ($attribute in @('failures', 'errors', 'disabled')) {
        if (-not $root.HasAttribute($attribute) -or [int] $root.GetAttribute($attribute) -ne 0) {
            throw "Missing or nonzero $attribute in $report"
        }
    }
    # CrashChild.run is the one skip that is by design: it is the child process of the CrashHandler tests, which start it
    # with UTN_CRASH_CHILD and UTN_CRASH_FILE set. Run on its own it skips itself. Every other skip still fails the check.
    $incomplete = $document.SelectNodes('//testcase[(@status="notrun" or @result="skipped" or @result="suppressed" or skipped) and not(@classname="CrashChild" and @name="run")]')
    if ($incomplete.Count -ne 0) {
        throw "$($incomplete.Count) skipped or disabled tests in $report"
    }
    Write-Output "$report : $($root.tests) tests passed, none disabled or skipped"
}
