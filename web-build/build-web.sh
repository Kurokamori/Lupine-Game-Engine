#!/usr/bin/env bash

# ============================================================
# Lupine Engine - Web Build Script (Linux/macOS)
# ============================================================

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
GRAY='\033[0;90m'
NC='\033[0m' # No Color

print_header() {
    echo -e "\n${CYAN}============================================================${NC}"
    echo -e "${CYAN}  $1${NC}"
    echo -e "${CYAN}============================================================${NC}\n"
}

print_step() {
    echo -e "${GREEN}[*] $1${NC}"
}

print_info() {
    echo -e "    ${GRAY}$1${NC}"
}

print_warn() {
    echo -e "${YELLOW}[!] $1${NC}"
}

print_err() {
    echo -e "${RED}[X] $1${NC}"
}

show_help() {
    cat << EOF

Lupine Engine - Web Build Script

Usage: ./build-web.sh [options]

Options:
    -d, --debug     Build with debug symbols and assertions
    -c, --clean     Clean the build directory before building
    -s, --serve     Start a local test server after building
    -j, --jobs N    Number of parallel build jobs (default: auto-detect)
    -h, --help      Show this help message

Examples:
    ./build-web.sh                  Build release template
    ./build-web.sh -d -c            Clean and build debug template
    ./build-web.sh -s               Build and start test server

EOF
    exit 0
}

# Parse arguments
DEBUG_BUILD=0
CLEAN=0
SERVE=0
JOBS=0
BUILD_TYPE="Release"

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            DEBUG_BUILD=1
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clean)
            CLEAN=1
            shift
            ;;
        -s|--serve)
            SERVE=1
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        -h|--help)
            show_help
            ;;
        *)
            print_err "Unknown option: $1"
            show_help
            ;;
    esac
done

# Script directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$SCRIPT_DIR/build-web"
OUTPUT_DIR="$BUILD_DIR/bin"
# The debug template links to its own directory: its files are named index.*
# exactly like the release template's and would otherwise overwrite them.
DEBUG_OUTPUT_DIR="$OUTPUT_DIR/web_debug"

print_header "Lupine Engine - Web Build"

print_step "Build type: $BUILD_TYPE"

# Check for Emscripten
print_step "Checking Emscripten..."
if ! command -v emcc &> /dev/null; then
    print_err "Emscripten not found in PATH!"
    echo ""
    print_info "Please install and activate Emscripten SDK:"
    print_info "  1. git clone https://github.com/emscripten-core/emsdk.git"
    print_info "  2. cd emsdk"
    print_info "  3. ./emsdk install latest"
    print_info "  4. ./emsdk activate latest"
    print_info "  5. source ./emsdk_env.sh"
    echo ""
    exit 1
fi
EMCC_VERSION=$(emcc --version 2>&1 | head -n 1)
print_info "Found: $EMCC_VERSION"

if ! command -v emcmake &> /dev/null; then
    print_err "emcmake not found! Emscripten may not be properly activated."
    exit 1
fi

# Check for CMake
print_step "Checking CMake..."
if ! command -v cmake &> /dev/null; then
    print_err "CMake not found in PATH!"
    exit 1
fi
CMAKE_VERSION=$(cmake --version | head -n 1)
print_info "Found: $CMAKE_VERSION"

# Determine generator
GENERATOR=""
if command -v ninja &> /dev/null; then
    GENERATOR="Ninja"
    print_info "Using Ninja generator"
else
    GENERATOR="Unix Makefiles"
    print_info "Using Unix Makefiles generator (install Ninja for faster builds)"
fi

# Determine job count
if [[ $JOBS -le 0 ]]; then
    if [[ -f /proc/cpuinfo ]]; then
        JOBS=$(grep -c ^processor /proc/cpuinfo)
    elif command -v sysctl &> /dev/null; then
        JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
    else
        JOBS=4
    fi
fi
print_info "Parallel jobs: $JOBS"

# Check for Ruby (optional)
print_step "Checking optional dependencies..."
HAS_MRUBY=0
if command -v ruby &> /dev/null && command -v rake &> /dev/null; then
    RUBY_VERSION=$(ruby --version 2>&1)
    print_info "Ruby found: $RUBY_VERSION (mRuby support enabled)"
    HAS_MRUBY=1
else
    print_warn "Ruby/Rake not found - mRuby scripting will be disabled"
fi

# Clean if requested
if [[ $CLEAN -eq 1 ]]; then
    print_step "Cleaning build directory..."
    if [[ -d "$BUILD_DIR" ]]; then
        rm -rf "$BUILD_DIR"
        print_info "Removed: $BUILD_DIR"
    fi
fi

# Create build directory
if [[ ! -d "$BUILD_DIR" ]]; then
    mkdir -p "$BUILD_DIR"
    print_info "Created: $BUILD_DIR"
fi

# Build mRuby for Emscripten if Ruby is available
if [[ $HAS_MRUBY -eq 1 ]]; then
    MRUBY_ROOT="$PROJECT_ROOT/external/mruby"
    MRUBY_BUILD_CONFIG_DIR="$MRUBY_ROOT/build_config"
    LUPINE_CONFIG_SRC="$PROJECT_ROOT/resources/lupine-emscripten.rb"
    LUPINE_CONFIG_DST="$MRUBY_BUILD_CONFIG_DIR/lupine-emscripten.rb"
    MRUBY_LIB="$MRUBY_ROOT/build/lupine-emscripten/lib/libmruby.a"

    print_step "Checking mRuby build configuration..."
    if [[ -f "$LUPINE_CONFIG_SRC" ]]; then
        if [[ ! -f "$LUPINE_CONFIG_DST" ]]; then
            print_info "Copying lupine-emscripten.rb to mRuby build_config..."
            cp "$LUPINE_CONFIG_SRC" "$LUPINE_CONFIG_DST"
            print_info "Copied: $LUPINE_CONFIG_DST"
        elif [[ "$LUPINE_CONFIG_SRC" -nt "$LUPINE_CONFIG_DST" ]]; then
            print_info "Updating lupine-emscripten.rb (source is newer)..."
            cp "$LUPINE_CONFIG_SRC" "$LUPINE_CONFIG_DST"
        else
            print_info "mRuby config already in place: lupine-emscripten.rb"
        fi
    else
        print_warn "Lupine mRuby config not found at: $LUPINE_CONFIG_SRC"
        print_warn "mRuby will use default emscripten config (may not work correctly)"
    fi

    if [[ ! -f "$MRUBY_LIB" ]]; then
        print_step "Building mRuby for Emscripten (two-step process)..."

        if ! command -v emmake &> /dev/null; then
            print_err "emmake not found! Emscripten may not be properly activated."
            exit 1
        fi

        pushd "$MRUBY_ROOT" > /dev/null

        # Step 1: Build host tools
        print_info "Step 1: Building mRuby host tools (mrbc) with native compiler..."
        export MRUBY_CONFIG="lupine-emscripten"

        if ! rake 2>&1; then
            print_warn "mRuby host build failed - continuing without mRuby support"
            HAS_MRUBY=0
        else
            print_info "Host build complete."

            # Step 2: Build cross library
            print_info "Step 2: Building mRuby for WebAssembly with Emscripten..."
            if ! emmake rake 2>&1; then
                print_warn "mRuby cross build failed - continuing without mRuby support"
                HAS_MRUBY=0
            else
                print_info "mRuby WebAssembly build complete."
            fi
        fi

        popd > /dev/null
    else
        print_info "mRuby already built for Emscripten at: $MRUBY_LIB"
    fi
fi

# Configure CMake
print_step "Configuring CMake..."
CONFIG_FILE="$BUILD_DIR/CMakeCache.txt"
NEEDS_CONFIGURE=0
[[ ! -f "$CONFIG_FILE" ]] && NEEDS_CONFIGURE=1
[[ $CLEAN -eq 1 ]] && NEEDS_CONFIGURE=1

if [[ $NEEDS_CONFIGURE -eq 1 ]]; then
    print_info "Running: emcmake cmake -S \"$PROJECT_ROOT\" -B \"$BUILD_DIR\" -G \"$GENERATOR\" -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DLUPINE_BUILD_EXPORT_TEMPLATES=ON -DBUILD_PYTHON_MODULES=OFF"

    pushd "$BUILD_DIR" > /dev/null
    if ! emcmake cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -G "$GENERATOR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DLUPINE_BUILD_EXPORT_TEMPLATES=ON \
        -DBUILD_PYTHON_MODULES=OFF; then
        print_err "CMake configuration failed!"
        popd > /dev/null
        exit 1
    fi
    popd > /dev/null
    print_info "Configuration complete"
else
    print_info "Using existing configuration (use -c to reconfigure)"
fi

# Build. Both templates are produced every time: the release one that ships, and
# the debug one (function names preserved in the wasm, assertions on) that the
# editor uses when a preset enables "Include debug symbols".
print_step "Building..."
print_info "Running: cmake --build \"$BUILD_DIR\" --target lupine_template lupine_template_debug --config $BUILD_TYPE -j $JOBS"

START_TIME=$(date +%s)

if ! cmake --build "$BUILD_DIR" --target lupine_template lupine_template_debug --config "$BUILD_TYPE" -j "$JOBS"; then
    print_err "Build failed!"
    exit 1
fi

END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))
print_info "Build completed in ${DURATION} seconds"

# Verify output files
print_step "Verifying output..."
REQUIRED_FILES=("index.html" "index.js" "index.wasm")
MISSING_FILES=()

for file in "${REQUIRED_FILES[@]}"; do
    filepath="$OUTPUT_DIR/$file"
    if [[ -f "$filepath" ]]; then
        size=$(stat -f%z "$filepath" 2>/dev/null || stat -c%s "$filepath" 2>/dev/null)
        if [[ $size -gt 1048576 ]]; then
            size_str="$(echo "scale=2; $size / 1048576" | bc) MB"
        elif [[ $size -gt 1024 ]]; then
            size_str="$(echo "scale=2; $size / 1024" | bc) KB"
        else
            size_str="$size bytes"
        fi
        print_info "$file : $size_str"
    else
        MISSING_FILES+=("$file")
    fi
done

for file in "${REQUIRED_FILES[@]}"; do
    filepath="$DEBUG_OUTPUT_DIR/$file"
    if [[ -f "$filepath" ]]; then
        size=$(stat -f%z "$filepath" 2>/dev/null || stat -c%s "$filepath" 2>/dev/null)
        if [[ $size -gt 1048576 ]]; then
            size_str="$(echo "scale=2; $size / 1048576" | bc) MB"
        elif [[ $size -gt 1024 ]]; then
            size_str="$(echo "scale=2; $size / 1024" | bc) KB"
        else
            size_str="$size bytes"
        fi
        print_info "debug/$file : $size_str"
    else
        MISSING_FILES+=("web_debug/$file")
    fi
done

# Check for optional data file
DATA_FILE="$OUTPUT_DIR/index.data"
if [[ -f "$DATA_FILE" ]]; then
    size=$(stat -f%z "$DATA_FILE" 2>/dev/null || stat -c%s "$DATA_FILE" 2>/dev/null)
    if [[ $size -gt 1048576 ]]; then
        size_str="$(echo "scale=2; $size / 1048576" | bc) MB"
    else
        size_str="$(echo "scale=2; $size / 1024" | bc) KB"
    fi
    print_info "index.data : $size_str (preloaded assets)"
fi

if [[ ${#MISSING_FILES[@]} -gt 0 ]]; then
    print_err "Missing output files: ${MISSING_FILES[*]}"
    exit 1
fi

# Install to export_templates
print_step "Installing web templates..."
EXPORT_TEMPLATES_DIR="$PROJECT_ROOT/build/export_templates"
WEB_TEMPLATE_DIR="$EXPORT_TEMPLATES_DIR/web"
# The exporter looks for the debug variant in web/debug/ (see ExportPlatform.
# debug_template_filename in editor/export/export_manager.py).
WEB_DEBUG_TEMPLATE_DIR="$WEB_TEMPLATE_DIR/debug"

for dir in "$WEB_TEMPLATE_DIR" "$WEB_DEBUG_TEMPLATE_DIR"; do
    if [[ ! -d "$dir" ]]; then
        mkdir -p "$dir"
        print_info "Created: $dir"
    fi
done

for file in index.html index.js index.wasm; do
    src="$OUTPUT_DIR/$file"
    dst="$WEB_TEMPLATE_DIR/$file"
    if [[ -f "$src" ]]; then
        cp "$src" "$dst"
        print_info "Installed: $file"
    fi
done

if [[ -f "$DATA_FILE" ]]; then
    cp "$DATA_FILE" "$WEB_TEMPLATE_DIR/index.data"
    print_info "Installed: index.data"
fi

# index.wasm.symbols is the --emit-symbol-map output used to demangle wasm frames.
for file in index.html index.js index.wasm index.data index.wasm.symbols; do
    src="$DEBUG_OUTPUT_DIR/$file"
    dst="$WEB_DEBUG_TEMPLATE_DIR/$file"
    if [[ -f "$src" ]]; then
        cp "$src" "$dst"
        print_info "Installed: debug/$file"
    fi
done

# Copy default Lupine favicon
LUPINE_ICON="$PROJECT_ROOT/editor/resources/icon.png"
FAVICON_DST="$WEB_TEMPLATE_DIR/favicon.ico"
if [[ -f "$LUPINE_ICON" ]]; then
    # Try to convert PNG to ICO using Python with PIL
    FAVICON_CONVERTED=0
    if command -v python3 &> /dev/null; then
        if python3 -c "from PIL import Image; img = Image.open('$LUPINE_ICON'); img.save('$FAVICON_DST', format='ICO', sizes=[(16,16), (32,32), (48,48), (64,64)])" 2>/dev/null; then
            print_info "Installed: favicon.ico (converted from PNG)"
            FAVICON_CONVERTED=1
        fi
    elif command -v python &> /dev/null; then
        if python -c "from PIL import Image; img = Image.open('$LUPINE_ICON'); img.save('$FAVICON_DST', format='ICO', sizes=[(16,16), (32,32), (48,48), (64,64)])" 2>/dev/null; then
            print_info "Installed: favicon.ico (converted from PNG)"
            FAVICON_CONVERTED=1
        fi
    fi

    if [[ $FAVICON_CONVERTED -eq 0 ]]; then
        # Fall back to PNG favicon
        cp "$LUPINE_ICON" "$WEB_TEMPLATE_DIR/favicon.png"
        print_info "Installed: favicon.png (PNG fallback)"
    fi
else
    print_warn "Lupine icon not found at: $LUPINE_ICON"
fi

# Update templates.json
print_step "Updating templates manifest..."
TEMPLATES_JSON="$EXPORT_TEMPLATES_DIR/templates.json"

# Use Python for JSON manipulation (more portable than jq)
if command -v python3 &> /dev/null; then
    python3 << EOF
import json
import os

json_path = "$TEMPLATES_JSON"
data = {"version": "0.1.0", "templates": {}}

if os.path.exists(json_path):
    try:
        with open(json_path, 'r') as f:
            data = json.load(f)
    except:
        pass

if "templates" not in data:
    data["templates"] = {}

data["templates"]["web"] = {
    "platform": "web",
    "architecture": "wasm32",
    "binary": "index.html",
    "debug_binary": "debug/index.html",
    "graphics_backends": ["webgl"]
}

with open(json_path, 'w') as f:
    json.dump(data, f, indent=2)
EOF
    print_info "Updated: $TEMPLATES_JSON"
elif command -v python &> /dev/null; then
    python << EOF
import json
import os

json_path = "$TEMPLATES_JSON"
data = {"version": "0.1.0", "templates": {}}

if os.path.exists(json_path):
    try:
        with open(json_path, 'r') as f:
            data = json.load(f)
    except:
        pass

if "templates" not in data:
    data["templates"] = {}

data["templates"]["web"] = {
    "platform": "web",
    "architecture": "wasm32",
    "binary": "index.html",
    "debug_binary": "debug/index.html",
    "graphics_backends": ["webgl"]
}

with open(json_path, 'w') as f:
    json.dump(data, f, indent=2)
EOF
    print_info "Updated: $TEMPLATES_JSON"
else
    print_warn "Python not found - templates.json not updated"
fi

print_header "Build Successful!"
print_info "Output directory: $OUTPUT_DIR"
print_info "Template installed to: $WEB_TEMPLATE_DIR"
echo ""
print_info "Files:"
print_info "  - index.html  : Main HTML file"
print_info "  - index.js    : JavaScript runtime"
print_info "  - index.wasm  : WebAssembly binary"
if [[ -f "$DATA_FILE" ]]; then
    print_info "  - index.data  : Preloaded assets"
fi
echo ""
print_info "Debug template installed to: $WEB_DEBUG_TEMPLATE_DIR"
print_info "It preserves function names and enables assertions, and is used"
print_info "automatically when an export preset enables 'Include debug symbols'."
echo ""

# Start server if requested
if [[ $SERVE -eq 1 ]]; then
    print_step "Starting local test server..."
    SERVE_PY="$SCRIPT_DIR/serve.py"

    if [[ -f "$SERVE_PY" ]]; then
        if command -v python3 &> /dev/null; then
            print_info "Server URL: http://localhost:8080/"
            print_info "Press Ctrl+C to stop"
            python3 "$SERVE_PY" "$OUTPUT_DIR"
        elif command -v python &> /dev/null; then
            print_info "Server URL: http://localhost:8080/"
            print_info "Press Ctrl+C to stop"
            python "$SERVE_PY" "$OUTPUT_DIR"
        else
            print_warn "Python not found - cannot start server"
            print_info "Start manually: python -m http.server 8080 --directory $OUTPUT_DIR"
        fi
    else
        print_warn "serve.py not found"
        print_info "Start manually: python -m http.server 8080 --directory $OUTPUT_DIR"
    fi
else
    print_info "To test locally:"
    print_info "  ./build-web.sh -s"
    print_info "  or: python serve.py"
fi
