# PIPP Modern Camera Support Installer/Uninstaller
# Adds LibRaw-based modern camera RAW file support to PIPP v2.5.9.
#
# Usage (from an admin PowerShell, or it will self-elevate):
#   .\install_modern_camera_support.ps1              # Install
#   .\install_modern_camera_support.ps1 -Uninstall   # Uninstall

param(
    [string]$PippDir = "C:\Program Files\PIPP",
    [switch]$Uninstall
)

$ErrorActionPreference = "Stop"

function Write-Status($msg) { Write-Host "[PIPP Patcher] $msg" -ForegroundColor Cyan }
function Write-Ok($msg)     { Write-Host "[OK] $msg" -ForegroundColor Green }
function Write-Warn($msg)   { Write-Host "[WARN] $msg" -ForegroundColor Yellow }
function Write-Err($msg)    { Write-Host "[ERROR] $msg" -ForegroundColor Red }

# --- Self-elevate to admin if needed ---
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Status "Requesting administrator privileges..."
    $argList = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -PippDir `"$PippDir`""
    if ($Uninstall) { $argList += " -Uninstall" }
    try {
        Start-Process powershell.exe -ArgumentList $argList -Verb RunAs -Wait
    } catch {
        Write-Err "Administrator privileges are required to modify $PippDir"
        Write-Host "Right-click PowerShell -> Run as administrator, then try again." -ForegroundColor Yellow
    }
    exit
}

# --- Check PIPP isn't running ---
$pippProcs = Get-Process -Name "PIPP","pipp_core","pipp_core_orig" -ErrorAction SilentlyContinue
if ($pippProcs) {
    Write-Err "PIPP is currently running. Please close it first."
    $pippProcs | ForEach-Object { Write-Host "  - $($_.ProcessName) (PID $($_.Id))" -ForegroundColor Yellow }
    Write-Host ""
    Write-Host "Press any key to exit..." -ForegroundColor Gray
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    exit 1
}

$scriptDir = Split-Path -Parent $PSCommandPath

# ===== UNINSTALL =====
if ($Uninstall) {
    Write-Status "Uninstalling modern camera support from $PippDir..."
    Write-Host ""

    $changed = $false

    # Restore original pipp_dll.dll
    if (Test-Path "$PippDir\pipp_dll_orig.dll") {
        Remove-Item "$PippDir\pipp_dll.dll" -Force -ErrorAction SilentlyContinue
        Rename-Item "$PippDir\pipp_dll_orig.dll" -NewName "pipp_dll.dll"
        Write-Ok "Restored original pipp_dll.dll"
        $changed = $true
    } else {
        Write-Warn "No backup found (pipp_dll_orig.dll) - skipping DLL restore"
    }

    # Restore original pipp_core.exe
    if (Test-Path "$PippDir\pipp_core_orig.exe") {
        Remove-Item "$PippDir\pipp_core.exe" -Force -ErrorAction SilentlyContinue
        Rename-Item "$PippDir\pipp_core_orig.exe" -NewName "pipp_core.exe"
        Write-Ok "Restored original pipp_core.exe"
        $changed = $true
    } else {
        Write-Warn "No backup found (pipp_core_orig.exe) - skipping exe restore"
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
        Write-Ok "Uninstall complete. PIPP is back to its original state."
    } else {
        Write-Warn "Nothing to uninstall - modern camera support was not installed."
    }
    Write-Host ""
    Write-Host "Press any key to exit..." -ForegroundColor Gray
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    return
}

# ===== INSTALL =====
Write-Status "Installing modern camera support for PIPP..."
Write-Status "PIPP directory: $PippDir"
Write-Host ""

# Verify PIPP installation
if (-not (Test-Path "$PippDir\PIPP.exe")) {
    throw "PIPP.exe not found in $PippDir. Use -PippDir to specify PIPP's location."
}

# Check for required patch files next to this script
foreach ($f in @("pipp_dll.dll", "pipp_core.exe")) {
    if (-not (Test-Path "$scriptDir\$f")) {
        throw "Required file missing: $scriptDir\$f"
    }
}

# Locate raw_r.dll from rawpy (pip install rawpy)
$rawpyDir = $null
try {
    $rawpyDir = python -I -c "import rawpy, os; print(os.path.dirname(rawpy.__file__))" 2>$null
} catch {}

if (-not $rawpyDir -or -not (Test-Path "$rawpyDir\raw_r.dll")) {
    Write-Err "Cannot find raw_r.dll (LibRaw)."
    Write-Host "  Install rawpy first:  pip install rawpy" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Press any key to exit..." -ForegroundColor Gray
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    exit 1
}

Write-Ok "Found LibRaw at: $rawpyDir\raw_r.dll"

# Step 1: Backup originals
Write-Status "Backing up original files..."

if (-not (Test-Path "$PippDir\pipp_dll_orig.dll")) {
    Copy-Item "$PippDir\pipp_dll.dll" "$PippDir\pipp_dll_orig.dll"
    Write-Ok "Backed up pipp_dll.dll -> pipp_dll_orig.dll"
} else {
    Write-Warn "pipp_dll_orig.dll already exists (previous install)"
}

if (-not (Test-Path "$PippDir\pipp_core_orig.exe")) {
    Rename-Item "$PippDir\pipp_core.exe" -NewName "pipp_core_orig.exe"
    Write-Ok "Backed up pipp_core.exe -> pipp_core_orig.exe"
} else {
    Write-Warn "pipp_core_orig.exe already exists (previous install)"
    Remove-Item "$PippDir\pipp_core.exe" -Force -ErrorAction SilentlyContinue
}

# Step 2: Copy LibRaw
Write-Status "Installing LibRaw..."
Copy-Item "$rawpyDir\raw_r.dll" "$PippDir\raw_r.dll" -Force
Write-Ok "Installed raw_r.dll (LibRaw)"

if (Test-Path "$rawpyDir\vcomp140.dll") {
    Copy-Item "$rawpyDir\vcomp140.dll" "$PippDir\vcomp140.dll" -Force
    Write-Ok "Installed vcomp140.dll (OpenMP runtime)"
}

# Step 3: Install proxy DLL
Write-Status "Installing proxy DLL..."
Copy-Item "$scriptDir\pipp_dll.dll" "$PippDir\pipp_dll.dll" -Force
Write-Ok "Installed pipp_dll.dll (LibRaw-enabled proxy)"

# Step 4: Install pipp_core wrapper
Write-Status "Installing pipp_core wrapper..."
Copy-Item "$scriptDir\pipp_core.exe" "$PippDir\pipp_core.exe" -Force
Write-Ok "Installed pipp_core.exe (RAW conversion wrapper)"

# Done
Write-Host ""
Write-Ok "Installation complete!"
Write-Host ""
Write-Host "  PIPP now supports modern cameras including:" -ForegroundColor White
Write-Host "    - Sony Alpha 1 II, A7R V, A7 IV, etc." -ForegroundColor Gray
Write-Host "    - Nikon Z 9, Z 8, Z 7 II, Z 6 III, etc." -ForegroundColor Gray
Write-Host "    - Canon EOS R5 II, R6 III, R1, etc." -ForegroundColor Gray
Write-Host "    - Fujifilm X-H2S, X-T5, GFX 100 II, etc." -ForegroundColor Gray
Write-Host "    - OM System OM-1, OM-5, etc." -ForegroundColor Gray
Write-Host "    - And hundreds more via LibRaw 0.22" -ForegroundColor Gray
Write-Host ""
Write-Host "  To uninstall:" -ForegroundColor Yellow
Write-Host "    .\install_modern_camera_support.ps1 -Uninstall" -ForegroundColor Yellow
Write-Host ""
Write-Host "Press any key to exit..." -ForegroundColor Gray
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
