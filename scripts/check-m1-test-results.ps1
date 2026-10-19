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
    $incomplete = $document.SelectNodes('//testcase[@status="notrun" or @result="skipped" or @result="suppressed" or skipped]')
    if ($incomplete.Count -ne 0) {
        throw "$($incomplete.Count) skipped or disabled tests in $report"
    }
    Write-Output "$report : $($root.tests) tests passed, none disabled or skipped"
}
