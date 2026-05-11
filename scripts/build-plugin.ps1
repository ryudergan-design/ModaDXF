$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $projectRoot "build\ModaDxfIllustratorPlugin.vcxproj"
$msbuild = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
$pythonExe = $env:MODADXF_PYTHON_EXE

if (-not $env:AI_SDK_ROOT) {
  throw "AI_SDK_ROOT nao definido. Instale o Adobe Illustrator SDK e defina AI_SDK_ROOT para compilar o .aip."
}

if (-not (Test-Path -LiteralPath $msbuild)) {
  $msbuild = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
}

if (-not (Test-Path -LiteralPath $msbuild)) {
  throw "MSBuild do Visual Studio 2022 nao encontrado."
}

if (-not $pythonExe) {
  $bundledPython = Join-Path $env:USERPROFILE ".cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
  if (Test-Path -LiteralPath $bundledPython) {
    $pythonExe = $bundledPython
  } else {
    $pythonExe = "python"
  }
}

& $msbuild $project /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:PythonExe="$pythonExe" /m
if ($LASTEXITCODE -ne 0) {
  throw "Falha ao compilar ModaDXF.aip."
}

$aip = Join-Path $projectRoot "bin\x64\Release\ModaDXF.aip"
Write-Host "Gerado: $aip"
