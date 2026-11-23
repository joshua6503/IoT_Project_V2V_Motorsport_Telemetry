<#
.SYNOPSIS
  Starts the V2V Node-RED dashboard on this laptop.

.DESCRIPTION
  Node-RED runs from a user directory OUTSIDE the repo (default %USERPROFILE%\.node-red-v2v), so the
  encrypted credential store never lands in git. The broker address comes from -Broker; the MQTT
  user/password are asked once with hidden input and kept in Node-RED's credential store
  (flows_cred.json, encrypted), never in flows.json.

.EXAMPLE
  .\dashboard\start-dashboard.ps1 -Broker RaspPi5.local            # home mode / hotspot
  .\dashboard\start-dashboard.ps1 -Broker 10.42.0.1                # pit mode
  .\dashboard\start-dashboard.ps1 -Broker localhost -NoAuth        # laptop dev broker (anonymous)
  .\dashboard\start-dashboard.ps1 -Broker RaspPi5.local -AskCredentials   # change user/password
#>
param(
    [Parameter(Mandatory = $true)][string]$Broker,
    [int]$Port = 1883,
    [string]$UserDir = (Join-Path $env:USERPROFILE ".node-red-v2v"),
    [int]$HttpPort = 1880,
    [switch]$NoAuth,
    [switch]$AskCredentials,
    [switch]$ResetFlows
)

$ErrorActionPreference = "Stop"
$env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")
$repoFlows = Join-Path $PSScriptRoot "node-red\flows.json"

if (-not (Get-Command node -ErrorAction SilentlyContinue)) {
    throw "Node.js not found. Install it with: winget install OpenJS.NodeJS.LTS (then open a new terminal)."
}

# 1. one-time install of Node-RED + Dashboard 2.0 in the user directory
New-Item -ItemType Directory -Force $UserDir | Out-Null
if (-not (Test-Path (Join-Path $UserDir "node_modules\node-red\package.json")) -or
    -not (Test-Path (Join-Path $UserDir "node_modules\@flowfuse\node-red-dashboard\package.json"))) {
    Write-Host "Installing Node-RED and @flowfuse/node-red-dashboard in $UserDir ..."
    Push-Location $UserDir
    if (-not (Test-Path package.json)) { '{"name":"node-red-v2v","version":"1.0.0","private":true}' | Set-Content -Encoding ascii package.json }
    npm install --no-audit --no-fund node-red@5.0.10 "@flowfuse/node-red-dashboard"
    Pop-Location
}

# 2. settings: editor and admin API reachable only from this laptop
$settings = Join-Path $UserDir "settings.js"
if (-not (Test-Path $settings)) {
    @"
module.exports = {
    flowFile: 'flows.json',
    uiHost: '127.0.0.1',
    uiPort: process.env.PORT || $HttpPort,
    flowFilePretty: true,
    logging: { console: { level: 'info', metrics: false, audit: false } },
    functionExternalModules: false,
    editorTheme: { projects: { enabled: false } }
};
"@ | Set-Content -Encoding utf8 $settings
}

# 3. flows: copy from the repo on first run or with -ResetFlows
$live = Join-Path $UserDir "flows.json"
if ($ResetFlows -or -not (Test-Path $live)) {
    Copy-Item $repoFlows $live -Force
    Write-Host "flows.json copied from the repo."
}

# 4. broker address through environment variables (used by the broker node)
$env:V2V_BROKER_HOST = $Broker
$env:V2V_BROKER_PORT = "$Port"

# 5. credentials: asked once (hidden), pushed through the local admin API to Node-RED's encrypted store
$credFile = Join-Path $UserDir "flows_cred.json"
$needCreds = (-not $NoAuth) -and ($AskCredentials -or -not (Test-Path $credFile))
$job = $null
if ($needCreds) {
    $mqttUser = Read-Host "MQTT user [dashboard]"
    if ([string]::IsNullOrWhiteSpace($mqttUser)) { $mqttUser = "dashboard" }
    $secure = Read-Host "MQTT password for '$mqttUser' (hidden)" -AsSecureString
    $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
    $plain = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
    $job = Start-Job -ArgumentList $HttpPort, $mqttUser, $plain -ScriptBlock {
        param($http, $u, $p)
        $base = "http://127.0.0.1:$http"
        for ($i = 0; $i -lt 90; $i++) {
            try { $flows = Invoke-RestMethod "$base/flows" -TimeoutSec 2; break } catch { Start-Sleep 1; $flows = $null }
        }
        if (-not $flows) { return "admin API did not come up" }
        foreach ($n in $flows) {
            if ($n.type -eq "mqtt-broker") {
                $n | Add-Member -NotePropertyName credentials -NotePropertyValue ([pscustomobject]@{ user = $u; password = $p }) -Force
            }
        }
        $body = ConvertTo-Json -InputObject $flows -Depth 30
        Invoke-RestMethod "$base/flows" -Method Post -ContentType "application/json; charset=utf-8" `
            -Headers @{ "Node-RED-Deployment-Type" = "full" } -Body ([Text.Encoding]::UTF8.GetBytes($body)) | Out-Null
        return "credentials stored"
    }
    Remove-Variable plain, secure
}

Write-Host ""
Write-Host "Broker:     ${Broker}:${Port}" -ForegroundColor Cyan
Write-Host "Dashboard:  http://localhost:$HttpPort/dashboard" -ForegroundColor Cyan
Write-Host "Editor:     http://localhost:$HttpPort/   (Ctrl+C to stop)" -ForegroundColor Cyan
Write-Host ""

try {
    & node (Join-Path $UserDir "node_modules\node-red\red.js") --userDir $UserDir
}
finally {
    if ($job) { Receive-Job $job -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "[credentials] $_" }; Remove-Job $job -Force -ErrorAction SilentlyContinue }
}
