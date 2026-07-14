#!/usr/bin/env python3
"""
Shader Compilation Script
Converts shader source files (.glsl, .vert, .frag, .lsh) into C++ header files
Supports multiple graphics backends (OpenGL, Vulkan, Metal, WebGL, DirectX)
Handles large shaders by splitting them to avoid C++ string literal size limits

For DirectX12: Pre-compiles HLSL to DXIL bytecode using dxc.exe (SM 6.0)

Unified shader path (.lsh):
  When --use-unified is set (default), reads .lsh files from the Unified/ directory,
  transpiles them to all backends via LupineShaderTranspiler, then embeds the
  generated source into GeneratedShaders.hpp with the same namespace structure.
  Falls back to legacy per-backend directories if no .lsh files are found.
"""

import os
import sys
import subprocess
import tempfile
import argparse
from pathlib import Path

# Maximum lines per string literal chunk (to avoid C++ 16KB limit)
MAX_LINES_PER_CHUNK = 200


# ---------------------------------------------------------------------------
# DXC helpers (unchanged)
# ---------------------------------------------------------------------------

def find_dxc_executable():
    """Find dxc.exe — prefers vcpkg build directory to ensure the same DXC version
    that vcpkg links into the engine is used for offline shader compilation."""
    script_dir = Path(__file__).parent
    project_root = script_dir.parent  # core/ -> project root

    # 1. vcpkg installed tools (preferred — matches the linked dxcompiler)
    for triplet in ['x64-windows-static-md', 'x64-windows']:
        vcpkg_dxc = project_root / 'build' / 'vcpkg_installed' / triplet / 'tools' / 'directx-dxc' / 'dxc.exe'
        if vcpkg_dxc.exists():
            print(f"  Using vcpkg DXC: {vcpkg_dxc}")
            return str(vcpkg_dxc)

    # 2. Project build directory (CMake may copy dxc there)
    for build_dir in ['build', 'build/Release', 'build/Debug', 'build/RelWithDebInfo']:
        p = project_root / build_dir / 'dxc.exe'
        if p.exists():
            print(f"  Using build-dir DXC: {p}")
            return str(p)

    # 3. Windows SDK
    program_files = os.environ.get('ProgramFiles(x86)', 'C:\\Program Files (x86)')
    sdk_path = Path(program_files) / 'Windows Kits' / '10' / 'bin'
    if sdk_path.exists():
        sdk_versions = [d for d in sdk_path.iterdir() if d.is_dir() and d.name.startswith('10.')]
        if sdk_versions:
            sdk_versions.sort(reverse=True)
            dxc_path = sdk_versions[0] / 'x64' / 'dxc.exe'
            if dxc_path.exists():
                print(f"  Using Windows SDK DXC: {dxc_path}")
                return str(dxc_path)

    # 4. System PATH
    import shutil
    dxc_in_path = shutil.which('dxc.exe') or shutil.which('dxc')
    if dxc_in_path:
        print(f"  Using PATH DXC: {dxc_in_path}")
        return dxc_in_path

    return None


def get_dxc_target(extension):
    """Get DXC shader target profile for SM 6.0"""
    targets = {
        'vert': 'vs_6_0',
        'frag': 'ps_6_0',
        'geom': 'gs_6_0',
        'comp': 'cs_6_0',
        'hull': 'hs_6_0',
        'dom': 'ds_6_0',
    }
    return targets.get(extension, 'vs_6_0')


def compile_hlsl_to_dxil(dxc_path, hlsl_source, shader_type, entry_point='main'):
    """Compile HLSL source to DXIL bytecode using dxc.exe.

    DX12 shaders are pre-compiled to DXIL here (SM 6.0) and embedded in
    GeneratedShaders.hpp; getShaderData() prefers this bytecode to avoid a runtime
    compile.  find_dxc_executable() prefers the vcpkg dxc.exe so the offline DXIL
    validator version matches the runtime dxcompiler.dll the engine links.  If DXC is
    unavailable or a shader fails to compile, stub bytecode accessors are emitted and
    the engine falls back to compiling the embedded HLSL source at runtime.
    """
    target = get_dxc_target(shader_type)

    # Write source to temp file
    with tempfile.NamedTemporaryFile(mode='w', suffix='.hlsl', delete=False, encoding='utf-8') as f:
        f.write(hlsl_source)
        temp_input = f.name

    temp_output = temp_input + '.dxil'

    try:
        # Run dxc.exe
        cmd = [
            dxc_path,
            '-T', target,           # Target profile (vs_6_0, ps_6_0, etc.)
            '-E', entry_point,      # Entry point
            '-Zpc',                 # Column-major matrices (match GLM)
            '-HV', '2021',          # HLSL 2021 for modern features
            '-Od',                  # Disable optimization (matches runtime debug build)
            '-Fo', temp_output,     # Output file
            temp_input              # Input file
        ]

        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode != 0:
            err = result.stderr.strip() if result.stderr else "(no error output)"
            print(f"\n  DXC compilation failed ({target}, entry={entry_point}):\n    {err}", file=sys.stderr)
            return None

        # Read compiled bytecode
        if os.path.exists(temp_output):
            with open(temp_output, 'rb') as f:
                bytecode = f.read()
            return bytecode
        else:
            print(f"  DXC output file not created", file=sys.stderr)
            return None

    finally:
        # Cleanup temp files
        if os.path.exists(temp_input):
            os.unlink(temp_input)
        if os.path.exists(temp_output):
            os.unlink(temp_output)


def bytecode_to_cpp_array(bytecode, var_name):
    """Convert bytecode to C++ uint8_t array"""
    lines = []
    lines.append(f'constexpr uint8_t {var_name}_data[] = {{')

    # Format as hex bytes, 16 per line
    hex_bytes = [f'0x{b:02X}' for b in bytecode]
    for i in range(0, len(hex_bytes), 16):
        chunk = hex_bytes[i:i+16]
        lines.append('    ' + ', '.join(chunk) + ',')

    lines.append('};')
    lines.append(f'constexpr size_t {var_name}_size = sizeof({var_name}_data);')
    return '\n'.join(lines)


def split_shader_into_chunks(source_lines, max_lines=MAX_LINES_PER_CHUNK):
    """Split shader source into chunks to avoid C++ string literal size limits"""
    chunks = []
    current_chunk = []

    for line in source_lines:
        current_chunk.append(line)
        if len(current_chunk) >= max_lines:
            chunks.append(current_chunk)
            current_chunk = []

    if current_chunk:
        chunks.append(current_chunk)

    return chunks if len(chunks) > 1 else [source_lines]


# ---------------------------------------------------------------------------
# C++ source embedding helpers
# ---------------------------------------------------------------------------

def _emit_source_shader(lines, var_name, extension, source):
    """Emit a non-DX12 shader source as constexpr string(s) + accessor function.

    This is the common code path shared by both the legacy and unified paths.
    """
    source_trimmed = source.lstrip()
    source_lines = source_trimmed.split('\n')
    chunks = split_shader_into_chunks(source_lines)

    if len(chunks) == 1:
        # Put source directly after R"( to avoid leading newline
        # GLSL requires #version to be on the first line
        lines.append(f'constexpr const char* {var_name}_{extension}_src = R"({source_trimmed})";')
        lines.append('')
        lines.append(f'inline const char* {var_name}_{extension}() {{')
        lines.append(f'    return {var_name}_{extension}_src;')
        lines.append(f'}}')
        lines.append('')
    else:
        lines.append(f'// Large shader split into {len(chunks)} chunks')
        for i, chunk in enumerate(chunks):
            chunk_source = '\n'.join(chunk)
            # Add trailing newline to all chunks except the last to ensure
            # proper line separation when chunks are concatenated
            if i < len(chunks) - 1:
                chunk_source += '\n'

            if i == 0:
                # First chunk: put source directly after R"( to avoid leading newline
                # This ensures #version is on line 1 for GLSL shaders
                lines.append(f'constexpr const char* {var_name}_{extension}_chunk{i} = R"({chunk_source})";')
            else:
                # Subsequent chunks: no leading newline needed since previous chunk ends with newline
                lines.append(f'constexpr const char* {var_name}_{extension}_chunk{i} = R"({chunk_source})";')
            lines.append('')

        lines.append(f'inline std::string get_{var_name}_{extension}() {{')
        lines.append(f'    std::string result;')
        lines.append(f'    result.reserve({len(source_trimmed) + 100});')
        for i in range(len(chunks)):
            lines.append(f'    result += {var_name}_{extension}_chunk{i};')
        lines.append(f'    return result;')
        lines.append(f'}}')
        lines.append('')

        lines.append(f'inline const char* {var_name}_{extension}() {{')
        lines.append(f'    static const std::string shader = get_{var_name}_{extension}();')
        lines.append(f'    return shader.c_str();')
        lines.append(f'}}')
        lines.append('')


def _emit_dx12_shader(lines, var_name, extension, source, display_name, dxc_path, stats):
    """Emit a DX12 shader: DXIL bytecode (if DXC available) + source fallback.

    *stats* is a dict with keys 'compiled' and 'failed' that are updated in place.
    """
    if dxc_path:
        print(f"  Compiling {display_name} to DXIL...", end=' ')
        bytecode = compile_hlsl_to_dxil(dxc_path, source, extension)

        if bytecode:
            print(f"OK ({len(bytecode)} bytes)")
            # Generate bytecode array
            array_code = bytecode_to_cpp_array(bytecode, f'{var_name}_{extension}')
            lines.append(f'// {display_name} - Pre-compiled DXIL')
            lines.append(array_code)
            lines.append('')

            # Generate accessor functions matching the source-based API
            lines.append(f'inline const uint8_t* {var_name}_{extension}_bytecode() {{')
            lines.append(f'    return {var_name}_{extension}_data;')
            lines.append(f'}}')
            lines.append('')
            lines.append(f'inline size_t {var_name}_{extension}_bytecode_size() {{')
            lines.append(f'    return {var_name}_{extension}_size;')
            lines.append(f'}}')
            lines.append('')

            # Also provide source for debugging/fallback
            lines.append(f'// Original HLSL source (for debugging)')
            _emit_dx12_source_fallback(lines, var_name, extension, source)

            stats['compiled'] += 1
        else:
            print("FAILED")
            stats['failed'] += 1
            lines.append(f'// {display_name} - COMPILATION FAILED, source only')
            _emit_dx12_source_fallback(lines, var_name, extension, source)
            # Emit stub bytecode functions so DefaultShaders.hpp compiles
            _emit_dx12_bytecode_stubs(lines, var_name, extension)
    else:
        # No DXC available - source only
        lines.append(f'// {display_name} - source only (DXC not available)')
        _emit_dx12_source_fallback(lines, var_name, extension, source)
        _emit_dx12_bytecode_stubs(lines, var_name, extension)


def _emit_dx12_source_fallback(lines, var_name, extension, source):
    """Emit DX12 source as raw string literals (used alongside or instead of DXIL)."""
    source_lines = source.split('\n')
    chunks = split_shader_into_chunks(source_lines)
    if len(chunks) == 1:
        lines.append(f'constexpr const char* {var_name}_{extension}_src = R"(')
        lines.append(source)
        lines.append(')";')
        lines.append(f'inline const char* {var_name}_{extension}() {{ return {var_name}_{extension}_src; }}')
    else:
        for i, chunk in enumerate(chunks):
            chunk_source = '\n'.join(chunk)
            lines.append(f'constexpr const char* {var_name}_{extension}_chunk{i} = R"(')
            lines.append(chunk_source)
            lines.append(')";')
        lines.append(f'inline std::string get_{var_name}_{extension}() {{')
        lines.append(f'    std::string result;')
        lines.append(f'    result.reserve({len(source) + 100});')
        for i in range(len(chunks)):
            lines.append(f'    result += {var_name}_{extension}_chunk{i};')
        lines.append(f'    return result;')
        lines.append(f'}}')
        lines.append(f'inline const char* {var_name}_{extension}() {{')
        lines.append(f'    static const std::string shader = get_{var_name}_{extension}();')
        lines.append(f'    return shader.c_str();')
        lines.append(f'}}')
    lines.append('')


def _emit_dx12_bytecode_stubs(lines, var_name, extension):
    """Emit stub bytecode accessor functions that return nullptr/0.

    DefaultShaders.hpp unconditionally calls *_bytecode() and *_bytecode_size()
    for DX12 shaders.  When offline DXIL compilation fails or is unavailable,
    these stubs ensure the C++ code still compiles; the engine will detect the
    null bytecode and fall back to runtime compilation from source.
    """
    lines.append(f'inline const uint8_t* {var_name}_{extension}_bytecode() {{ return nullptr; }}')
    lines.append(f'inline size_t {var_name}_{extension}_bytecode_size() {{ return 0; }}')
    lines.append('')


# ---------------------------------------------------------------------------
# Unified (.lsh) transpiler path
# ---------------------------------------------------------------------------

def _import_transpiler():
    """Import the lupine_shader_transpiler module from the same directory."""
    script_dir = Path(__file__).parent
    if str(script_dir) not in sys.path:
        sys.path.insert(0, str(script_dir))
    # Import the module - it lives next to this script
    import lupine_shader_transpiler as lst
    return lst


def _collect_lsh_files(lsh_dir):
    """Return a sorted list of .lsh Path objects in *lsh_dir*, or [] if none."""
    lsh_dir = Path(lsh_dir)
    if not lsh_dir.exists():
        return []
    files = sorted(lsh_dir.glob('*.lsh'))
    return files


def generate_shader_header_unified(lsh_dir, output_file, dxc_path=None):
    """Generate GeneratedShaders.hpp by transpiling .lsh files from *lsh_dir*.

    For each .lsh file the transpiler produces backend-specific source code
    which is then embedded in exactly the same namespace / accessor format as
    the legacy path.
    """
    lst = _import_transpiler()
    transpiler = lst.LupineShaderTranspiler()

    lsh_files = _collect_lsh_files(lsh_dir)
    if not lsh_files:
        return 0, 0, 0  # caller will fall back to legacy

    # Backend ordering must match the legacy path
    backends = ['OpenGL', 'Vulkan', 'Metal', 'WebGL', 'DirectX11', 'DirectX12']

    # Stage 1 -- transpile every .lsh to every backend, collecting the
    # generated source strings keyed by (backend, filename_base).
    # Structure:  { backend_name: [ (filename_base, stage, ext, source), ... ] }
    backend_shaders = {b: [] for b in backends}

    for lsh_file in lsh_files:
        with open(lsh_file, 'r', encoding='utf-8') as f:
            lsh_source = f.read()

        ir = transpiler.parser.parse(lsh_source)
        filename_base = lst.get_shader_filename(ir.name)
        print(f"  Transpiling '{ir.name}' ({lsh_file.name}) -> {filename_base}")

        all_outputs = transpiler.transpile_all(lsh_source)

        for backend_name in backends:
            outputs = all_outputs.get(backend_name, {})
            if not outputs:
                continue

            if backend_name == 'Metal':
                # Metal shaders are combined (vertex+fragment in one file)
                backend_shaders[backend_name].append(
                    (filename_base, 'combined', 'metal', outputs['combined'])
                )
            else:
                # Separate vertex / fragment (and geometry if present)
                for stage_key in ('vertex', 'fragment', 'geometry'):
                    if stage_key not in outputs:
                        continue
                    ext_map = {'vertex': 'vert', 'fragment': 'frag', 'geometry': 'geom'}
                    ext = ext_map[stage_key]
                    backend_shaders[backend_name].append(
                        (filename_base, stage_key, ext, outputs[stage_key])
                    )

    # Stage 2 -- build the C++ header
    lines = []
    lines.append('#pragma once')
    lines.append('')
    lines.append('// Auto-generated file - DO NOT EDIT')
    lines.append('// Generated by compile_shaders.py from core/shaders/')
    lines.append('// Contains shader sources for all supported graphics backends')
    lines.append('// Large shaders are split into chunks to avoid C++ string literal size limits')
    lines.append('// DirectX12 shaders are pre-compiled to DXIL bytecode (SM 6.0)')
    lines.append('')
    lines.append('#include <string>')
    lines.append('#include <cstdint>')
    lines.append('#include <cstddef>')
    lines.append('')
    lines.append('namespace lupine {')
    lines.append('namespace GeneratedShaders {')
    lines.append('')

    total_shaders = 0
    dx12_stats = {'compiled': 0, 'failed': 0}

    for backend_name in backends:
        entries = backend_shaders[backend_name]
        if not entries:
            continue

        is_dx12 = (backend_name == 'DirectX12')

        lines.append(f'// ===== {backend_name} Shaders =====')
        if is_dx12:
            lines.append('// Pre-compiled DXIL bytecode (Shader Model 6.0)')
        lines.append(f'namespace {backend_name} {{')
        lines.append('')

        # Sort entries by (filename_base, ext) to produce deterministic output
        entries.sort(key=lambda e: (e[0], e[2]))

        for filename_base, _stage, ext, source in entries:
            var_name = filename_base.replace('-', '_').replace('.', '_')
            display_name = f'{filename_base}.{ext}'

            if is_dx12 and dxc_path:
                _emit_dx12_shader(lines, var_name, ext, source, display_name,
                                  dxc_path, dx12_stats)
            elif is_dx12:
                _emit_dx12_shader(lines, var_name, ext, source, display_name,
                                  None, dx12_stats)
            else:
                _emit_source_shader(lines, var_name, ext, source)

            total_shaders += 1

        lines.append(f'}} // namespace {backend_name}')
        lines.append('')

    lines.append('} // namespace GeneratedShaders')
    lines.append('} // namespace lupine')
    lines.append('')

    # Write output file
    output_file = Path(output_file)
    output_file.parent.mkdir(parents=True, exist_ok=True)
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))

    return total_shaders, dx12_stats['compiled'], dx12_stats['failed']


# ---------------------------------------------------------------------------
# Legacy per-backend-directory path (original behaviour)
# ---------------------------------------------------------------------------

def generate_shader_header(shader_base_dir, output_file, dxc_path=None):
    """Generate a C++ header with all shader sources organized by backend"""

    # Supported backends and their directories
    backends = ['OpenGL', 'Vulkan', 'Metal', 'WebGL', 'DirectX11', 'DirectX12']

    # Generate header content
    lines = []
    lines.append('#pragma once')
    lines.append('')
    lines.append('// Auto-generated file - DO NOT EDIT')
    lines.append('// Generated by compile_shaders.py from core/shaders/')
    lines.append('// Contains shader sources for all supported graphics backends')
    lines.append('// Large shaders are split into chunks to avoid C++ string literal size limits')
    lines.append('// DirectX12 shaders are pre-compiled to DXIL bytecode (SM 6.0)')
    lines.append('')
    lines.append('#include <string>')
    lines.append('#include <cstdint>')
    lines.append('#include <cstddef>')
    lines.append('')
    lines.append('namespace lupine {')
    lines.append('namespace GeneratedShaders {')
    lines.append('')

    total_shaders = 0
    dx12_compiled = 0
    dx12_failed = 0

    # Process each backend
    for backend in backends:
        backend_dir = shader_base_dir / backend

        if not backend_dir.exists():
            continue

        is_dx12 = (backend == 'DirectX12')

        lines.append(f'// ===== {backend} Shaders =====')
        if is_dx12:
            lines.append('// Pre-compiled DXIL bytecode (Shader Model 6.0)')
        lines.append(f'namespace {backend} {{')
        lines.append('')

        # Find all shader files in this backend directory
        shader_files = []
        for ext in ['.vert', '.frag', '.geom', '.glsl', '.hlsl', '.metal']:
            shader_files.extend(backend_dir.glob(f'*{ext}'))

        shader_files.sort()

        for shader_file in shader_files:
            # Read shader source
            with open(shader_file, 'r', encoding='utf-8') as f:
                source = f.read()

            # Generate variable name from filename (remove extension, convert to valid C++ identifier)
            var_name = shader_file.stem.replace('-', '_').replace('.', '_')
            extension = shader_file.suffix[1:]  # Remove the dot

            if is_dx12 and dxc_path:
                dx12_stats = {'compiled': 0, 'failed': 0}
                _emit_dx12_shader(lines, var_name, extension, source,
                                  shader_file.name, dxc_path, dx12_stats)
                dx12_compiled += dx12_stats['compiled']
                dx12_failed += dx12_stats['failed']
            elif is_dx12:
                dx12_stats = {'compiled': 0, 'failed': 0}
                _emit_dx12_shader(lines, var_name, extension, source,
                                  shader_file.name, None, dx12_stats)
                dx12_failed += dx12_stats['failed']
            else:
                _emit_source_shader(lines, var_name, extension, source)

            total_shaders += 1

        lines.append(f'}} // namespace {backend}')
        lines.append('')

    lines.append('} // namespace GeneratedShaders')
    lines.append('} // namespace lupine')
    lines.append('')

    # Write output file
    output_file.parent.mkdir(parents=True, exist_ok=True)
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))

    return total_shaders, dx12_compiled, dx12_failed


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Compile shader sources into GeneratedShaders.hpp')
    parser.add_argument(
        '--use-unified', action='store_true', default=True,
        help='Use the unified .lsh transpiler path (default: True)')
    parser.add_argument(
        '--no-unified', action='store_true', default=False,
        help='Force legacy per-backend directory path')
    parser.add_argument(
        '--lsh-dir', type=str, default=None,
        help='Directory containing .lsh files (default: <shaders>/Unified)')
    args = parser.parse_args()

    script_dir = Path(__file__).parent
    shader_base_dir = script_dir
    output_file = script_dir.parent / 'include' / 'lupine' / 'rendering' / 'GeneratedShaders.hpp'

    # Resolve --use-unified / --no-unified
    use_unified = args.use_unified and not args.no_unified

    # Resolve the .lsh directory
    lsh_dir = Path(args.lsh_dir) if args.lsh_dir else (shader_base_dir / 'Unified')

    # Find DXC compiler
    dxc_path = find_dxc_executable()
    if dxc_path:
        print(f"Found DXC compiler: {dxc_path}")
    else:
        print("WARNING: DXC compiler not found. DirectX12 shaders will not be pre-compiled.")
        print("  Install directx-dxc via vcpkg or Windows SDK to enable DXIL compilation.")

    # Try the unified path first when enabled
    total = 0
    dx12_ok = 0
    dx12_fail = 0

    if use_unified:
        lsh_files = _collect_lsh_files(lsh_dir)
        if lsh_files:
            print(f"Transpiling {len(lsh_files)} .lsh shaders from: {lsh_dir}")
            print(f"Output: {output_file}")
            print()
            total, dx12_ok, dx12_fail = generate_shader_header_unified(
                lsh_dir, output_file, dxc_path)
        else:
            print(f"No .lsh files found in {lsh_dir}, falling back to legacy path.")
            use_unified = False  # fall through to legacy below

    if not use_unified or total == 0:
        print(f"Compiling shaders from: {shader_base_dir}")
        print(f"Output: {output_file}")
        print()
        total, dx12_ok, dx12_fail = generate_shader_header(
            shader_base_dir, output_file, dxc_path)

    print()
    print(f"Total shaders processed: {total}")
    if dxc_path:
        print(f"DirectX12 DXIL compiled: {dx12_ok}")
        if dx12_fail > 0:
            print(f"DirectX12 compilation failures: {dx12_fail}")

    sys.exit(0 if total > 0 else 1)
