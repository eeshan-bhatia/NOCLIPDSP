param(
    [string]$PluginPath = "build-native/noclip.DSP_artefacts/Release/VST3/noclip.DSP.vst3",
    [string]$ValidatorPath = ".tools/pluginval/pluginval.exe",
    [string]$OutputDirectory = "build-native"
)
$ErrorActionPreference = "Stop"
$plugin = (Resolve-Path -LiteralPath $PluginPath).Path
$validator = (Resolve-Path -LiteralPath $ValidatorPath).Path
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$output = (Resolve-Path -LiteralPath $OutputDirectory).Path
# Explicitly wait for this Windows GUI-subsystem executable.
$arguments = '--strictness-level 5 --random-seed 12345 --timeout-ms 60000 --output-dir "' +
    $output + '" --output-filename pluginval-level5.txt --validate "' + $plugin + '"'
$process = Start-Process -FilePath $validator -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
Get-Content -LiteralPath (Join-Path $output "pluginval-level5.txt") -Tail 12
if ($process.ExitCode -ne 0) { throw "pluginval failed with exit code $($process.ExitCode)" }
Write-Output "pluginval exit code: 0"
