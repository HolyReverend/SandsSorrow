param([string]$EngineRoot = 'C:/Program Files/Epic Games/UE_5.8')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
& "$EngineRoot/Engine/Build/BatchFiles/Build.bat" SandsOfSorrowEditor Win64 Development "-Project=$projectRoot/SandsOfSorrow.uproject" -WaitMutex
if ($LASTEXITCODE -ne 0) { throw "Unreal build failed: $LASTEXITCODE" }
