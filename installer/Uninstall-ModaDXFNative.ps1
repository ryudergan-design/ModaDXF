param(
  [Alias("R")]
  [string]$IllustratorRoot = "C:\Program Files\Adobe",
  [Alias("P")]
  [string]$IllustratorPluginPath = ""
)

$ErrorActionPreference = "Stop"

function Normalize-DirectoryPath {
  param([string]$Path)

  if (-not $Path) {
    return ""
  }

  return [System.IO.Path]::GetFullPath($Path.Trim().TrimEnd("\"))
}

if ($IllustratorPluginPath -eq "__AUTO__") {
  $IllustratorPluginPath = ""
}
$removed = $false

$targets = @()

if ($IllustratorPluginPath) {
  $targets += (Join-Path (Normalize-DirectoryPath -Path $IllustratorPluginPath) "ModaDXF")
} else {
  $settingsRoot = Join-Path $env:APPDATA "Adobe"
  if (Test-Path -LiteralPath $settingsRoot) {
    Get-ChildItem -LiteralPath $settingsRoot -Directory -ErrorAction SilentlyContinue |
      Where-Object { $_.Name -like "Adobe Illustrator * Settings" } |
      ForEach-Object {
        Get-ChildItem -LiteralPath $_.FullName -Directory -ErrorAction SilentlyContinue |
          ForEach-Object {
            $targets += (Join-Path (Join-Path (Join-Path $_.FullName "x64") "Plug-ins") "ModaDXF")
          }
      }
  }

  if (Test-Path -LiteralPath $IllustratorRoot) {
    Get-ChildItem -LiteralPath $IllustratorRoot -Directory -ErrorAction SilentlyContinue |
      Where-Object { $_.Name -like "Adobe Illustrator*" } |
      ForEach-Object {
        $targets += (Join-Path (Join-Path $_.FullName "Plug-ins") "ModaDXF")
      }
  }
}

foreach ($target in $targets | Sort-Object -Unique) {
  if (Test-Path -LiteralPath $target) {
    Remove-Item -LiteralPath $target -Recurse -Force
    Write-Host "Removido: $target"
    $removed = $true
  }
}

if (-not $removed) {
  Write-Host "Nenhuma instalacao ModaDXF encontrada."
}
