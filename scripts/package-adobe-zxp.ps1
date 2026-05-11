param(
  [string]$Version = "0.1.4",
  [string]$ZxpSignCmd = "",
  [string]$CertificatePath = "",
  [string]$CertificatePassword = "",
  [string]$TimestampUrl = "http://timestamp.digicert.com",
  [switch]$SkipSigning
)

$ErrorActionPreference = "Stop"

function Assert-UnderRoot {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Root,
    [Parameter(Mandatory = $true)]
    [string]$Path
  )

  $resolvedRoot = [System.IO.Path]::GetFullPath($Root)
  $resolvedPath = [System.IO.Path]::GetFullPath($Path)
  if (-not $resolvedPath.StartsWith($resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Caminho fora do projeto: $resolvedPath"
  }
}

$projectRoot = Split-Path -Parent $PSScriptRoot
$distributionRoot = Join-Path $projectRoot "adobe-distribution"
$distRoot = Join-Path $projectRoot "dist\adobe"
$stageRoot = Join-Path $distRoot "ModaDXF-$Version"
$payloadRoot = Join-Path $stageRoot "payload\ModaDXF"
$zxpPath = Join-Path $distRoot "com.modadxf.illustrator.zxp"

$aip = Join-Path $projectRoot "bin\x64\Release\ModaDXF.aip"
$diagnostic = Join-Path $projectRoot "bin\x64\Release\ModaDxfDiagnostic.exe"
$readme = Join-Path $projectRoot "README.md"
$license = Join-Path $projectRoot "msi\License.rtf"
$mxi = Join-Path $distributionRoot "com.modadxf.illustrator.mxi"

foreach ($required in @($aip, $diagnostic, $readme, $license, $mxi)) {
  if (-not (Test-Path -LiteralPath $required)) {
    throw "Arquivo obrigatorio ausente: $required"
  }
}

New-Item -ItemType Directory -Force -Path $distRoot | Out-Null
Assert-UnderRoot -Root $projectRoot -Path $stageRoot
if (Test-Path -LiteralPath $stageRoot) {
  Remove-Item -LiteralPath $stageRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $payloadRoot | Out-Null
Copy-Item -LiteralPath $aip -Destination (Join-Path $payloadRoot "ModaDXF.aip") -Force
Copy-Item -LiteralPath $diagnostic -Destination (Join-Path $payloadRoot "ModaDxfDiagnostic.exe") -Force
Copy-Item -LiteralPath $readme -Destination (Join-Path $payloadRoot "README.md") -Force
Copy-Item -LiteralPath $license -Destination (Join-Path $payloadRoot "LICENSE.rtf") -Force
Copy-Item -LiteralPath $mxi -Destination (Join-Path $stageRoot "com.modadxf.illustrator.mxi") -Force

Write-Host "Staging Adobe gerado: $stageRoot"

if ($SkipSigning) {
  Write-Host "Assinatura ignorada. Valide o staging antes de assinar/submeter."
  exit 0
}

foreach ($required in @($ZxpSignCmd, $CertificatePath, $CertificatePassword)) {
  if ([string]::IsNullOrWhiteSpace($required)) {
    throw "Informe -ZxpSignCmd, -CertificatePath e -CertificatePassword ou use -SkipSigning."
  }
}

if (-not (Test-Path -LiteralPath $ZxpSignCmd)) {
  throw "ZXPSignCmd nao encontrado: $ZxpSignCmd"
}
if (-not (Test-Path -LiteralPath $CertificatePath)) {
  throw "Certificado nao encontrado: $CertificatePath"
}

if (Test-Path -LiteralPath $zxpPath) {
  Remove-Item -LiteralPath $zxpPath -Force
}

& $ZxpSignCmd -sign $stageRoot $zxpPath $CertificatePath $CertificatePassword -tsa $TimestampUrl
if ($LASTEXITCODE -ne 0) {
  throw "Falha ao assinar ZXP."
}

Write-Host "ZXP gerado: $zxpPath"
