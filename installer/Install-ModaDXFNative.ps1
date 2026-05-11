param(
  [Alias("S")]
  [string]$PluginSource = (Join-Path (Split-Path -Parent $PSScriptRoot) "bin\x64\Release\ModaDXF.aip"),
  [Alias("D")]
  [string]$DiagnosticSource = (Join-Path (Split-Path -Parent $PSScriptRoot) "bin\x64\Release\ModaDxfDiagnostic.exe"),
  [Alias("R")]
  [string]$IllustratorRoot = "C:\Program Files\Adobe",
  [Alias("V")]
  [string]$IllustratorVersion = "",
  [Alias("P")]
  [string]$IllustratorPluginPath = "",
  [switch]$List,
  [Alias("N")]
  [switch]$NonInteractive
)

$ErrorActionPreference = "Stop"

function Test-IsAdministrator {
  $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
  $principal = New-Object Security.Principal.WindowsPrincipal($identity)
  return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Normalize-DirectoryPath {
  param([string]$Path)

  if (-not $Path) {
    return ""
  }

  return [System.IO.Path]::GetFullPath($Path.Trim().TrimEnd("\"))
}

if ($IllustratorVersion -eq "__AUTO__") {
  $IllustratorVersion = ""
}

if ($IllustratorPluginPath -eq "__AUTO__") {
  $IllustratorPluginPath = ""
}

if (-not (Test-Path -LiteralPath $PluginSource)) {
  $localPlugin = Join-Path $PSScriptRoot "ModaDXF.aip"
  if (Test-Path -LiteralPath $localPlugin) {
    $PluginSource = $localPlugin
  }
}

if (-not (Test-Path -LiteralPath $DiagnosticSource)) {
  $localDiagnostic = Join-Path $PSScriptRoot "ModaDxfDiagnostic.exe"
  if (Test-Path -LiteralPath $localDiagnostic) {
    $DiagnosticSource = $localDiagnostic
  }
}

function Get-IllustratorInstallations {
  param([string]$Root)

  $found = @()

  $settingsRoot = Join-Path $env:APPDATA "Adobe"
  if (Test-Path -LiteralPath $settingsRoot) {
    Get-ChildItem -LiteralPath $settingsRoot -Directory -ErrorAction SilentlyContinue |
      Where-Object { $_.Name -like "Adobe Illustrator * Settings" } |
      ForEach-Object {
        $settingsFolder = $_
        $version = ($settingsFolder.Name -replace '^Adobe Illustrator\s*', '' -replace '\s*Settings$', '').Trim()
        $displayVersion = $version
        $majorVersion = 0
        if ([int]::TryParse($version, [ref]$majorVersion)) {
          $displayVersion = [string]($majorVersion + 1996)
        }
        Get-ChildItem -LiteralPath $settingsFolder.FullName -Directory -ErrorAction SilentlyContinue |
          ForEach-Object {
            $plugIns = Join-Path (Join-Path $_.FullName "x64") "Plug-ins"
            if (Test-Path -LiteralPath $plugIns) {
              $found += [pscustomobject]@{
                Version = $displayVersion
                Name = "$($settingsFolder.Name) $($_.Name) (usuario atual)"
                Root = $_.FullName
                PlugIns = $plugIns
                Scope = "CurrentUser"
              }
            }
          }
      }
  }

  if (Test-Path -LiteralPath $Root) {
    Get-ChildItem -LiteralPath $Root -Directory -ErrorAction SilentlyContinue |
      Where-Object { $_.Name -like "Adobe Illustrator*" } |
      ForEach-Object {
        $plugIns = Join-Path $_.FullName "Plug-ins"
        if (Test-Path -LiteralPath $plugIns) {
          $found += [pscustomobject]@{
            Version = ($_.Name -replace '^Adobe Illustrator\s*', '').Trim()
            Name = $_.Name
            Root = $_.FullName
            PlugIns = $plugIns
            Scope = "Machine"
          }
        }
      }
  }

  $uninstallRoots = @(
    "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall",
    "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall"
  )

  foreach ($registryRoot in $uninstallRoots) {
    if (-not (Test-Path -LiteralPath $registryRoot)) {
      continue
    }

    Get-ChildItem -LiteralPath $registryRoot -ErrorAction SilentlyContinue |
      ForEach-Object {
        $item = Get-ItemProperty -LiteralPath $_.PSPath -ErrorAction SilentlyContinue
        if ($item.DisplayName -like "Adobe Illustrator*" -and $item.InstallLocation) {
          $plugIns = Join-Path $item.InstallLocation "Plug-ins"
          if (Test-Path -LiteralPath $plugIns) {
            $found += [pscustomobject]@{
              Version = ($item.DisplayName -replace '^Adobe Illustrator\s*', '').Trim()
              Name = $item.DisplayName
              Root = $item.InstallLocation
              PlugIns = $plugIns
              Scope = "Machine"
            }
          }
        }
      }
  }

  $found |
    Sort-Object PlugIns -Unique |
    Sort-Object @{ Expression = { if ($_.Scope -eq "CurrentUser") { 0 } else { 1 } } }, Name
}

function Select-Target {
  param(
    [array]$Installations,
    [string]$Version,
    [string]$PluginPath,
    [switch]$NonInteractive
  )

  if ($PluginPath) {
    $normalizedPluginPath = Normalize-DirectoryPath -Path $PluginPath
    if ((Split-Path -Leaf $normalizedPluginPath) -ieq "ModaDXF") {
      $normalizedPluginPath = Split-Path -Parent $normalizedPluginPath
    }
    return [pscustomobject]@{
      Version = "custom"
      Name = "Custom Illustrator Plug-ins"
      Root = (Split-Path -Parent $normalizedPluginPath)
      PlugIns = $normalizedPluginPath
    }
  }

  if ($Version) {
    $match = $Installations | Where-Object { $_.Version -eq $Version -or $_.Name -like "*$Version*" } | Select-Object -First 1
    if ($match) {
      return $match
    }
    throw "Nenhuma instalacao do Illustrator encontrada para a versao '$Version'."
  }

  if ($Installations.Count -eq 1) {
    return $Installations[0]
  }

  if ($Installations.Count -eq 0) {
    throw "Nenhuma instalacao do Adobe Illustrator foi encontrada."
  }

  if ($NonInteractive) {
    $list = ($Installations | ForEach-Object { "- $($_.Name): $($_.PlugIns)" }) -join [Environment]::NewLine
    throw "Mais de uma instalacao do Illustrator encontrada. Informe ILLUSTRATORVERSION ou ILLUSTRATORPLUGINPATH no MSI.`n$list"
  }

  Write-Host "Selecione a versao do Adobe Illustrator para instalar o ModaDXF:"
  for ($i = 0; $i -lt $Installations.Count; $i++) {
    Write-Host ("[{0}] {1} -> {2}" -f ($i + 1), $Installations[$i].Name, $Installations[$i].PlugIns)
  }

  $choice = Read-Host "Numero"
  $index = [int]$choice - 1
  if ($index -lt 0 -or $index -ge $Installations.Count) {
    throw "Selecao invalida."
  }

  return $Installations[$index]
}

if ($List) {
  Get-IllustratorInstallations -Root $IllustratorRoot | ConvertTo-Json -Depth 4
  exit 0
}

if (-not (Test-Path -LiteralPath $PluginSource)) {
  throw "ModaDXF.aip nao encontrado: $PluginSource. Compile o plugin nativo antes de instalar."
}

$installations = @(Get-IllustratorInstallations -Root $IllustratorRoot)
$target = Select-Target -Installations $installations -Version $IllustratorVersion -PluginPath $IllustratorPluginPath -NonInteractive:$NonInteractive

$destinationRoot = Join-Path $target.PlugIns "ModaDXF"
$destinationPlugin = Join-Path $destinationRoot "ModaDXF.aip"
$programData = Join-Path $env:ProgramData "ModaDXF"
$installLog = Join-Path $programData "install.log"

if ($destinationRoot -like "$env:ProgramFiles\*" -and -not (Test-IsAdministrator)) {
  throw "Permissao administrativa necessaria para instalar em '$destinationRoot'. Execute o instalador como administrador."
}

New-Item -ItemType Directory -Force -Path $destinationRoot | Out-Null
New-Item -ItemType Directory -Force -Path $programData | Out-Null

if ([System.IO.Path]::GetFullPath($PluginSource) -ine [System.IO.Path]::GetFullPath($destinationPlugin)) {
  Copy-Item -LiteralPath $PluginSource -Destination $destinationPlugin -Force
}

if (Test-Path -LiteralPath $DiagnosticSource) {
  $destinationDiagnostic = Join-Path $destinationRoot "ModaDxfDiagnostic.exe"
  if ([System.IO.Path]::GetFullPath($DiagnosticSource) -ine [System.IO.Path]::GetFullPath($destinationDiagnostic)) {
    Copy-Item -LiteralPath $DiagnosticSource -Destination $destinationDiagnostic -Force
  }
}

$message = "$(Get-Date -Format o) Installed ModaDXF to $destinationPlugin"
Add-Content -LiteralPath $installLog -Value $message
Write-Host $message
