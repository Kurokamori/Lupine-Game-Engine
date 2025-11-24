#!/usr/bin/env python3
"""
Lupine Engine Editor
Entry point for the PyQt-based game editor
"""

import sys
import os

# Add editor directory to path
editor_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'editor')
sys.path.insert(0, editor_dir)

def load_engine_modules():
    """
    Attempt to load the Lupine engine Python modules.
    Returns a tuple of (engine_module, runtime_module, success_messages, error_messages)
    """
    engine_module = None
    runtime_module = None
    success_messages = []
    error_messages = []

    # Try to import lupine_engine
    try:
        import lupine_engine
        engine_module = lupine_engine
        version = lupine_engine.__version__ if hasattr(lupine_engine, '__version__') else 'unknown'
        success_messages.append(f"[OK] lupine_engine module loaded successfully (version {version})")
    except ImportError as e:
        error_messages.append(f"[ERROR] Failed to load lupine_engine module: {e}")
    except Exception as e:
        error_messages.append(f"[ERROR] Error loading lupine_engine module: {e}")

    # Try to import lupine_runtime
    try:
        import lupine_runtime
        runtime_module = lupine_runtime
        version = lupine_runtime.__version__ if hasattr(lupine_runtime, '__version__') else 'unknown'
        success_messages.append(f"[OK] lupine_runtime module loaded successfully (version {version})")
    except ImportError as e:
        error_messages.append(f"[ERROR] Failed to load lupine_runtime module: {e}")
    except Exception as e:
        error_messages.append(f"[ERROR] Error loading lupine_runtime module: {e}")

    return engine_module, runtime_module, success_messages, error_messages

def main():
    """Main entry point for the Lupine Engine Editor"""
    print("=" * 60)
    print("Lupine Engine Editor v0.1.0")
    print("=" * 60)
    print()

    # Load engine modules
    print("Loading engine modules...")
    engine, runtime, success_msgs, error_msgs = load_engine_modules()

    # Print results
    print()
    for msg in success_msgs:
        print(msg)

    if error_msgs:
        print()
        for msg in error_msgs:
            print(msg)
        print()
        print("Note: Make sure to build the project first using CMake.")
        print("The Python modules should be automatically copied to the editor folder.")

    print()
    print("=" * 60)

    # Initialize PyQt application
    from PyQt6.QtWidgets import QApplication
    from editor.theme import get_theme_manager
    from editor.project_manager import ProjectManager
    from editor.main_editor import MainEditor

    app = QApplication(sys.argv)
    
    # Apply theme
    theme_manager = get_theme_manager()
    theme_manager.apply_theme(app)
    
    print("Starting Project Manager...")
    print()
    
    # Editor state class to hold references
    class EditorState:
        def __init__(self):
            self.current_editor = None
            self.project_manager = None
    
    state = EditorState()
    
    def open_editor(project_data):
        """Open the main editor with the selected project"""
        print("\n" + "=" * 60)
        print("OPENING EDITOR")
        print("=" * 60)
        
        # Create and show editor
        state.current_editor = MainEditor(project_data)
        state.current_editor.project_closed.connect(open_project_manager)
        state.current_editor.show()
        
        # Close project manager after showing editor
        if state.project_manager:
            state.project_manager.hide()
        
        # Apply theme to editor
        theme_manager.apply_theme(app)
    
    def open_project_manager():
        """Open or reopen the project manager"""
        # Close editor if open
        if state.current_editor:
            state.current_editor.deleteLater()
            state.current_editor = None
        
        # Create and show project manager
        state.project_manager = ProjectManager()
        state.project_manager.project_selected.connect(open_editor)
        state.project_manager.show()
        
        # Apply theme
        theme_manager.apply_theme(app)
    
    # Start with project manager
    open_project_manager()
    
    # Run application
    return app.exec()

if __name__ == "__main__":
    sys.exit(main())
