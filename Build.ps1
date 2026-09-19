$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$env:PLATFORMIO_CORE_DIR = Join-Path $PSScriptRoot 'work/platformio'
$python = Join-Path $PSScriptRoot 'work/tools/Scripts/python.exe'
if (!(Test-Path -LiteralPath $python)) {
  py -3.11 -m venv work/tools
  if ($LASTEXITCODE) { throw 'venv failed' }
}
& $python -m pip install --index-url https://pypi.org/simple esphome==2026.6.5
if ($LASTEXITCODE) { throw 'Dependency installation failed' }
& $python -m esphome compile haier-s3.yaml
if ($LASTEXITCODE) { throw 'Compilation failed' }
