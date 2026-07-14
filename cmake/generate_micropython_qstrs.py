#!/usr/bin/env python3
"""
Generate MicroPython qstrdefs.generated.h for Lupine Engine embed.
Uses the djb2 hash algorithm as defined in MicroPython.
"""

import sys

def compute_hash(s):
    """Compute djb2 hash as MicroPython does."""
    hash_val = 5381
    for c in s.encode('utf-8'):
        hash_val = ((hash_val << 5) + hash_val) ^ c
        hash_val &= 0xFFFFFFFF  # Keep as 32-bit
    hash_val &= 0xFFFF  # MicroPython uses 16-bit hash by default
    if hash_val == 0:
        hash_val = 1
    return hash_val

# Define all QSTRs needed
# Format: (qstr_id, string_value)
# Note: MP_QSTRnull must be first (the null/empty qstr)
QSTRS = [
    # MP_QSTRnull must be first entry (represents null/invalid qstr)
    ("MP_QSTRnull", ""),
    # MP_QSTR_ is also an empty string qstr used in some places
    ("", ""),
    # Special names
    ("__name__", "__name__"),
    ("__module__", "__module__"),
    ("__class__", "__class__"),
    ("__init__", "__init__"),
    ("__call__", "__call__"),
    ("__str__", "__str__"),
    ("__repr__", "__repr__"),
    ("__iter__", "__iter__"),
    ("__next__", "__next__"),
    ("__getattr__", "__getattr__"),
    ("__setattr__", "__setattr__"),
    ("__delattr__", "__delattr__"),
    ("__getitem__", "__getitem__"),
    ("__setitem__", "__setitem__"),
    ("__delitem__", "__delitem__"),
    ("__len__", "__len__"),
    ("__add__", "__add__"),
    ("__sub__", "__sub__"),
    ("__mul__", "__mul__"),
    ("__truediv__", "__truediv__"),
    ("__floordiv__", "__floordiv__"),
    ("__mod__", "__mod__"),
    ("__eq__", "__eq__"),
    ("__ne__", "__ne__"),
    ("__lt__", "__lt__"),
    ("__le__", "__le__"),
    ("__gt__", "__gt__"),
    ("__ge__", "__ge__"),
    ("print", "print"),
    ("len", "len"),
    ("range", "range"),
    ("list", "list"),
    ("dict", "dict"),
    ("tuple", "tuple"),
    ("set", "set"),
    ("str", "str"),
    ("int", "int"),
    ("float", "float"),
    ("bool", "bool"),
    ("None", "None"),
    ("True", "True"),
    ("False", "False"),
    ("Exception", "Exception"),
    ("self", "self"),
    ("value", "value"),
    # Context managers
    ("__enter__", "__enter__"),
    ("__exit__", "__exit__"),
    # Additional operators
    ("__neg__", "__neg__"),
    ("__pos__", "__pos__"),
    ("__invert__", "__invert__"),
    ("__and__", "__and__"),
    ("__or__", "__or__"),
    ("__xor__", "__xor__"),
    ("__lshift__", "__lshift__"),
    ("__rshift__", "__rshift__"),
    ("__pow__", "__pow__"),
    ("__matmul__", "__matmul__"),
    # Reverse operators
    ("__radd__", "__radd__"),
    ("__rsub__", "__rsub__"),
    ("__rmul__", "__rmul__"),
    ("__rtruediv__", "__rtruediv__"),
    # In-place operators
    ("__iadd__", "__iadd__"),
    ("__isub__", "__isub__"),
    ("__imul__", "__imul__"),
    ("__itruediv__", "__itruediv__"),
    # Container methods
    ("__contains__", "__contains__"),
    ("__hash__", "__hash__"),
    ("__bool__", "__bool__"),
    # Attribute access
    ("__dict__", "__dict__"),
    ("__bases__", "__bases__"),
    ("__doc__", "__doc__"),
    ("__new__", "__new__"),
    ("__del__", "__del__"),
    # Exception handling
    ("__traceback__", "__traceback__"),
    ("__cause__", "__cause__"),
    ("__context__", "__context__"),
    ("args", "args"),
    # Generator/coroutine
    ("send", "send"),
    ("throw", "throw"),
    ("close", "close"),
    # Common builtins
    ("abs", "abs"),
    ("all", "all"),
    ("any", "any"),
    ("append", "append"),
    ("bin", "bin"),
    ("bytes", "bytes"),
    ("bytearray", "bytearray"),
    ("callable", "callable"),
    ("chr", "chr"),
    ("classmethod", "classmethod"),
    ("clear", "clear"),
    ("compile", "compile"),
    ("complex", "complex"),
    ("copy", "copy"),
    ("count", "count"),
    ("delattr", "delattr"),
    ("dir", "dir"),
    ("divmod", "divmod"),
    ("enumerate", "enumerate"),
    ("eval", "eval"),
    ("exec", "exec"),
    ("extend", "extend"),
    ("filter", "filter"),
    ("find", "find"),
    ("format", "format"),
    ("frozenset", "frozenset"),
    ("get", "get"),
    ("getattr", "getattr"),
    ("globals", "globals"),
    ("hasattr", "hasattr"),
    ("hash", "hash"),
    ("hex", "hex"),
    ("id", "id"),
    ("index", "index"),
    ("input", "input"),
    ("insert", "insert"),
    ("isinstance", "isinstance"),
    ("issubclass", "issubclass"),
    ("items", "items"),
    ("iter", "iter"),
    ("join", "join"),
    ("keys", "keys"),
    # Note: 'lambda' is a Python keyword, but we need it as a qstr
    ("lambda", "lambda"),
    ("locals", "locals"),
    ("lower", "lower"),
    ("map", "map"),
    ("max", "max"),
    ("memoryview", "memoryview"),
    ("min", "min"),
    ("next", "next"),
    ("object", "object"),
    ("oct", "oct"),
    ("open", "open"),
    ("ord", "ord"),
    ("pop", "pop"),
    ("pow", "pow"),
    ("property", "property"),
    ("remove", "remove"),
    ("replace", "replace"),
    ("repr", "repr"),
    ("reverse", "reverse"),
    ("reversed", "reversed"),
    ("round", "round"),
    ("setattr", "setattr"),
    ("setdefault", "setdefault"),
    ("slice", "slice"),
    ("sort", "sort"),
    ("sorted", "sorted"),
    ("split", "split"),
    ("staticmethod", "staticmethod"),
    ("strip", "strip"),
    ("sum", "sum"),
    ("super", "super"),
    ("type", "type"),
    ("update", "update"),
    ("upper", "upper"),
    ("values", "values"),
    ("zip", "zip"),
    # Exception types
    ("BaseException", "BaseException"),
    ("TypeError", "TypeError"),
    ("ValueError", "ValueError"),
    ("AttributeError", "AttributeError"),
    ("KeyError", "KeyError"),
    ("IndexError", "IndexError"),
    ("RuntimeError", "RuntimeError"),
    ("StopIteration", "StopIteration"),
    ("GeneratorExit", "GeneratorExit"),
    ("ImportError", "ImportError"),
    ("NameError", "NameError"),
    ("OSError", "OSError"),
    ("NotImplementedError", "NotImplementedError"),
    ("ZeroDivisionError", "ZeroDivisionError"),
    ("OverflowError", "OverflowError"),
    ("MemoryError", "MemoryError"),
    ("AssertionError", "AssertionError"),
    ("SyntaxError", "SyntaxError"),
    ("IndentationError", "IndentationError"),
    ("SystemExit", "SystemExit"),
    ("KeyboardInterrupt", "KeyboardInterrupt"),
    # More special methods
    ("__abs__", "__abs__"),
    ("__bytes__", "__bytes__"),
    ("__divmod__", "__divmod__"),
    ("__float__", "__float__"),
    ("__int__", "__int__"),
    ("__reversed__", "__reversed__"),
    ("__sizeof__", "__sizeof__"),
    # Module attributes
    ("__file__", "__file__"),
    ("__path__", "__path__"),
    ("__builtins__", "__builtins__"),
    ("__import__", "__import__"),
    ("__all__", "__all__"),
    # Descriptors
    ("__get__", "__get__"),
    ("__set__", "__set__"),
    ("__delete__", "__delete__"),
    # sys module
    ("sys", "sys"),
    ("path", "path"),
    ("argv", "argv"),
    ("exit", "exit"),
    ("modules", "modules"),
    ("platform", "platform"),
    ("version", "version"),
    ("version_info", "version_info"),
    ("implementation", "implementation"),
    ("stdout", "stdout"),
    ("stderr", "stderr"),
    ("stdin", "stdin"),
    # Common string methods
    ("encode", "encode"),
    ("decode", "decode"),
    ("startswith", "startswith"),
    ("endswith", "endswith"),
    ("isalpha", "isalpha"),
    ("isdigit", "isdigit"),
    ("isspace", "isspace"),
    ("lstrip", "lstrip"),
    ("rstrip", "rstrip"),
    ("partition", "partition"),
    ("rpartition", "rpartition"),
    ("rsplit", "rsplit"),
    ("rfind", "rfind"),
    ("rindex", "rindex"),
    ("center", "center"),
    ("ljust", "ljust"),
    ("rjust", "rjust"),
    ("zfill", "zfill"),
    # Math module
    ("math", "math"),
    ("sin", "sin"),
    ("cos", "cos"),
    ("tan", "tan"),
    ("sqrt", "sqrt"),
    ("floor", "floor"),
    ("ceil", "ceil"),
    ("pi", "pi"),
    ("e", "e"),
    ("log", "log"),
    ("exp", "exp"),
    ("fabs", "fabs"),
    ("atan2", "atan2"),
    ("asin", "asin"),
    ("acos", "acos"),
    ("atan", "atan"),
    ("degrees", "degrees"),
    ("radians", "radians"),
    # gc module
    ("gc", "gc"),
    ("collect", "collect"),
    ("enable", "enable"),
    ("disable", "disable"),
    ("isenabled", "isenabled"),
    ("mem_alloc", "mem_alloc"),
    ("mem_free", "mem_free"),
    ("threshold", "threshold"),
    # array module
    ("array", "array"),
    ("typecode", "typecode"),
    # struct module
    ("struct", "struct"),
    ("pack", "pack"),
    ("unpack", "unpack"),
    ("pack_into", "pack_into"),
    ("unpack_from", "unpack_from"),
    ("calcsize", "calcsize"),
    # json module
    ("json", "json"),
    ("dumps", "dumps"),
    ("loads", "loads"),
    # re module
    ("re", "re"),
    ("match", "match"),
    ("search", "search"),
    ("sub", "sub"),
    ("group", "group"),
    ("groups", "groups"),
    ("span", "span"),
    ("start", "start"),
    ("end", "end"),
    # collections module
    ("collections", "collections"),
    ("namedtuple", "namedtuple"),
    ("OrderedDict", "OrderedDict"),
    ("deque", "deque"),
    # Common identifiers
    ("key", "key"),
    ("default", "default"),
    ("step", "step"),
    ("stop", "stop"),
    ("data", "data"),
    ("size", "size"),
    ("read", "read"),
    ("write", "write"),
    ("readline", "readline"),
    ("readlines", "readlines"),
    ("writelines", "writelines"),
    ("seek", "seek"),
    ("tell", "tell"),
    ("flush", "flush"),
    ("mode", "mode"),
    ("name", "name"),
    ("buffer", "buffer"),
    # More special attributes
    ("__slots__", "__slots__"),
    ("__weakref__", "__weakref__"),
    ("__mro__", "__mro__"),
    ("__subclasses__", "__subclasses__"),
    ("__qualname__", "__qualname__"),
    ("__globals__", "__globals__"),
    ("__locals__", "__locals__"),
    ("__code__", "__code__"),
    ("__closure__", "__closure__"),
    ("__annotations__", "__annotations__"),
    ("__kwdefaults__", "__kwdefaults__"),
    ("__defaults__", "__defaults__"),
    # More exception attributes
    ("errno", "errno"),
    ("strerror", "strerror"),
    ("filename", "filename"),
    ("lineno", "lineno"),
    ("offset", "offset"),
    ("text", "text"),
    ("msg", "msg"),
    # Write method
    ("__build_class__", "__build_class__"),
    ("__prepare__", "__prepare__"),
    ("__instancecheck__", "__instancecheck__"),
    ("__subclasscheck__", "__subclasscheck__"),
    # More operators
    ("__iand__", "__iand__"),
    ("__ior__", "__ior__"),
    ("__ixor__", "__ixor__"),
    ("__ilshift__", "__ilshift__"),
    ("__irshift__", "__irshift__"),
    ("__ifloordiv__", "__ifloordiv__"),
    ("__imod__", "__imod__"),
    ("__ipow__", "__ipow__"),
    ("__imatmul__", "__imatmul__"),
    # Reverse ops
    ("__rfloordiv__", "__rfloordiv__"),
    ("__rmod__", "__rmod__"),
    ("__rpow__", "__rpow__"),
    ("__rand__", "__rand__"),
    ("__ror__", "__ror__"),
    ("__rxor__", "__rxor__"),
    ("__rlshift__", "__rlshift__"),
    ("__rrshift__", "__rrshift__"),
    ("__rmatmul__", "__rmatmul__"),
    # builtin_names from runtime
    ("micropython", "micropython"),
    ("builtins", "builtins"),
    ("__main__", "__main__"),
    ("const", "const"),
    ("native", "native"),
    ("viper", "viper"),
    ("asm_thumb", "asm_thumb"),
    ("asm_xtensa", "asm_xtensa"),
    # More VM internals
    ("__aiter__", "__aiter__"),
    ("__anext__", "__anext__"),
    ("__await__", "__await__"),
    # More common
    ("encoding", "encoding"),
    ("errors", "errors"),
    ("sep", "sep"),
    ("file", "file"),
    ("newline", "newline"),
    ("buffering", "buffering"),
    ("iterable", "iterable"),
    ("number", "number"),
    ("string", "string"),
    # More slice-related
    ("indices", "indices"),
    # NoneType
    ("NoneType", "NoneType"),
    # ellipsis
    ("Ellipsis", "Ellipsis"),
    # NotImplemented
    ("NotImplemented", "NotImplemented"),
    # Function introspection
    ("function", "function"),
    ("generator", "generator"),
    ("method", "method"),
    ("bound_method", "bound_method"),
    ("module", "module"),
    # Special stdin used by embed_util
    ("_lt_stdin_gt_", "<stdin>"),
    # Common punctuation/format
    ("utf_hyphen_8", "utf-8"),

    # Compile modes (from builtinevex.c)
    ("single", "single"),
    ("_lt_string_gt_", "<string>"),
    ("bytecode", "bytecode"),
    ("_star_", "*"),

    # Async exceptions and methods
    ("StopAsyncIteration", "StopAsyncIteration"),
    ("__aenter__", "__aenter__"),
    ("__aexit__", "__aexit__"),

    # Error messages with spaces (MicroPython encodes spaces as _space_)
    ("maximum_space_recursion_space_depth_space_exceeded", "maximum recursion depth exceeded"),

    # From compile.c and runtime.c
    ("_lt_module_gt_", "<module>"),
    ("_lt_lambda_gt_", "<lambda>"),
    ("_lt_listcomp_gt_", "<listcomp>"),
    ("_lt_dictcomp_gt_", "<dictcomp>"),
    ("_lt_setcomp_gt_", "<setcomp>"),
    ("_lt_genexpr_gt_", "<genexpr>"),

    # Object type representations
    ("_lt_class_space__quote_", "<class '"),
    ("_quote__gt_", "'>"),
    ("_lt_function_space_", "<function "),
    ("_lt_generator_space_", "<generator "),
    ("_lt_bound_method_space_", "<bound method "),
    ("_space_of_space_", " of "),
    ("_space_at_space_", " at "),
    ("_gt_", ">"),

    # Keyword args (** becomes _star__star_)
    ("_star__star_", "**"),

    # From objtype.c
    ("__set_name__", "__set_name__"),
    ("__init_subclass__", "__init_subclass__"),
    ("mro", "mro"),

    # From objstr.c
    ("capitalize", "capitalize"),
    ("title", "title"),
    ("swapcase", "swapcase"),
    ("isalnum", "isalnum"),
    ("isupper", "isupper"),
    ("islower", "islower"),
    ("istitle", "istitle"),
    ("isprintable", "isprintable"),
    ("isidentifier", "isidentifier"),
    ("isdecimal", "isdecimal"),
    ("isnumeric", "isnumeric"),
    ("expandtabs", "expandtabs"),
    ("splitlines", "splitlines"),
    ("maketrans", "maketrans"),
    ("translate", "translate"),

    # From objdict.c
    ("popitem", "popitem"),
    ("fromkeys", "fromkeys"),
    ("dict_view", "dict_view"),
    ("dict_items", "dict_items"),
    ("dict_keys", "dict_keys"),
    ("dict_values", "dict_values"),

    # From objlist.c
    ("popleft", "popleft"),
    ("appendleft", "appendleft"),
    ("extendleft", "extendleft"),
    ("rotate", "rotate"),

    # From objint.c
    ("to_bytes", "to_bytes"),
    ("from_bytes", "from_bytes"),
    ("bit_length", "bit_length"),
    ("byteorder", "byteorder"),
    ("little", "little"),
    ("big", "big"),
    ("signed", "signed"),

    # From objfloat.c
    ("is_integer", "is_integer"),
    ("as_integer_ratio", "as_integer_ratio"),
    ("hex", "hex"),
    ("fromhex", "fromhex"),

    # From objcomplex.c
    ("real", "real"),
    ("imag", "imag"),
    ("conjugate", "conjugate"),

    # From objarray.c
    ("frombytes", "frombytes"),
    ("tobytes", "tobytes"),

    # From objrange.c

    # From objslice.c

    # Additional builtins
    ("breakpoint", "breakpoint"),
    ("ascii", "ascii"),
    ("vars", "vars"),

    # Internal object names
    ("code", "code"),
    ("cell", "cell"),
    ("closure", "closure"),
    ("iterator", "iterator"),
    ("coroutine", "coroutine"),
    ("async_generator", "async_generator"),

    # More exception types
    ("LookupError", "LookupError"),
    ("ArithmeticError", "ArithmeticError"),
    ("UnicodeError", "UnicodeError"),
    ("UnicodeDecodeError", "UnicodeDecodeError"),
    ("UnicodeEncodeError", "UnicodeEncodeError"),
    ("EOFError", "EOFError"),
    ("FileNotFoundError", "FileNotFoundError"),
    ("PermissionError", "PermissionError"),
    ("TimeoutError", "TimeoutError"),
    ("ConnectionError", "ConnectionError"),
    ("BrokenPipeError", "BrokenPipeError"),
    ("ConnectionResetError", "ConnectionResetError"),
    ("ConnectionAbortedError", "ConnectionAbortedError"),
    ("ConnectionRefusedError", "ConnectionRefusedError"),
    ("IsADirectoryError", "IsADirectoryError"),
    ("NotADirectoryError", "NotADirectoryError"),
    ("InterruptedError", "InterruptedError"),
    ("BlockingIOError", "BlockingIOError"),
    ("ChildProcessError", "ChildProcessError"),
    ("ProcessLookupError", "ProcessLookupError"),
    ("FileExistsError", "FileExistsError"),
    ("RecursionError", "RecursionError"),

    # More keywords
    ("finally", "finally"),
    ("except", "except"),
    ("raise", "raise"),
    ("return", "return"),
    ("yield", "yield"),
    ("pass", "pass"),
    ("break", "break"),
    ("continue", "continue"),
    ("import", "import"),
    ("from", "from"),
    ("as", "as"),
    ("global", "global"),
    ("nonlocal", "nonlocal"),
    ("assert", "assert"),
    ("del", "del"),
    ("with", "with"),
    ("async", "async"),
    ("await", "await"),
    ("class", "class"),
    ("def", "def"),
    ("if", "if"),
    ("elif", "elif"),
    ("else", "else"),
    ("for", "for"),
    ("while", "while"),
    ("try", "try"),
    ("and", "and"),
    ("or", "or"),
    ("not", "not"),
    ("in", "in"),
    ("is", "is"),

    # print function args
    ("flush", "flush"),

    # More sys module
    ("exc_info", "exc_info"),
    ("print_exception", "print_exception"),
    ("getsizeof", "getsizeof"),
    ("getrefcount", "getrefcount"),
    ("maxsize", "maxsize"),
    ("byteorder", "byteorder"),

    # From scheduler
    ("schedule", "schedule"),

    # From gc module
    ("heap_lock", "heap_lock"),
    ("heap_unlock", "heap_unlock"),

    # micropython module
    ("opt_level", "opt_level"),
    ("mem_info", "mem_info"),
    ("qstr_info", "qstr_info"),
    ("stack_use", "stack_use"),
    ("kbd_intr", "kbd_intr"),
    ("heap_lock", "heap_lock"),
    ("heap_unlock", "heap_unlock"),
    ("schedule", "schedule"),

    # More internal strings
    ("object_space_not_space_callable", "object not callable"),
    ("object_space_not_space_subscriptable", "object not subscriptable"),
    ("object_space_not_space_iterable", "object not iterable"),
    ("object_space_of_space_type_space_", "object of type "),
    ("_space_has_space_no_space_len", " has no len"),
    ("_space_is_space_not_space_", " is not "),
    ("_space_indices_space_must_space_be_space_integers", " indices must be integers"),
    ("can_quote_t_space_convert_space_", "can't convert "),
    ("_space_to_space_", " to "),
    ("_space_implicitly", " implicitly"),

    # Type name strings
    ("type", "type"),
    ("str", "str"),
    ("bytes", "bytes"),
    ("int", "int"),
    ("float", "float"),
    ("complex", "complex"),
    ("list", "list"),
    ("tuple", "tuple"),
    ("dict", "dict"),
    ("set", "set"),
    ("frozenset", "frozenset"),
    ("range", "range"),
    ("slice", "slice"),
    ("bool", "bool"),
    ("bytearray", "bytearray"),
    ("memoryview", "memoryview"),
    ("super", "super"),
    ("property", "property"),
    ("classmethod", "classmethod"),
    ("staticmethod", "staticmethod"),
    ("object", "object"),
    ("map", "map"),
    ("filter", "filter"),
    ("zip", "zip"),
    ("enumerate", "enumerate"),
    ("reversed", "reversed"),

    # More format strings
    ("_colon_", ":"),
    ("_percent_s", "%s"),

    # Format strings used by bin/hex/oct builtins
    ("_brace_open__colon__hash_b_brace_close_", "{:#b}"),
    ("_brace_open__colon__hash_x_brace_close_", "{:#x}"),
    ("_brace_open__colon__hash_o_brace_close_", "{:#o}"),

    # Special characters for print function
    ("_space_", " "),
    ("_0x0a_", "\n"),

    # bitwise operations messages
    ("unsupported_space_types_space_for_space_", "unsupported types for "),

    # attribute error messages
    ("_quote_", "'"),
    ("_space_object_space_has_space_no_space_attribute_space_", " object has no attribute "),

    # key error
    ("KeyError", "KeyError"),

    # Module names from mpstate
    ("__repl_print__", "__repl_print__"),

    # From objgenerator
    ("gi_frame", "gi_frame"),
    ("gi_running", "gi_running"),
    ("gi_code", "gi_code"),
    ("gi_yieldfrom", "gi_yieldfrom"),
    ("ag_frame", "ag_frame"),
    ("ag_running", "ag_running"),
    ("ag_code", "ag_code"),
    ("ag_await", "ag_await"),
    ("cr_frame", "cr_frame"),
    ("cr_running", "cr_running"),
    ("cr_code", "cr_code"),
    ("cr_await", "cr_await"),
    ("asend", "asend"),
    ("athrow", "athrow"),
    ("aclose", "aclose"),
]


def generate_qstrdefs(output_path):
    """Generate the qstrdefs.generated.h file.

    MicroPython uses two qstr pools:
    - QDEF0: Static pool (unsorted) - core qstrs for .mpy ABI compatibility
    - QDEF1: Main pool (sorted) - all other qstrs

    IMPORTANT: The QDEF1 pool MUST have at least 1 entry, otherwise
    qstr_find_strn() will have an integer underflow when computing
    'high = pool->len - 1' with len=0.
    """
    seen_ids = set()
    qdef0_count = 0
    qdef1_count = 0

    # Number of qstrs to put in QDEF0 (static pool)
    # These are core Python qstrs that need stable IDs for .mpy ABI
    # The rest go in QDEF1 (main pool) which must have len >= 1
    QDEF0_COUNT = 200

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("// Auto-generated QSTR definitions for Lupine Engine MicroPython embed\n")
        f.write("// Format: QDEF0/QDEF1(id, hash, len, str)\n")
        f.write("// Generated by generate_micropython_qstrs.py\n")
        f.write("//\n")
        f.write("// QDEF0 = static pool (unsorted, core qstrs for ABI compatibility)\n")
        f.write("// QDEF1 = main pool (sorted, must have at least 1 entry)\n\n")

        unique_entries = []
        for name_or_id, string_value in QSTRS:
            # Handle full IDs (like MP_QSTRnull) vs name suffixes
            if name_or_id.startswith("MP_QSTR") or name_or_id.startswith("MP_Q"):
                qstr_id = name_or_id
            elif name_or_id == "":
                qstr_id = "MP_QSTR_"  # Empty string qstr
            else:
                qstr_id = f"MP_QSTR_{name_or_id}"

            # Skip duplicates
            if qstr_id in seen_ids:
                continue
            seen_ids.add(qstr_id)
            unique_entries.append((qstr_id, string_value))

        # Write QDEF0 entries (static pool - first N entries)
        f.write("// === QDEF0: Static pool (unsorted, ABI-stable) ===\n")
        for i, (qstr_id, string_value) in enumerate(unique_entries):
            if i >= QDEF0_COUNT:
                break

            hash_val = compute_hash(string_value)
            str_len = len(string_value.encode('utf-8'))
            # Escape special characters for C string literal
            escaped_str = string_value.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\r', '\\r').replace('\t', '\\t')

            f.write(f'QDEF0({qstr_id}, {hash_val}, {str_len}, "{escaped_str}")\n')
            qdef0_count += 1

        # Write QDEF1 entries (main pool - remaining entries)
        # These are sorted by string value for binary search
        remaining = unique_entries[QDEF0_COUNT:]
        remaining.sort(key=lambda x: x[1])  # Sort by string value

        f.write("\n// === QDEF1: Main pool (sorted for binary search) ===\n")
        for qstr_id, string_value in remaining:
            hash_val = compute_hash(string_value)
            str_len = len(string_value.encode('utf-8'))
            # Escape special characters for C string literal
            escaped_str = string_value.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\r', '\\r').replace('\t', '\\t')

            f.write(f'QDEF1({qstr_id}, {hash_val}, {str_len}, "{escaped_str}")\n')
            qdef1_count += 1

        f.write("\n// End of generated qstrs\n")

    total = qdef0_count + qdef1_count
    print(f"Generated {output_path} with {total} unique qstrs:")
    print(f"  - QDEF0 (static pool): {qdef0_count}")
    print(f"  - QDEF1 (main pool): {qdef1_count}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <output_path>")
        sys.exit(1)

    generate_qstrdefs(sys.argv[1])
