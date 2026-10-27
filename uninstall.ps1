# PIPP Modern Camera Support - One-click uninstaller
# Right-click this file -> Run with PowerShell

$ErrorActionPreference = "Stop"

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
    $alt = @("${env:ProgramFiles(x86)}\PIPP", "$env:LOCALAPPDATA\PIPP", "D:\PIPP")
    foreach ($d in $alt) {
        if (Test-Path "$d\PIPP.exe") { $PippDir = $d; break }
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

Write-Host ""
Write-Host "  Uninstalling modern camera support from PIPP" -ForegroundColor White
Write-Host "  Target: $PippDir" -ForegroundColor Gray
Write-Host ""

$changed = $false

# Restore original pipp_dll.dll
if (Test-Path "$PippDir\pipp_dll_orig.dll") {
    Remove-Item "$PippDir\pipp_dll.dll" -Force -ErrorAction SilentlyContinue
    Rename-Item "$PippDir\pipp_dll_orig.dll" -NewName "pipp_dll.dll"
    Write-Ok "Restored original pipp_dll.dll"
    $changed = $true
} else {
    Write-Warn "No backup found (pipp_dll_orig.dll)"
}

# Restore original pipp_core.exe
if (Test-Path "$PippDir\pipp_core_orig.exe") {
    Remove-Item "$PippDir\pipp_core.exe" -Force -ErrorAction SilentlyContinue
    Rename-Item "$PippDir\pipp_core_orig.exe" -NewName "pipp_core.exe"
    Write-Ok "Restored original pipp_core.exe"
    $changed = $true
} else {
    Write-Warn "No backup found (pipp_core_orig.exe)"
}

# Remove added files
foreach ($f in @("raw_r.dll", "vcomp140.dll")) {
    if (Test-Path "$PippDir\$f") {
        Remove-Item "$PippDir\$f" -Force
        Write-Ok "Removed $f"
        $changed = $true
    }
}

Write-Host ""
if ($changed) {
    Write-Ok "Done! PIPP is back to its original state."
} else {
    Write-Warn "Nothing to uninstall."
}
Write-Host ""
Write-Host "Press any key to exit..." -ForegroundColor Gray
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
