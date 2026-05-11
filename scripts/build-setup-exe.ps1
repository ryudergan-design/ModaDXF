param(
  [Parameter(Mandatory = $true)]
  [string]$MsiPath
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $projectRoot "tools\ModaDXFSetupLauncher.cs"
$icon = Join-Path $projectRoot "msi\assets\ModaDXF.ico"
$output = Join-Path $projectRoot "ModaDXF-Setup.exe"
$resolvedMsi = (Resolve-Path -LiteralPath $MsiPath).Path

$csc = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\Roslyn\csc.exe"
if (-not (Test-Path -LiteralPath $csc)) {
  throw "csc.exe nao encontrado para gerar o instalador com icone proprio."
}

$compilerArgs = @(
  "/nologo",
  "/target:winexe",
  "/platform:x64",
  "/optimize+",
  "/win32icon:$icon",
  "/resource:$resolvedMsi,ModaDXFNativeMsi",
  "/out:$output",
  "/reference:System.Windows.Forms.dll",
  $source
)

& $csc @compilerArgs
if ($LASTEXITCODE -ne 0) {
  throw "Falha ao gerar ModaDXF-Setup.exe."
}

Write-Host "Gerado: $output"
