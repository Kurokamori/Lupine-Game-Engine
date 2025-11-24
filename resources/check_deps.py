import pefile
import sys
import os

def check_dll_dependencies(pyd_file):
    try:
        pe = pefile.PE(pyd_file)
        print(f"Dependencies for {pyd_file}:")
        print("=" * 60)

        if hasattr(pe, 'DIRECTORY_ENTRY_IMPORT'):
            for entry in pe.DIRECTORY_ENTRY_IMPORT:
                dll_name = entry.dll.decode('utf-8')
                # Check if DLL exists in system
                dll_found = False
                try:
                    import ctypes
                    ctypes.WinDLL(dll_name)
                    dll_found = True
                    status = "✓ FOUND"
                except:
                    status = "✗ MISSING"

                print(f"  {status} - {dll_name}")

                # Show imported functions
                if hasattr(entry, 'imports'):
                    for imp in entry.imports[:5]:  # Show first 5 imports
                        if imp.name:
                            print(f"      → {imp.name.decode('utf-8')}")
        else:
            print("  No import directory found")

        print("=" * 60)
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

print("Checking lupine_engine.cp312-win_amd64.pyd")
check_dll_dependencies(r"editor\lupine_engine.cp312-win_amd64.pyd")
print()
print("Checking lupine_runtime.cp312-win_amd64.pyd")
check_dll_dependencies(r"editor\lupine_runtime.cp312-win_amd64.pyd")

