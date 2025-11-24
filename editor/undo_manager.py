"""
Undo/Redo Manager for Lupine Engine Editor
Manages undo/redo operations for both C++ (via EditorBridge) and Python-side operations
"""

from typing import Optional, Callable, Any, Dict, List
from dataclasses import dataclass
import json


@dataclass
class TextEditCommand:
    """Command for text editing operations"""
    file_path: str
    old_text: str
    new_text: str
    cursor_position: int
    description: str
    
    def execute(self, editor_widget) -> bool:
        """Execute the command"""
        try:
            editor_widget.setPlainText(self.new_text)
            cursor = editor_widget.textCursor()
            cursor.setPosition(self.cursor_position)
            editor_widget.setTextCursor(cursor)
            return True
        except Exception as e:
            print(f"Failed to execute text edit command: {e}")
            return False
    
    def undo(self, editor_widget) -> bool:
        """Undo the command"""
        try:
            editor_widget.setPlainText(self.old_text)
            return True
        except Exception as e:
            print(f"Failed to undo text edit command: {e}")
            return False


class UndoManager:
    """
    Manages undo/redo operations for the editor
    Coordinates between C++ command history (via EditorBridge) and Python-side operations
    """
    
    def __init__(self, editor_bridge=None):
        """
        Initialize the undo manager
        
        Args:
            editor_bridge: The EditorBridge instance for C++ command history
        """
        self.editor_bridge = editor_bridge
        
        # Python-side command history (for script editor, etc.)
        self.python_commands: List[Any] = []
        self.python_current_index: int = 0
        self.max_python_history: int = 100
        
        # Track which context we're in (scene editing vs script editing)
        self.current_context: str = "scene"  # "scene" or "script"
        
        # Script editor references for text undo/redo
        self.script_editors: Dict[str, Any] = {}  # file_path -> editor widget
    
    def set_editor_bridge(self, editor_bridge):
        """Set the editor bridge"""
        self.editor_bridge = editor_bridge
    
    def set_context(self, context: str):
        """
        Set the current editing context
        
        Args:
            context: "scene" for scene editing, "script" for script editing
        """
        self.current_context = context
    
    def register_script_editor(self, file_path: str, editor_widget):
        """Register a script editor widget for undo/redo"""
        self.script_editors[file_path] = editor_widget
    
    def unregister_script_editor(self, file_path: str):
        """Unregister a script editor widget"""
        if file_path in self.script_editors:
            del self.script_editors[file_path]
    
    def can_undo(self) -> bool:
        """Check if undo is available in the current context"""
        if self.current_context == "scene":
            return self.editor_bridge and self.editor_bridge.can_undo()
        elif self.current_context == "script":
            return self.python_current_index > 0
        return False
    
    def can_redo(self) -> bool:
        """Check if redo is available in the current context"""
        if self.current_context == "scene":
            return self.editor_bridge and self.editor_bridge.can_redo()
        elif self.current_context == "script":
            return self.python_current_index < len(self.python_commands)
        return False
    
    def undo(self) -> bool:
        """Undo the last operation in the current context"""
        if self.current_context == "scene":
            if self.editor_bridge:
                return self.editor_bridge.undo()
        elif self.current_context == "script":
            return self._undo_python()
        return False
    
    def redo(self) -> bool:
        """Redo the last undone operation in the current context"""
        if self.current_context == "scene":
            if self.editor_bridge:
                return self.editor_bridge.redo()
        elif self.current_context == "script":
            return self._redo_python()
        return False
    
    def get_undo_description(self) -> str:
        """Get description of the command that would be undone"""
        if self.current_context == "scene":
            if self.editor_bridge:
                return self.editor_bridge.get_undo_description()
        elif self.current_context == "script":
            if self.can_undo():
                cmd = self.python_commands[self.python_current_index - 1]
                return getattr(cmd, 'description', 'Text Edit')
        return ""
    
    def get_redo_description(self) -> str:
        """Get description of the command that would be redone"""
        if self.current_context == "scene":
            if self.editor_bridge:
                return self.editor_bridge.get_redo_description()
        elif self.current_context == "script":
            if self.can_redo():
                cmd = self.python_commands[self.python_current_index]
                return getattr(cmd, 'description', 'Text Edit')
        return ""
    
    def _undo_python(self) -> bool:
        """Undo a Python-side command"""
        if not self.can_undo():
            return False
        
        cmd = self.python_commands[self.python_current_index - 1]
        # Get the appropriate editor widget
        editor_widget = self.script_editors.get(cmd.file_path)
        if editor_widget and cmd.undo(editor_widget):
            self.python_current_index -= 1
            return True
        return False
    
    def _redo_python(self) -> bool:
        """Redo a Python-side command"""
        if not self.can_redo():
            return False
        
        cmd = self.python_commands[self.python_current_index]
        # Get the appropriate editor widget
        editor_widget = self.script_editors.get(cmd.file_path)
        if editor_widget and cmd.execute(editor_widget):
            self.python_current_index += 1
            return True
        return False

