$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$aip = Join-Path $projectRoot "bin\x64\Release\ModaDXF.aip"
$diagnostic = Join-Path $projectRoot "bin\x64\Release\ModaDxfDiagnostic.exe"
$wixProject = Join-Path $projectRoot "msi\ModaDXFNative.wixproj"

if (-not (Test-Path -LiteralPath $diagnostic)) {
  & (Join-Path $PSScriptRoot "build-diagnostic.ps1")
}

if (-not (Test-Path -LiteralPath $aip)) {
  throw "ModaDXF.aip nao encontrado em '$aip'. Compile o plugin com scripts\build-plugin.ps1 antes de gerar o MSI."
}

dotnet build $wixProject -c Release /t:Rebuild
if ($LASTEXITCODE -ne 0) {
  throw "Falha ao compilar o MSI nativo."
}

$msi = Join-Path $projectRoot "msi\bin\x64\Release\ModaDXF-Native.msi"
& (Join-Path $PSScriptRoot "patch-msi-ui.ps1") -MsiPath $msi
& (Join-Path $PSScriptRoot "build-setup-exe.ps1") -MsiPath $msi

Write-Host "Gerado: $msi"
Write-Host "Gerado: $(Join-Path $projectRoot 'ModaDXF-Setup.exe')"
