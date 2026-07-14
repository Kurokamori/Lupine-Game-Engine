# Lupine Engine - Web Build

Build Lupine Engine games for the web using WebAssembly and WebGL 2.0.

## Quick Start

### Windows

```powershell
# 1. Install Emscripten (one-time setup)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
.\emsdk.ps1 install latest
.\emsdk.ps1 activate latest

# 2. Activate environment (every new terminal)
.\emsdk_env.ps1

# 3. Build
cd path\to\Lupine-Engine\web-build
.\build-web.ps1
```

### Linux/macOS

```bash
# 1. Install Emscripten (one-time setup)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest

# 2. Activate environment (every new terminal)
source ./emsdk_env.sh

# 3. Build
cd path/to/Lupine-Engine/web-build
./build-web.sh
```

## Prerequisites

| Requirement | Purpose | Installation |
|-------------|---------|--------------|
| **Emscripten SDK** | WebAssembly compiler | See Quick Start above |
| **Ruby + Rake** | mRuby scripting | `winget install RubyInstallerTeam.Ruby` (Windows) or `apt install ruby rake` (Linux) |
| **Ninja** (optional) | Faster builds | `winget install Ninja-build.Ninja` (Windows) or `apt install ninja-build` (Linux) |
| **Python** | Local test server | Usually pre-installed |

## Build Options

### Windows (PowerShell)

```powershell
.\build-web.ps1                  # Release build
.\build-web.ps1 -DebugBuild      # Debug build with assertions
.\build-web.ps1 -Clean           # Clean and rebuild
.\build-web.ps1 -Serve           # Build and start test server
.\build-web.ps1 -Help            # Show all options
```

### Linux/macOS (Bash)

```bash
./build-web.sh               # Release build
./build-web.sh --debug       # Debug build with assertions
./build-web.sh --clean       # Clean and rebuild
./build-web.sh --serve       # Build and start test server
./build-web.sh --help        # Show all options
```

## Output Files

After a successful build, you'll find these files in `web-build/build-web/bin/`:

| File | Description |
|------|-------------|
| `index.html` | Main HTML file (upload this to itch.io) |
| `index.js` | JavaScript runtime |
| `index.wasm` | WebAssembly binary (the actual engine) |
| `index.data` | Preloaded game assets (if any) |

## Debug Template

Each build produces two web templates. The debug one is staged separately, in
`web-build/build-web/bin/web_debug/` and installed to `build/export_templates/web/debug/`,
because all of its files are named `index.*` exactly like the release template's.

It is built with `-g2 --profiling-funcs`, so the WebAssembly **name section survives**
and browser stack traces show real C++ function names instead of `wasm-function[1234]`.
It also emits `index.wasm.symbols` (`--emit-symbol-map`) and turns on `ASSERTIONS=2` and
stack-overflow checks. Optimization stays at `-O2` so the build is still playable.

The editor uses it automatically when an export preset enables **Include debug symbols**.
Ship the release template.

## Testing Locally

Web builds require an HTTP server (file:// URLs won't work due to CORS).

```bash
# Option 1: Use the build script
.\build-web.ps1 -Serve          # Windows
./build-web.sh --serve          # Linux/macOS

# Option 2: Python server
cd web-build/build-web/bin
python -m http.server 8080
# Open http://localhost:8080/
```

## Publishing to itch.io

1. Build in Release mode:
   ```powershell
   .\build-web.ps1 -Clean
   ```

2. Create a ZIP containing:
   - `index.html`
   - `index.js`
   - `index.wasm`
   - `index.data` (if present)

3. Upload to itch.io:
   - Add the ZIP file
   - Check "This file will be played in the browser"
   - Set viewport to 1280x720 (or your game's resolution)
   - Enable "SharedArrayBuffer support" if needed

## Scripting Languages

The web build supports three scripting languages:

| Language | Library | Notes |
|----------|---------|-------|
| **Lua** | sol2 | Full support via Emscripten port |
| **Python** | MicroPython | Embedded subset (not full CPython) |
| **Ruby** | mRuby | Cross-compiled for WebAssembly |

**Note:** CPython is NOT supported in web builds. Use MicroPython for Python scripting.

### mRuby Requirements

For mRuby support, you need Ruby and Rake installed on your build machine:

- **Windows:** `winget install RubyInstallerTeam.Ruby`
- **macOS:** `brew install ruby`
- **Linux:** `apt install ruby rake`

If Ruby is not installed, the build will continue without mRuby support.

## Troubleshooting

### "emcc not found"

Make sure Emscripten is activated in your current terminal:

```powershell
# Windows
cd path\to\emsdk
.\emsdk_env.ps1
```

```bash
# Linux/macOS
cd path/to/emsdk
source ./emsdk_env.sh
```

### CMake errors

Try a clean build:

```powershell
.\build-web.ps1 -Clean
```

### Game won't load (CORS errors)

You must use an HTTP server. File URLs (file://) don't work with WebAssembly.

### Game won't load (SharedArrayBuffer errors)

On itch.io, enable "SharedArrayBuffer support" in your game settings.

### No audio on mobile

Mobile browsers require user interaction before playing audio. The shell template handles this automatically.

### Performance issues

- Always use Release builds for distribution
- The first load may be slow (WASM compilation)
- Subsequent loads use cached compiled code

## Browser Compatibility

| Browser | Minimum Version |
|---------|----------------|
| Chrome | 80+ |
| Firefox | 75+ |
| Safari | 15+ |
| Edge | 80+ |

**Requirements:**
- WebGL 2.0 support
- WebAssembly support

## Advanced Configuration

### Memory Settings

Edit `cmake/ExportTemplates.cmake`:

```cmake
"-sINITIAL_MEMORY=67108864"   # 64MB initial (increase for large games)
"-sMAXIMUM_MEMORY=536870912"  # 512MB max
```

### Custom Shell Template

Edit `runtime/web/shell.html` to customize:
- Loading screen appearance
- Error messages
- Fullscreen behavior
- Canvas sizing

### Preloading Assets

Assets in the `resources/` directory are automatically preloaded. For custom paths:

```cmake
"--preload-file=${YOUR_ASSETS_PATH}@/"
```

## Manual Build (Advanced)

If you prefer manual control over the build process:

```bash
# Set up environment
source path/to/emsdk/emsdk_env.sh

# Configure
mkdir -p web-build/build-web
cd web-build/build-web
emcmake cmake ../.. \
    -DCMAKE_BUILD_TYPE=Release \
    -DLUPINE_BUILD_EXPORT_TEMPLATES=ON \
    -G Ninja

# Build (release + debug templates)
emmake ninja lupine_template lupine_template_debug
```

The output will be in `web-build/build-web/bin/` (debug template in `bin/web_debug/`).
