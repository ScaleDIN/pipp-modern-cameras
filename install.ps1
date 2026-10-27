# PIPP Modern Camera Support - One-click installer
# Right-click this file -> Run with PowerShell

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $PSCommandPath

function Write-Ok($msg)   { Write-Host "[OK] $msg" -ForegroundColor Green }
function Write-Warn($msg) { Write-Host "[WARN] $msg" -ForegroundColor Yellow }
function Write-Err($msg)  { Write-Host "[ERROR] $msg" -ForegroundColor Red }

# --- Self-elevate ---
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    try {
        Start-Process powershell.exe -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`"" -Verb RunAs -Wait
    } catch {
        Write-Err "Administrator privileges required. Right-click -> Run as administrator."
    }
    exit
}

# --- Find PIPP ---
$PippDir = "C:\Program Files\PIPP"
if (-not (Test-Path "$PippDir\PIPP.exe")) {
    # Try common alternatives
    $alt = @("${env:ProgramFiles(x86)}\PIPP", "$env:LOCALAPPDATA\PIPP", "D:\PIPP")
    $found = $false
    foreach ($d in $alt) {
        if (Test-Path "$d\PIPP.exe") { $PippDir = $d; $found = $true; break }
    }
    if (-not $found) {
        Write-Err "Cannot find PIPP installation."
        Write-Host "  Expected: $PippDir\PIPP.exe" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Press any key to exit..." -ForegroundColor Gray
        $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
        exit 1
    }
}

# --- Check PIPP isn't running ---
$pippProcs = Get-Process -Name "PIPP","pipp_core","pipp_core_orig" -ErrorAction SilentlyContinue
if ($pippProcs) {
    Write-Err "PIPP is currently running. Please close it first."
    Write-Host ""
    Write-Host "Press any key to exit..." -ForegroundColor Gray
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    exit 1
}

# --- Check required files ---
$needed = @("pipp_dll.dll", "pipp_core.exe", "raw_r.dll")
foreach ($f in $needed) {
    if (-not (Test-Path "$scriptDir\$f")) {
        Write-Err "Missing file: $f"
        Write-Host "  Make sure all files from the zip are in the same folder." -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Press any key to exit..." -ForegroundColor Gray
        $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
        exit 1
    }
}

Write-Host ""
Write-Host "  Installing modern camera support for PIPP" -ForegroundColor White
Write-Host "  Target: $PippDir" -ForegroundColor Gray
Write-Host ""

# --- Backup originals ---
if (-not (Test-Path "$PippDir\pipp_dll_orig.dll")) {
    Copy-Item "$PippDir\pipp_dll.dll" "$PippDir\pipp_dll_orig.dll"
    Write-Ok "Backed up pipp_dll.dll"
} else {
    Write-Warn "Previous backup exists (pipp_dll_orig.dll)"
}

if (-not (Test-Path "$PippDir\pipp_core_orig.exe")) {
    Rename-Item "$PippDir\pipp_core.exe" -NewName "pipp_core_orig.exe"
    Write-Ok "Backed up pipp_core.exe"
} else {
    Write-Warn "Previous backup exists (pipp_core_orig.exe)"
    Remove-Item "$PippDir\pipp_core.exe" -Force -ErrorAction SilentlyContinue
}

# --- Install ---
Copy-Item "$scriptDir\pipp_dll.dll" "$PippDir\pipp_dll.dll" -Force
Write-Ok "Installed pipp_dll.dll (LibRaw-enabled proxy)"

Copy-Item "$scriptDir\pipp_core.exe" "$PippDir\pipp_core.exe" -Force
Write-Ok "Installed pipp_core.exe (RAW conversion wrapper)"

Copy-Item "$scriptDir\raw_r.dll" "$PippDir\raw_r.dll" -Force
Write-Ok "Installed raw_r.dll (LibRaw)"

if (Test-Path "$scriptDir\vcomp140.dll") {
    Copy-Item "$scriptDir\vcomp140.dll" "$PippDir\vcomp140.dll" -Force
    Write-Ok "Installed vcomp140.dll (OpenMP runtime)"
}

Write-Host ""
Write-Ok "Done! Launch PIPP normally - modern camera RAW files will now be accepted."
Write-Host ""
Write-Host "  Supported: Sony A1 II, Nikon Z 9/Z 7 II, Canon R5 II, Fujifilm X-H2S," -ForegroundColor Gray
Write-Host "  OM System OM-1, Panasonic S5 II, and hundreds more." -ForegroundColor Gray
Write-Host ""
Write-Host "  To uninstall, run uninstall.ps1" -ForegroundColor Yellow
Write-Host ""
Write-Host "Press any key to exit..." -ForegroundColor Gray
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
