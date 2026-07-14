#!/usr/bin/env python3
"""
Build MicroPython embed package for Lupine Engine.
This script generates all necessary headers using MicroPython's own tools.
Works on Windows (MSVC), Linux, and macOS.
"""

import os
import sys
import subprocess
import re
from pathlib import Path


def find_micropython_dir():
    """Find the MicroPython source directory."""
    script_dir = Path(__file__).parent
    mp_dir = script_dir.parent / "external" / "micropython"
    if mp_dir.exists() and (mp_dir / "py" / "py.mk").exists():
        return mp_dir
    raise RuntimeError(f"MicroPython not found at {mp_dir}")


def ensure_dir(path):
    """Create directory if it doesn't exist."""
    Path(path).mkdir(parents=True, exist_ok=True)


def generate_mpversion_h(output_dir, mp_dir):
    """Generate mpversion.h header."""
    output_file = output_dir / "mpversion.h"

    # Try to get version from MicroPython's mpconfig.h
    version_major = 1
    version_minor = 24
    version_micro = 0

    mpconfig = mp_dir / "py" / "mpconfig.h"
    if mpconfig.exists():
        content = mpconfig.read_text()
        major_match = re.search(r'#define\s+MICROPY_VERSION_MAJOR\s+\((\d+)\)', content)
        minor_match = re.search(r'#define\s+MICROPY_VERSION_MINOR\s+\((\d+)\)', content)
        micro_match = re.search(r'#define\s+MICROPY_VERSION_MICRO\s+\((\d+)\)', content)
        if major_match:
            version_major = int(major_match.group(1))
        if minor_match:
            version_minor = int(minor_match.group(1))
        if micro_match:
            version_micro = int(micro_match.group(1))

    version_string = f"{version_major}.{version_minor}.{version_micro}"

    content = f'''// Auto-generated for Lupine Engine MicroPython embed
#define MICROPY_GIT_TAG "v{version_string}"
#define MICROPY_GIT_HASH "lupine-embed"
#define MICROPY_BUILD_DATE __DATE__
#define MICROPY_VERSION_MAJOR {version_major}
#define MICROPY_VERSION_MINOR {version_minor}
#define MICROPY_VERSION_MICRO {version_micro}
#define MICROPY_VERSION_STRING "{version_string}"
'''
    output_file.write_text(content, encoding='utf-8', newline='\n')
    print(f"  Generated: {output_file.name}")


def generate_moduledefs_h(output_dir):
    """Generate moduledefs.h header with proper module registration."""
    output_file = output_dir / "moduledefs.h"

    # Generate minimal module definitions
    # For embedded use, we keep this very minimal to avoid MSVC issues
    content = '''// Auto-generated module definitions for Lupine Engine MicroPython embed
// Minimal configuration - only essential modules

#ifndef MICROPY_MODULEDEFS_H
#define MICROPY_MODULEDEFS_H

// Core modules that MicroPython always requires (__main__ from runtime.c, builtins from modbuiltins.c)
extern const struct _mp_obj_module_t mp_module___main__;
#undef MODULE_DEF___MAIN__
#define MODULE_DEF___MAIN__ { MP_ROM_QSTR(MP_QSTR___main__), MP_ROM_PTR(&mp_module___main__) },

extern const struct _mp_obj_module_t mp_module_builtins;
#undef MODULE_DEF_BUILTINS
#define MODULE_DEF_BUILTINS { MP_ROM_QSTR(MP_QSTR_builtins), MP_ROM_PTR(&mp_module_builtins) },

#define MICROPY_REGISTERED_MODULES \\
    MODULE_DEF___MAIN__ \\
    MODULE_DEF_BUILTINS \\
// MICROPY_REGISTERED_MODULES

// No extensible modules for minimal embed
#define MICROPY_HAVE_REGISTERED_EXTENSIBLE_MODULES 0

#endif // MICROPY_MODULEDEFS_H
'''
    output_file.write_text(content, encoding='utf-8', newline='\n')
    print(f"  Generated: {output_file.name}")


def generate_root_pointers_h(output_dir):
    """Generate root_pointers.h header for minimal config."""
    output_file = output_dir / "root_pointers.h"
    content = '''// Auto-generated root pointers for Lupine Engine
// Minimal configuration - no extra root pointers needed
'''
    output_file.write_text(content, encoding='utf-8', newline='\n')
    print(f"  Generated: {output_file.name}")


def generate_qstrdefs(output_dir, mp_dir, python_exe):
    """Generate qstrdefs.generated.h using our custom generator.

    MicroPython has two qstr pools:
    - QDEF0: Static pool (unsorted) - core qstrs for .mpy ABI compatibility
    - QDEF1: Main pool (sorted) - all other qstrs for binary search

    IMPORTANT: Both pools MUST have at least 1 entry, otherwise qstr_find_strn()
    will have an integer underflow when computing 'high = pool->len - 1' with len=0.
    """
    output_file = output_dir / "qstrdefs.generated.h"

    # Use our custom generator that properly handles both QDEF0 and QDEF1 pools
    script_dir = Path(__file__).parent
    generate_qstrs_script = script_dir / "generate_micropython_qstrs.py"

    if generate_qstrs_script.exists():
        try:
            result = subprocess.run(
                [python_exe, str(generate_qstrs_script), str(output_file)],
                capture_output=True,
                text=True,
                check=True
            )
            print(f"  Generated: {output_file.name}")
            if result.stdout:
                for line in result.stdout.strip().split('\n'):
                    print(f"    {line}")
            return
        except subprocess.CalledProcessError as e:
            print(f"  Warning: generate_micropython_qstrs.py failed: {e.stderr}")

    # Fallback to built-in generator
    print(f"  Using fallback qstr generator...")
    generate_minimal_qstrdefs(output_file, mp_dir)


def scan_source_for_qstrs(source_dir):
    """Scan MicroPython source files for Q() and MP_QSTR_ usage."""
    qstrs = set()

    q_pattern = re.compile(r'Q\(([^)]+)\)')
    mp_qstr_pattern = re.compile(r'MP_QSTR_([A-Za-z_][A-Za-z0-9_]*)')

    for pattern in ["**/*.c", "**/*.h"]:
        for src_file in source_dir.glob(pattern):
            try:
                content = src_file.read_text(errors='ignore')
                for match in q_pattern.finditer(content):
                    qstr = match.group(1).strip()
                    if qstr:
                        qstrs.add(qstr)
                for match in mp_qstr_pattern.finditer(content):
                    qstr_name = match.group(1)
                    if qstr_name and qstr_name != 'null':
                        qstrs.add(qstr_name)
            except Exception:
                pass

    return qstrs


def generate_minimal_qstrdefs(output_file, mp_dir):
    """Generate minimal qstrdefs.generated.h as fallback."""
    print("  Generating comprehensive qstrdefs...")

    qstrs = []

    # Read static qstrs from makeqstrdata.py
    makeqstrdata = mp_dir / "py" / "makeqstrdata.py"
    if makeqstrdata.exists():
        content = makeqstrdata.read_text()
        match = re.search(r'static_qstr_list\s*=\s*\[(.*?)\]', content, re.DOTALL)
        if match:
            qstr_text = match.group(1)
            qstrs = re.findall(r'"([^"]*)"', qstr_text)

    # Also read from qstrdefs.h
    qstrdefs_h = mp_dir / "py" / "qstrdefs.h"
    if qstrdefs_h.exists():
        content = qstrdefs_h.read_text()
        for match in re.finditer(r'Q\(([^)]+)\)', content):
            qstr = match.group(1).strip()
            if qstr not in qstrs:
                qstrs.append(qstr)

    # Scan source files for additional qstrs
    print("    Scanning source files for QSTRs...")
    source_qstrs = scan_source_for_qstrs(mp_dir / "py")
    source_qstrs.update(scan_source_for_qstrs(mp_dir / "ports" / "embed"))
    source_qstrs.update(scan_source_for_qstrs(mp_dir / "shared" / "runtime"))
    print(f"    Found {len(source_qstrs)} QSTRs in source files")

    for qstr in source_qstrs:
        if qstr not in qstrs:
            qstrs.append(qstr)

    # Add essential qstrs
    essential_qstrs = [
        "function", "closure", "bound_method", "generator", "iterator",
        "module", "type", "object", "property", "staticmethod", "classmethod",
        "cell", "code", "frame", "traceback", "slice", "range",
        "str", "bytes", "bytearray", "int", "float", "complex", "bool",
        "list", "tuple", "dict", "set", "frozenset", "memoryview", "array",
        "NoneType", "super", "map", "filter", "zip", "enumerate", "reversed",
        "gc", "collect", "disable", "enable", "isenabled", "mem_free", "mem_alloc",
        "threshold", "heap_lock", "heap_unlock",
        "__build_class__", "__import__", "__repl_print__",
        "abs", "all", "any", "bin", "callable", "chr", "compile",
        "delattr", "dir", "divmod", "eval", "exec", "format", "getattr",
        "globals", "hasattr", "hash", "hex", "id", "input", "isinstance",
        "issubclass", "iter", "len", "locals", "max", "min", "next",
        "oct", "open", "ord", "pow", "print", "repr", "round", "setattr",
        "sorted", "sum", "vars",
        "\\n", " ", "*", "/", "<module>", "<lambda>", "<listcomp>",
        "<dictcomp>", "<setcomp>", "<genexpr>", "<string>", "<stdin>",
        "append", "clear", "copy", "count", "extend", "find", "format",
        "get", "index", "insert", "items", "join", "keys", "lower",
        "pop", "remove", "replace", "reverse", "sort", "split", "strip",
        "upper", "update", "values", "encode", "decode", "endswith",
        "startswith", "isalpha", "isdigit", "isspace", "capitalize",
        "math", "sin", "cos", "tan", "sqrt", "floor", "ceil", "pi", "e",
        "log", "exp", "fabs", "atan2", "asin", "acos", "atan", "degrees",
        "radians", "pow", "modf", "frexp", "ldexp", "trunc", "isfinite",
        "isinf", "isnan", "copysign", "fmod",
        "struct", "pack", "unpack", "pack_into", "unpack_from", "calcsize",
        "micropython", "const", "native", "viper", "opt_level", "mem_info",
        "qstr_info", "stack_use", "kbd_intr", "schedule",
        "__abs__", "__add__", "__and__", "__bool__", "__bytes__",
        "__contains__", "__del__", "__delattr__", "__dict__", "__divmod__",
        "__doc__", "__eq__", "__float__", "__floordiv__", "__ge__",
        "__get__", "__gt__", "__iadd__", "__iand__", "__ifloordiv__",
        "__ilshift__", "__imod__", "__imul__", "__index__", "__invert__",
        "__ior__", "__ipow__", "__irshift__", "__isub__", "__itruediv__",
        "__ixor__", "__le__", "__lshift__", "__lt__", "__matmul__",
        "__mod__", "__mul__", "__ne__", "__neg__", "__or__", "__pos__",
        "__pow__", "__radd__", "__rand__", "__repr__", "__reversed__",
        "__rfloordiv__", "__rlshift__", "__rmod__", "__rmul__", "__ror__",
        "__rpow__", "__rrshift__", "__rshift__", "__rsub__", "__rtruediv__",
        "__rxor__", "__set__", "__setattr__", "__sizeof__", "__sub__",
        "__truediv__", "__xor__", "__set_name__", "__init_subclass__",
        "__bases__", "__mro__", "__subclasses__", "__prepare__",
        "__instancecheck__", "__subclasscheck__",
        "args", "errno", "strerror", "filename", "lineno", "offset",
        "text", "msg", "value", "__traceback__", "__cause__", "__context__",
        "sys", "path", "argv", "exit", "modules", "platform", "version",
        "version_info", "implementation", "stdout", "stderr", "stdin",
        "exc_info", "print_exception", "getsizeof", "maxsize", "byteorder",
        "send", "throw", "close", "gi_frame", "gi_running", "gi_code",
        "gi_yieldfrom",
        "io", "read", "write", "readline", "readlines", "writelines",
        "seek", "tell", "flush", "mode", "name", "buffer",
        "StringIO", "BytesIO",
        "key", "default", "step", "stop", "start", "end", "data", "size",
        "encoding", "errors", "sep", "file", "newline", "buffering",
        "iterable", "number", "string", "self", "cls",
        "builtins", "__name__", "__main__", "__file__", "__path__",
        "__builtins__", "__all__", "__annotations__", "__code__",
        "__closure__", "__defaults__", "__globals__", "__kwdefaults__",
        "__qualname__", "__slots__", "__weakref__",
        "__init__", "__new__", "__call__", "__class__", "__module__",
        "__getattr__", "__setattr__", "__delattr__",
        "__getitem__", "__setitem__", "__delitem__",
        "__len__", "__iter__", "__next__", "__hash__",
        "__str__", "__enter__", "__exit__",
        "__aiter__", "__anext__", "__await__", "__aenter__", "__aexit__",
    ]

    for qstr in essential_qstrs:
        if qstr not in qstrs:
            qstrs.append(qstr)

    # Generate the header with both QDEF0 and QDEF1 pools
    # IMPORTANT: Both pools must have at least 1 entry to avoid integer underflow
    lines = [
        "// Auto-generated QSTR definitions for Lupine Engine MicroPython embed",
        "// Minimal configuration fallback",
        "// QDEF0 = static pool (unsorted, ABI-stable)",
        "// QDEF1 = main pool (sorted for binary search, must have >= 1 entry)",
        ""
    ]

    # djb2 hash function
    def compute_hash(s):
        hash_val = 5381
        for c in s.encode('utf-8'):
            hash_val = ((hash_val << 5) + hash_val) ^ c
            hash_val &= 0xFFFFFFFF
        hash_val &= 0xFFFF
        if hash_val == 0:
            hash_val = 1
        return hash_val

    # Convert special characters to C identifier format
    def qstr_to_id(s):
        if not s:
            return "MP_QSTR_"

        char_map = {
            ' ': '_space_', "'": '_squot_', '-': '_hyphen_', '<': '_lt_',
            '>': '_gt_', '*': '_star_', '/': '_slash_', '\n': '_newline_',
            ':': '_colon_', '.': '_dot_', ',': '_comma_', '(': '_paren_open_',
            ')': '_paren_close_', '[': '_bracket_open_', ']': '_bracket_close_',
            '{': '_brace_open_', '}': '_brace_close_', '!': '_bang_',
            '\\': '_backslash_', '+': '_plus_', '=': '_equals_',
            '?': '_question_', '@': '_at_sign_', '^': '_caret_',
            '|': '_pipe_', '~': '_tilde_', '#': '_hash_', '%': '_percent_',
            '$': '_dollar_', ';': '_semicolon_',
        }

        result = ""
        for c in s:
            if c in char_map:
                result += char_map[c]
            elif c.isalnum() or c == '_':
                result += c
            else:
                result += f"_x{ord(c):02x}_"

        return f"MP_QSTR_{result}"

    seen = set()

    # Build unique qstr list
    unique_qstrs = []
    # First entry must be null/empty
    unique_qstrs.append(("MP_QSTRnull", ""))
    seen.add("MP_QSTRnull")

    for qstr in qstrs:
        qstr_id = qstr_to_id(qstr)
        if qstr_id in seen:
            continue
        seen.add(qstr_id)
        unique_qstrs.append((qstr_id, qstr))

    # Split into QDEF0 (first 200) and QDEF1 (rest)
    QDEF0_COUNT = 200

    lines.append("// === QDEF0: Static pool (unsorted, ABI-stable) ===")
    for i, (qstr_id, qstr) in enumerate(unique_qstrs):
        if i >= QDEF0_COUNT:
            break
        hash_val = compute_hash(qstr)
        str_len = len(qstr.encode('utf-8'))
        escaped = qstr.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')
        lines.append(f'QDEF0({qstr_id}, {hash_val}, {str_len}, "{escaped}")')

    # QDEF1 pool - sorted for binary search, MUST have at least 1 entry
    lines.append("")
    lines.append("// === QDEF1: Main pool (sorted for binary search) ===")

    remaining = unique_qstrs[QDEF0_COUNT:]
    # Sort by string value for binary search
    remaining.sort(key=lambda x: x[1])

    # Ensure at least one entry in QDEF1
    if not remaining:
        # Add a dummy entry if no remaining qstrs
        remaining = [("MP_QSTR___lupine_qdef1_placeholder__", "__lupine_qdef1_placeholder__")]

    for qstr_id, qstr in remaining:
        hash_val = compute_hash(qstr)
        str_len = len(qstr.encode('utf-8'))
        escaped = qstr.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')
        lines.append(f'QDEF1({qstr_id}, {hash_val}, {str_len}, "{escaped}")')

    lines.append("")
    lines.append("// End of generated qstrs")

    output_file.write_text('\n'.join(lines), encoding='utf-8', newline='\n')
    qdef0_count = min(QDEF0_COUNT, len(unique_qstrs))
    qdef1_count = len(remaining)
    print(f"  Generated fallback: {output_file.name} (QDEF0: {qdef0_count}, QDEF1: {qdef1_count})")


def create_mpconfigport_h(output_dir):
    """Create mpconfigport.h for Lupine Engine with MSVC fixes."""
    output_file = output_dir / "mpconfigport.h"
    content = '''/* MicroPython configuration for Lupine Engine */
#ifndef LUPINE_MPCONFIGPORT_H
#define LUPINE_MPCONFIGPORT_H

#include <stdint.h>
#include <stdio.h>

/* MSVC compatibility - must come before any MicroPython includes */
#ifdef _MSC_VER
    /* Disable specific MSVC warnings for MicroPython code */
    #pragma warning(disable: 4116)  /* unnamed type definition in parentheses */
    #pragma warning(disable: 4200)  /* nonstandard extension: zero-sized array */
    #pragma warning(disable: 4244)  /* conversion, possible loss of data */
    #pragma warning(disable: 4267)  /* conversion from 'size_t' */

    /* MSVC doesn't have ssize_t */
    #include <BaseTsd.h>
    #ifndef _SSIZE_T_DEFINED
    typedef SSIZE_T ssize_t;
    #define _SSIZE_T_DEFINED
    #endif

    /* MSVC-specific macros */
    #define MP_NORETURN __declspec(noreturn)
    #define MP_NOINLINE __declspec(noinline)
    #define MP_ALWAYSINLINE __forceinline
    #define MP_LIKELY(x) (x)
    #define MP_UNLIKELY(x) (x)
    #define MP_WEAK

    /* MSVC doesn't support __builtin functions */
    #define MP_CLZ(x) _lzcnt_u32(x)
    #define MP_CTZ(x) _tzcnt_u32(x)

    /* Restrict keyword */
    #define restrict __restrict

#else
    /* GCC/Clang */
    #define MP_NORETURN __attribute__((noreturn))
    #define MP_NOINLINE __attribute__((noinline))
    #define MP_ALWAYSINLINE __attribute__((always_inline)) inline
    #define MP_LIKELY(x) __builtin_expect((x), 1)
    #define MP_UNLIKELY(x) __builtin_expect((x), 0)
    #define MP_WEAK __attribute__((weak))
#endif

/* Define endianness before any MicroPython includes to avoid missing endian.h on Windows */
#ifndef MP_ENDIANNESS_LITTLE
#define MP_ENDIANNESS_LITTLE (1)
#endif

/* Include common MicroPython embed configuration */
#include "port/mpconfigport_common.h"

/* Use minimum feature level for maximum compatibility */
#define MICROPY_CONFIG_ROM_LEVEL            (MICROPY_CONFIG_ROM_LEVEL_MINIMUM)

/* Explicitly set 2-byte qstr hashes */
#define MICROPY_QSTR_BYTES_IN_HASH          (2)

/* NLR implementation - use setjmp on all platforms for portability */
#define MICROPY_NLR_SETJMP                  (1)

/* GC helper - use generic for portability */
#define MICROPY_GCREGS_SETJMP               (1)

/* Core features for game scripting */
#define MICROPY_ENABLE_COMPILER             (1)
#define MICROPY_ENABLE_GC                   (1)
#define MICROPY_HELPER_REPL                 (0)
#define MICROPY_REPL_AUTO_INDENT            (0)
#define MICROPY_ENABLE_SOURCE_LINE          (1)
#define MICROPY_ENABLE_DOC_STRING           (0)

/* Python features - keep minimal to avoid MSVC ICE */
#define MICROPY_PY_BUILTINS                 (1)
#define MICROPY_PY_GC                       (0)  /* Disable to avoid module registration issues */
#define MICROPY_PY_MATH                     (0)  /* Disable to avoid module registration issues */
#define MICROPY_PY_ARRAY                    (0)  /* Disable to avoid module registration issues */
#define MICROPY_PY_STRUCT                   (0)  /* Disable to avoid module registration issues */
#define MICROPY_PY_MICROPYTHON              (0)  /* Disable to avoid module registration issues */
#define MICROPY_PY_IO                       (0)

/* Disable features that cause issues or aren't needed for embedding */
#define MICROPY_PY_SYS                      (0)
#define MICROPY_PY_ASYNC_AWAIT              (0)
#define MICROPY_ENABLE_SCHEDULER            (0)
#define MICROPY_KBD_EXCEPTION               (0)
#define MICROPY_ENABLE_FINALISER            (0)
#define MICROPY_PY_ATTRTUPLE                (0)
#define MICROPY_STACK_CHECK                 (0)
#define MICROPY_COMP_MODULE_CONST           (0)
#define MICROPY_COMP_TRIPLE_TUPLE_ASSIGN    (0)
#define MICROPY_COMP_RETURN_IF_EXPR         (0)
#define MICROPY_OPT_LOAD_ATTR_FAST_PATH     (0)
#define MICROPY_OPT_MAP_LOOKUP_CACHE        (0)

/* Floating point support */
#define MICROPY_FLOAT_IMPL                  (MICROPY_FLOAT_IMPL_DOUBLE)
#define MICROPY_PY_BUILTINS_FLOAT           (1)
#define MICROPY_PY_BUILTINS_COMPLEX         (0)  /* Disable complex numbers */
#define MICROPY_FLOAT_USE_NATIVE_FLT16      (0)  /* Disable native _Float16 to avoid compiler-rt dependency */

/* Error handling */
#define MICROPY_ERROR_REPORTING             (MICROPY_ERROR_REPORTING_TERSE)

/* Long int support - use simpler implementation */
#define MICROPY_LONGINT_IMPL                (MICROPY_LONGINT_IMPL_LONGLONG)

/* Disable extensible modules to avoid empty array issues */
/* MICROPY_HAVE_REGISTERED_EXTENSIBLE_MODULES is defined in moduledefs.h */
#define MICROPY_MODULE_ATTR_DELEGATION      (0)

/* Type definitions */
typedef intptr_t mp_int_t;
typedef uintptr_t mp_uint_t;
typedef long mp_off_t;

/* Memory configuration */
#define MICROPY_ALLOC_PATH_MAX              (256)
#define MICROPY_ALLOC_PARSE_CHUNK_INIT      (64)

/* Ensure we have mp_hal definitions */
#ifndef mp_hal_stdin_rx_chr
int mp_hal_stdin_rx_chr(void);
#endif
#ifndef mp_hal_stdout_tx_strn_cooked
void mp_hal_stdout_tx_strn_cooked(const char* str, size_t len);
#endif

#endif /* LUPINE_MPCONFIGPORT_H */
'''
    output_file.write_text(content, encoding='utf-8', newline='\n')
    print(f"  Generated: {output_file.name}")


def create_mphalport_h(output_dir):
    """Create mphalport.h for Lupine Engine."""
    output_file = output_dir / "mphalport.h"
    content = '''/* MicroPython HAL port for Lupine Engine */
#ifndef LUPINE_MPHALPORT_H
#define LUPINE_MPHALPORT_H

#include <stdint.h>

/* These are defined in MicroPythonEnvironment.cpp */
int mp_hal_stdin_rx_chr(void);
void mp_hal_stdout_tx_strn_cooked(const char* str, size_t len);

static inline void mp_hal_set_interrupt_char(char c) { (void)c; }

/* Timing functions - can be overridden */
#ifndef mp_hal_ticks_ms
typedef unsigned long mp_uint_t;
mp_uint_t mp_hal_ticks_ms(void);
mp_uint_t mp_hal_ticks_us(void);
mp_uint_t mp_hal_ticks_cpu(void);
void mp_hal_delay_ms(mp_uint_t ms);
void mp_hal_delay_us(mp_uint_t us);
#endif

#endif /* LUPINE_MPHALPORT_H */
'''
    output_file.write_text(content, encoding='utf-8', newline='\n')
    print(f"  Generated: {output_file.name}")


def create_unistd_stub(output_dir):
    """Create stub unistd.h for MSVC.

    This header lives on the global include path, so it shadows the real
    system <unistd.h> for every translation unit. Only MSVC lacks a real
    unistd.h; on every other toolchain (Emscripten, clang, gcc, mingw) we must
    fall through to the genuine header via #include_next, otherwise functions
    like getcwd/chdir/rmdir become undeclared.
    """
    output_file = output_dir / "unistd.h"
    content = '''/* Stub unistd.h for MSVC - MicroPython embed */
#ifndef LUPINE_STUB_UNISTD_H
#define LUPINE_STUB_UNISTD_H

#ifndef _MSC_VER

/* Real toolchains ship a proper unistd.h; defer to it. */
#include_next <unistd.h>

#else

#include <io.h>
#include <process.h>
#include <stdio.h>
#include <BaseTsd.h>

/* ssize_t for MSVC */
#ifndef _SSIZE_T_DEFINED
typedef SSIZE_T ssize_t;
#define _SSIZE_T_DEFINED
#endif

/* File access constants */
#ifndef F_OK
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1
#endif

#ifndef access
#define access _access
#endif

/* Standard file descriptors */
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif

/* SEEK constants */
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#endif /* _MSC_VER */

#endif /* LUPINE_STUB_UNISTD_H */
'''
    output_file.write_text(content, encoding='utf-8', newline='\n')
    print(f"  Generated: {output_file.name}")


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <output_dir> [python_executable]")
        sys.exit(1)

    output_dir = Path(sys.argv[1])
    python_exe = sys.argv[2] if len(sys.argv) > 2 else sys.executable

    print(f"Building MicroPython embed package...")
    print(f"  Output directory: {output_dir}")
    print(f"  Python executable: {python_exe}")

    try:
        mp_dir = find_micropython_dir()
        print(f"  MicroPython source: {mp_dir}")
    except RuntimeError as e:
        print(f"Error: {e}")
        sys.exit(1)

    # Create output directories
    genhdr_dir = output_dir / "genhdr"
    ensure_dir(output_dir)
    ensure_dir(genhdr_dir)

    print("\nGenerating headers...")

    # Generate all required headers
    create_mpconfigport_h(output_dir)
    create_mphalport_h(output_dir)
    create_unistd_stub(output_dir)

    generate_mpversion_h(genhdr_dir, mp_dir)
    generate_moduledefs_h(genhdr_dir)
    generate_root_pointers_h(genhdr_dir)
    generate_qstrdefs(genhdr_dir, mp_dir, python_exe)

    print("\nMicroPython embed package generated successfully!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
