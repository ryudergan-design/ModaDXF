$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $projectRoot "build\ModaDxfDiagnostic.vcxproj"
$msbuild = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"

if (-not (Test-Path -LiteralPath $msbuild)) {
  $msbuild = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
}

if (-not (Test-Path -LiteralPath $msbuild)) {
  throw "MSBuild do Visual Studio 2022 nao encontrado."
}

& $msbuild $project /p:Configuration=Release /p:Platform=x64 /m
if ($LASTEXITCODE -ne 0) {
  throw "Falha ao compilar ModaDxfDiagnostic."
}

$exe = Join-Path $projectRoot "bin\x64\Release\ModaDxfDiagnostic.exe"
Write-Host "Gerado: $exe"
