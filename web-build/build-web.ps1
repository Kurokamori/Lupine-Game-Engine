#Requires -Version 5.1
<#
.SYNOPSIS
    Build Lupine Engine for WebAssembly using Emscripten.

.DESCRIPTION
    This script configures and builds the Lupine Engine web export template.
    It requires Emscripten SDK to be installed and activated in the current session.

.PARAMETER DebugBuild
    Build with debug symbols and assertions.

.PARAMETER ReleaseBuild
    Build optimized release (default).

.PARAMETER Clean
    Clean the build directory before building.

.PARAMETER Serve
    Start a local test server after building.

.PARAMETER Jobs
    Number of parallel build jobs (default: auto-detect).

.PARAMETER Generator
    CMake generator to use (Ninja or "Unix Makefiles").

.PARAMETER Help
    Show this help message.

.EXAMPLE
    .\build-web.ps1
    Build release template.

.EXAMPLE
    .\build-web.ps1 -Debug -Clean
    Clean and build debug template.

.EXAMPLE
    .\build-web.ps1 -Serve
    Build and start test server.
#>

param(
    [switch]$DebugBuild,
    [switch]$ReleaseBuild,
    [switch]$Clean,
    [switch]$Serve,
    [int]$Jobs = 0,
    [string]$Generator = "",
    [switch]$Help
)

# Script configuration
$ErrorActionPreference = "Stop"
# Prevent PowerShell 7+ from treating stderr output as errors
if ($PSVersionTable.PSVersion.Major -ge 7) {
    $PSNativeCommandUseErrorActionPreference = $false
}
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$BuildDir = Join-Path $ScriptDir "build-web"
$OutputDir = Join-Path $BuildDir "bin"
# The debug template links to its own directory: its files are named index.*
# exactly like the release template's and would otherwise overwrite them.
$DebugOutputDir = Join-Path $OutputDir "web_debug"

# Colors for output
function Write-Header { param([string]$Text) Write-Host "`n$("=" * 60)" -ForegroundColor Cyan; Write-Host "  $Text" -ForegroundColor Cyan; Write-Host "$("=" * 60)`n" -ForegroundColor Cyan }
function Write-Step { param([string]$Text) Write-Host "[*] $Text" -ForegroundColor Green }
function Write-Info { param([string]$Text) Write-Host "    $Text" -ForegroundColor Gray }
function Write-Warn { param([string]$Text) Write-Host "[!] $Text" -ForegroundColor Yellow }
function Write-Err { param([string]$Text) Write-Host "[X] $Text" -ForegroundColor Red }

# Show help
if ($Help) {
    Get-Help $MyInvocation.MyCommand.Path -Detailed
    exit 0
}

Write-Header "Lupine Engine - Web Build"

# Determine build type
$BuildType = if ($DebugBuild) { "Debug" } else { "Release" }
Write-Step "Build type: $BuildType"

# Check for Emscripten
Write-Step "Checking Emscripten..."
$EmccPath = Get-Command "emcc" -ErrorAction SilentlyContinue
if (-not $EmccPath) {
    Write-Err "Emscripten not found in PATH!"
    Write-Info ""
    Write-Info "Please install and activate Emscripten SDK:"
    Write-Info "  1. git clone https://github.com/emscripten-core/emsdk.git"
    Write-Info "  2. cd emsdk"
    Write-Info "  3. .\emsdk.ps1 install latest"
    Write-Info "  4. .\emsdk.ps1 activate latest"
    Write-Info "  5. .\emsdk_env.ps1"
    Write-Info ""
    exit 1
}
$EmccVersion = & emcc --version 2>&1 | Select-Object -First 1
Write-Info "Found: $EmccVersion"

$EmcmakePath = Get-Command "emcmake" -ErrorAction SilentlyContinue
if (-not $EmcmakePath) {
    Write-Err "emcmake not found! Emscripten may not be properly activated."
    exit 1
}

# Check for CMake
Write-Step "Checking CMake..."
$CmakePath = Get-Command "cmake" -ErrorAction SilentlyContinue
if (-not $CmakePath) {
    Write-Err "CMake not found in PATH!"
    exit 1
}
$CmakeVersion = & cmake --version | Select-Object -First 1
Write-Info "Found: $CmakeVersion"

# Determine generator
if (-not $Generator) {
    $NinjaPath = Get-Command "ninja" -ErrorAction SilentlyContinue
    if ($NinjaPath) {
        $Generator = "Ninja"
        Write-Info "Using Ninja generator"
    } else {
        $Generator = "MinGW Makefiles"
        Write-Info "Using MinGW Makefiles generator (install Ninja for faster builds)"
    }
}

# Determine job count
if ($Jobs -le 0) {
    $Jobs = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors
    if ($Jobs -le 0) { $Jobs = 4 }
}
Write-Info "Parallel jobs: $Jobs"

# Check for Ruby (optional, for mRuby support)
Write-Step "Checking optional dependencies..."
$RubyPath = Get-Command "ruby" -ErrorAction SilentlyContinue
$RakePath = Get-Command "rake" -ErrorAction SilentlyContinue
if ($RubyPath -and $RakePath) {
    $RubyVersion = & ruby --version 2>&1
    Write-Info "Ruby found: $RubyVersion (mRuby support enabled)"
    $HasMRuby = $true
} else {
    Write-Warn "Ruby/Rake not found - mRuby scripting will be disabled"
    $HasMRuby = $false
}

# Clean if requested
if ($Clean) {
    Write-Step "Cleaning build directory..."
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
        Write-Info "Removed: $BuildDir"
    }
}

# Create build directory
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
    Write-Info "Created: $BuildDir"
}

# Build mRuby for Emscripten if Ruby is available
if ($HasMRuby) {
    $MRubyRoot = Join-Path $ProjectRoot "external\mruby"
    $MRubyBuildConfigDir = Join-Path $MRubyRoot "build_config"
    $LupineConfigSrc = Join-Path $ProjectRoot "resources\lupine-emscripten.rb"
    $LupineConfigDst = Join-Path $MRubyBuildConfigDir "lupine-emscripten.rb"
    # The library is built in build/lupine-emscripten/ not build/emscripten/
    $MRubyLib = Join-Path $MRubyRoot "build\lupine-emscripten\lib\libmruby.a"

    # Copy Lupine's mRuby config if not already present
    Write-Step "Checking mRuby build configuration..."
    if (Test-Path $LupineConfigSrc) {
        if (-not (Test-Path $LupineConfigDst)) {
            Write-Info "Copying lupine-emscripten.rb to mRuby build_config..."
            Copy-Item -Path $LupineConfigSrc -Destination $LupineConfigDst -Force
            Write-Info "Copied: $LupineConfigDst"
        } else {
            # Check if source is newer and update if needed
            $SrcTime = (Get-Item $LupineConfigSrc).LastWriteTime
            $DstTime = (Get-Item $LupineConfigDst).LastWriteTime
            if ($SrcTime -gt $DstTime) {
                Write-Info "Updating lupine-emscripten.rb (source is newer)..."
                Copy-Item -Path $LupineConfigSrc -Destination $LupineConfigDst -Force
            } else {
                Write-Info "mRuby config already in place: lupine-emscripten.rb"
            }
        }
    } else {
        Write-Warn "Lupine mRuby config not found at: $LupineConfigSrc"
        Write-Warn "mRuby will use default emscripten config (may not work correctly)"
    }

    if (-not (Test-Path $MRubyLib)) {
        Write-Step "Building mRuby for Emscripten (two-step process)..."

        # Find emmake
        $EmmakePath = Get-Command "emmake" -ErrorAction SilentlyContinue
        if (-not $EmmakePath) {
            Write-Err "emmake not found! Emscripten may not be properly activated."
            exit 1
        }

        Push-Location $MRubyRoot
        try {
            # Step 1: Build host tools (mrbc) with native compiler
            # The host build is defined in lupine-emscripten.rb and builds mrbc
            Write-Info "Step 1: Building mRuby host tools (mrbc) with native compiler..."
            $env:MRUBY_CONFIG = "lupine-emscripten"

            # Use cmd /c to avoid PowerShell stderr handling issues with rake
            $RakeOutput = cmd /c "rake 2>&1"
            $RakeExitCode = $LASTEXITCODE
            foreach ($line in $RakeOutput) {
                Write-Info $line
            }

            if ($RakeExitCode -ne 0) {
                Write-Warn "mRuby host build failed - continuing without mRuby support"
                $HasMRuby = $false
            } else {
                Write-Info "Host build complete."

                # Step 2: Build cross library with Emscripten
                # emmake wraps rake so emcc is used for the cross build
                Write-Info "Step 2: Building mRuby for WebAssembly with Emscripten..."
                $EmmakeOutput = cmd /c "emmake rake 2>&1"
                $EmmakeExitCode = $LASTEXITCODE
                foreach ($line in $EmmakeOutput) {
                    Write-Info $line
                }

                if ($EmmakeExitCode -ne 0) {
                    Write-Warn "mRuby cross build failed - continuing without mRuby support"
                    $HasMRuby = $false
                } else {
                    Write-Info "mRuby WebAssembly build complete."
                }
            }
        } finally {
            Pop-Location
        }
    } else {
        Write-Info "mRuby already built for Emscripten at: $MRubyLib"
    }
}

# Configure CMake
Write-Step "Configuring CMake..."
$ConfigFile = Join-Path $BuildDir "CMakeCache.txt"
$NeedsConfigure = (-not (Test-Path $ConfigFile)) -or $Clean

if ($NeedsConfigure) {
    $CmakeArgs = "-S `"$ProjectRoot`" -B `"$BuildDir`" -G `"$Generator`" -DCMAKE_BUILD_TYPE=$BuildType -DLUPINE_BUILD_EXPORT_TEMPLATES=ON -DBUILD_PYTHON_MODULES=OFF"

    Write-Info "Running: emcmake cmake $CmakeArgs"

    Push-Location $BuildDir
    try {
        # Run emcmake using cmd /c to avoid PowerShell stderr handling issues
        $ConfigOutput = cmd /c "emcmake cmake $CmakeArgs 2>&1"
        $ConfigExitCode = $LASTEXITCODE

        # Display output
        foreach ($line in $ConfigOutput) {
            if ($line -match "^--") { Write-Info $line }
            elseif ($line -match "CMake Error") { Write-Err $line }
            elseif ($line -match "CMake Warning") { Write-Warn $line }
            elseif ($line -match "emcmake:") { Write-Info $line }
            else { Write-Host "    $line" -ForegroundColor DarkGray }
        }

        if ($ConfigExitCode -ne 0) {
            Write-Err "CMake configuration failed with exit code $ConfigExitCode!"
            exit 1
        }
        Write-Info "Configuration complete"
    } finally {
        Pop-Location
    }
} else {
    Write-Info "Using existing configuration (use -Clean to reconfigure)"
}

# Build. Both templates are produced every time: the release one that ships, and
# the debug one (function names preserved in the wasm, assertions on) that the
# editor uses when a preset enables "Include debug symbols".
Write-Step "Building..."
$BuildArgs = @(
    "--build", $BuildDir,
    "--target", "lupine_template", "lupine_template_debug",
    "--config", $BuildType,
    "-j", $Jobs
)

Write-Info "Running: cmake $($BuildArgs -join ' ')"
$StartTime = Get-Date

# Run build with proper error handling
$OldErrorAction = $ErrorActionPreference
$ErrorActionPreference = "Continue"

$BuildOutput = & cmake @BuildArgs 2>&1
$BuildExitCode = $LASTEXITCODE

$ErrorActionPreference = $OldErrorAction

# Display output
foreach ($line in $BuildOutput) {
    $lineStr = $line.ToString()
    if ($lineStr -match "error:|Error:|ERROR:") { Write-Err $lineStr }
    elseif ($lineStr -match "warning:|Warning:|WARNING:") { Write-Warn $lineStr }
    elseif ($lineStr -match "^\[.*\]") { Write-Host "    $lineStr" -ForegroundColor DarkGray }
    else { Write-Host "    $lineStr" -ForegroundColor DarkGray }
}

if ($BuildExitCode -ne 0) {
    Write-Err "Build failed with exit code $BuildExitCode!"
    exit 1
}

$Duration = (Get-Date) - $StartTime
Write-Info "Build completed in $($Duration.TotalSeconds.ToString("F1")) seconds"

# Verify output files
Write-Step "Verifying output..."
$RequiredFiles = @("index.html", "index.js", "index.wasm")
$MissingFiles = @()

function Format-FileSize {
    param([long]$Size)
    if ($Size -gt 1MB) { return "$([math]::Round($Size / 1MB, 2)) MB" }
    if ($Size -gt 1KB) { return "$([math]::Round($Size / 1KB, 2)) KB" }
    return "$Size bytes"
}

foreach ($File in $RequiredFiles) {
    $FilePath = Join-Path $OutputDir $File
    if (Test-Path $FilePath) {
        Write-Info "$File : $(Format-FileSize (Get-Item $FilePath).Length)"
    } else {
        $MissingFiles += $File
    }
}

foreach ($File in $RequiredFiles) {
    $FilePath = Join-Path $DebugOutputDir $File
    if (Test-Path $FilePath) {
        Write-Info "debug/$File : $(Format-FileSize (Get-Item $FilePath).Length)"
    } else {
        $MissingFiles += "web_debug/$File"
    }
}

# Check for optional data file
$DataFile = Join-Path $OutputDir "index.data"
if (Test-Path $DataFile) {
    $Size = (Get-Item $DataFile).Length
    $SizeStr = if ($Size -gt 1MB) { "$([math]::Round($Size / 1MB, 2)) MB" } else { "$([math]::Round($Size / 1KB, 2)) KB" }
    Write-Info "index.data : $SizeStr (preloaded assets)"
}

if ($MissingFiles.Count -gt 0) {
    Write-Err "Missing output files: $($MissingFiles -join ', ')"
    exit 1
}

# Copy to export_templates directory
Write-Step "Installing web templates..."
$ExportTemplatesDir = Join-Path $ProjectRoot "build\export_templates"
$WebTemplateDir = Join-Path $ExportTemplatesDir "web"
# The exporter looks for the debug variant in web/debug/ (see ExportPlatform.
# debug_template_filename in editor/export/export_manager.py).
$WebDebugTemplateDir = Join-Path $WebTemplateDir "debug"

foreach ($Dir in @($WebTemplateDir, $WebDebugTemplateDir)) {
    if (-not (Test-Path $Dir)) {
        New-Item -ItemType Directory -Path $Dir -Force | Out-Null
        Write-Info "Created: $Dir"
    }
}

# Copy web files to export_templates/web
$WebFiles = @("index.html", "index.js", "index.wasm")
foreach ($File in $WebFiles) {
    $SrcPath = Join-Path $OutputDir $File
    if (Test-Path $SrcPath) {
        Copy-Item -Path $SrcPath -Destination (Join-Path $WebTemplateDir $File) -Force
        Write-Info "Installed: $File"
    }
}

# Copy data file if it exists
if (Test-Path $DataFile) {
    Copy-Item -Path $DataFile -Destination (Join-Path $WebTemplateDir "index.data") -Force
    Write-Info "Installed: index.data"
}

# Copy debug web files to export_templates/web/debug. index.wasm.symbols is the
# --emit-symbol-map output used to demangle wasm frames.
$DebugWebFiles = @("index.html", "index.js", "index.wasm", "index.data", "index.wasm.symbols")
foreach ($File in $DebugWebFiles) {
    $SrcPath = Join-Path $DebugOutputDir $File
    if (Test-Path $SrcPath) {
        Copy-Item -Path $SrcPath -Destination (Join-Path $WebDebugTemplateDir $File) -Force
        Write-Info "Installed: debug/$File"
    }
}

# Copy default Lupine favicon
$LupineIconPath = Join-Path $ProjectRoot "editor\resources\icon.png"
$FaviconDst = Join-Path $WebTemplateDir "favicon.ico"
if (Test-Path $LupineIconPath) {
    # Try to convert PNG to ICO using Python if available
    $ConvertedFavicon = $false
    if (Get-Command "python" -ErrorAction SilentlyContinue) {
        try {
            $ConvertScript = @"
from PIL import Image
import sys
try:
    img = Image.open(sys.argv[1])
    img.save(sys.argv[2], format='ICO', sizes=[(16,16), (32,32), (48,48), (64,64)])
    print('OK')
except Exception as e:
    print(f'FAIL: {e}')
"@
            $TempScript = Join-Path $env:TEMP "convert_favicon.py"
            $ConvertScript | Set-Content $TempScript -Encoding UTF8
            $ConvertResult = & python $TempScript $LupineIconPath $FaviconDst 2>&1
            if ($ConvertResult -match "^OK") {
                Write-Info "Installed: favicon.ico (converted from PNG)"
                $ConvertedFavicon = $true
            }
            Remove-Item $TempScript -ErrorAction SilentlyContinue
        } catch {
            # Conversion failed, fall through to PNG copy
        }
    }

    if (-not $ConvertedFavicon) {
        # Fall back to copying PNG as favicon.png (browsers support this)
        $FaviconPngDst = Join-Path $WebTemplateDir "favicon.png"
        Copy-Item -Path $LupineIconPath -Destination $FaviconPngDst -Force
        Write-Info "Installed: favicon.png (PNG fallback)"
    }
} else {
    Write-Warn "Lupine icon not found at: $LupineIconPath"
}

# Update templates.json
Write-Step "Updating templates manifest..."
$TemplatesJsonPath = Join-Path $ExportTemplatesDir "templates.json"
$TemplatesData = @{
    version = "0.1.0"
    templates = @{}
}

# Load existing templates.json if it exists
if (Test-Path $TemplatesJsonPath) {
    try {
        $ExistingData = Get-Content $TemplatesJsonPath -Raw | ConvertFrom-Json
        # Convert to hashtable for easier manipulation
        if ($ExistingData.templates) {
            $ExistingData.templates.PSObject.Properties | ForEach-Object {
                $TemplatesData.templates[$_.Name] = @{
                    platform = $_.Value.platform
                    architecture = $_.Value.architecture
                    binary = $_.Value.binary
                    debug_binary = $_.Value.debug_binary
                    graphics_backends = @($_.Value.graphics_backends)
                }
            }
        }
        if ($ExistingData.version) {
            $TemplatesData.version = $ExistingData.version
        }
    } catch {
        Write-Warn "Could not parse existing templates.json, creating new one"
    }
}

# Add/update web template entry
$TemplatesData.templates["web"] = @{
    platform = "web"
    architecture = "wasm32"
    binary = "index.html"
    debug_binary = "debug/index.html"
    graphics_backends = @("webgl")
}

# Write updated templates.json
$TemplatesData | ConvertTo-Json -Depth 4 | Set-Content $TemplatesJsonPath -Encoding UTF8
Write-Info "Updated: $TemplatesJsonPath"

Write-Header "Build Successful!"
Write-Info "Output directory: $OutputDir"
Write-Info ""
Write-Info "Files:"
Write-Info "  - index.html  : Main HTML file"
Write-Info "  - index.js    : JavaScript runtime"
Write-Info "  - index.wasm  : WebAssembly binary"
if (Test-Path $DataFile) {
    Write-Info "  - index.data  : Preloaded assets"
}
Write-Info ""
Write-Info "Debug template (web_debug/): function names preserved, assertions enabled."
Write-Info "Used automatically when an export preset enables 'Include debug symbols'."
Write-Info ""

# Start server if requested
if ($Serve) {
    Write-Step "Starting local test server..."
    $ServePy = Join-Path $ScriptDir "serve.py"

    if (Test-Path $ServePy) {
        $PythonPath = Get-Command "python" -ErrorAction SilentlyContinue
        if (-not $PythonPath) {
            $PythonPath = Get-Command "python3" -ErrorAction SilentlyContinue
        }

        if ($PythonPath) {
            Write-Info "Server URL: http://localhost:8080/"
            Write-Info "Press Ctrl+C to stop"
            & python $ServePy $OutputDir
        } else {
            Write-Warn "Python not found - cannot start server"
            Write-Info "Start manually: python -m http.server 8080 --directory $OutputDir"
        }
    } else {
        Write-Warn "serve.py not found"
        Write-Info "Start manually: python -m http.server 8080 --directory $OutputDir"
    }
} else {
    Write-Info "To test locally:"
    Write-Info "  .\build-web.ps1 -Serve"
    Write-Info "  or: python serve.py"
}

