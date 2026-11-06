# Launches an installed / unzipped BlackMiro.exe the way a user would (no Qt on PATH),
# waits for the main window to render and saves a screenshot. Fails if the app
# does not start, shows a QML load error, or produces no screenshot.
#   pwsh scripts/smoke-test-windows.ps1 -Exe <path\BlackMiro.exe> -Name portable -OutDir <dir>
param([Parameter(Mandatory)][string]$Exe, [Parameter(Mandatory)][string]$Name, [string]$OutDir = $env:RUNNER_TEMP)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path $Exe)) { throw "${Name}: $Exe not found" }
New-Item -ItemType Directory $OutDir -Force | Out-Null
$OutDir = (Resolve-Path $OutDir).Path

function Invoke-App([string]$tag, [hashtable]$extraEnv) {
    $shot = Join-Path $OutDir "$Name$tag.png"
    $log = Join-Path $OutDir "$Name$tag.log"
    Remove-Item $shot, $log -ErrorAction SilentlyContinue
    $saved = @{}
    $vars = @{ PATH = "$env:SystemRoot\system32;$env:SystemRoot"; BLACKMIRO_NO_ERROR_DIALOG = '1'
               BLACKMIRO_SETTINGS_ORG = 'BlackMiroSmokeTest'; QT_FORCE_STDERR_LOGGING = '1' } + $extraEnv
    foreach ($k in $vars.Keys) { $saved[$k] = [Environment]::GetEnvironmentVariable($k); [Environment]::SetEnvironmentVariable($k, $vars[$k]) }
    try {
        $p = Start-Process -FilePath $Exe -ArgumentList '--screenshot', "`"$shot`"", '--screenshot-delay', '4000' `
             -WorkingDirectory (Split-Path $Exe) -RedirectStandardError $log -PassThru
        $null = $p.Handle   # keeps ExitCode readable after exit
        if (-not $p.WaitForExit(90000)) { $p.Kill(); Write-Host "${Name}${tag}: timed out" }
        $p.WaitForExit()
        $code = $p.ExitCode
    } finally {
        foreach ($k in $saved.Keys) { [Environment]::SetEnvironmentVariable($k, $saved[$k]) }
    }
    $out = if (Test-Path $log) { Get-Content $log -Raw } else { '' }
    if ($out) { Write-Host "---- ${Name}${tag} log ----`n$out" }
    $ok = ($code -eq 0) -and (Test-Path $shot) -and ((Get-Item $shot).Length -gt 1000) -and ($out -notmatch 'could not start|failed to load component')
    Write-Host "${Name}${tag}: exit=$code screenshot=$(Test-Path $shot) ok=$ok"
    return $ok
}

if (Invoke-App '' @{}) { "$Name OK"; exit 0 }
# Distinguish "app is broken" from "this CI machine has no usable GPU".
if (Invoke-App '-software' @{ QT_QUICK_BACKEND = 'software' }) {
    Write-Host "::warning::$Name only renders with the software backend on this runner (no GPU); app itself starts fine."
    exit 0
}
throw "$Name failed to start - see log above"
