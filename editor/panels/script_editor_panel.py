"""
Script Editor Panel
Edit scripts and code files with syntax highlighting
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                             QPushButton, QTextEdit, QComboBox, QTabWidget,
                             QSplitter, QFileDialog, QMessageBox, QDialog,
                             QFormLayout, QCheckBox, QDialogButtonBox, QListWidget,
                             QListWidgetItem, QLineEdit)
from PyQt6.QtCore import Qt, pyqtSignal, QRegularExpression
from PyQt6.QtGui import (QFont, QSyntaxHighlighter, QTextCharFormat, QColor,
                        QFontMetrics, QPalette)
from pathlib import Path
from .base_panel import EditorPanel
import markdown


class SyntaxHighlighter(QSyntaxHighlighter):
    """Base syntax highlighter"""
    
    def __init__(self, document, language="python"):
        super().__init__(document)
        self.language = language
        self.highlighting_rules = []
        self._setup_rules()
    
    def _setup_rules(self):
        """Setup syntax highlighting rules based on language"""
        self.highlighting_rules = []
        
        # Keyword format
        keyword_format = QTextCharFormat()
        keyword_format.setForeground(QColor("#C586C0"))
        keyword_format.setFontWeight(QFont.Weight.Bold)
        
        # String format
        string_format = QTextCharFormat()
        string_format.setForeground(QColor("#CE9178"))
        
        # Comment format
        comment_format = QTextCharFormat()
        comment_format.setForeground(QColor("#6A9955"))
        comment_format.setFontItalic(True)
        
        # Number format
        number_format = QTextCharFormat()
        number_format.setForeground(QColor("#B5CEA8"))
        
        # Function format
        function_format = QTextCharFormat()
        function_format.setForeground(QColor("#DCDCAA"))
        
        # Class format
        class_format = QTextCharFormat()
        class_format.setForeground(QColor("#4EC9B0"))
        
        if self.language == "python":
            keywords = [
                'and', 'as', 'assert', 'break', 'class', 'continue', 'def',
                'del', 'elif', 'else', 'except', 'False', 'finally', 'for',
                'from', 'global', 'if', 'import', 'in', 'is', 'lambda', 'None',
                'nonlocal', 'not', 'or', 'pass', 'raise', 'return', 'True',
                'try', 'while', 'with', 'yield', 'async', 'await'
            ]
            
            for word in keywords:
                pattern = QRegularExpression(f'\\b{word}\\b')
                self.highlighting_rules.append((pattern, keyword_format))
            
            # Python strings
            self.highlighting_rules.append((QRegularExpression('"[^"\\\\]*(\\\\.[^"\\\\]*)*"'), string_format))
            self.highlighting_rules.append((QRegularExpression("'[^'\\\\]*(\\\\.[^'\\\\]*)*'"), string_format))
            
            # Python comments
            self.highlighting_rules.append((QRegularExpression('#[^\n]*'), comment_format))
            
            # Python functions
            self.highlighting_rules.append((QRegularExpression('\\bdef\\s+(\\w+)'), function_format))
            
            # Python classes
            self.highlighting_rules.append((QRegularExpression('\\bclass\\s+(\\w+)'), class_format))
            
        elif self.language == "lua":
            keywords = [
                'and', 'break', 'do', 'else', 'elseif', 'end', 'false', 'for',
                'function', 'if', 'in', 'local', 'nil', 'not', 'or', 'repeat',
                'return', 'then', 'true', 'until', 'while'
            ]

            for word in keywords:
                pattern = QRegularExpression(f'\\b{word}\\b')
                self.highlighting_rules.append((pattern, keyword_format))

            # Lua strings
            self.highlighting_rules.append((QRegularExpression('"[^"\\\\]*(\\\\.[^"\\\\]*)*"'), string_format))
            self.highlighting_rules.append((QRegularExpression("'[^'\\\\]*(\\\\.[^'\\\\]*)*'"), string_format))

            # Lua comments
            self.highlighting_rules.append((QRegularExpression('--[^\n]*'), comment_format))

        elif self.language == "ruby" or self.language == "mruby":
            keywords = [
                'begin', 'end', 'class', 'module', 'def', 'if', 'unless', 'case',
                'when', 'while', 'until', 'for', 'break', 'next', 'redo', 'retry',
                'return', 'yield', 'super', 'self', 'nil', 'true', 'false', 'and',
                'or', 'not', 'then', 'else', 'elsif', 'ensure', 'rescue', 'raise',
                'do', 'in', 'alias', 'defined?', '__FILE__', '__LINE__'
            ]

            for word in keywords:
                pattern = QRegularExpression(f'\\b{word}\\b')
                self.highlighting_rules.append((pattern, keyword_format))

            # Ruby strings
            self.highlighting_rules.append((QRegularExpression('"[^"\\\\]*(\\\\.[^"\\\\]*)*"'), string_format))
            self.highlighting_rules.append((QRegularExpression("'[^'\\\\]*(\\\\.[^'\\\\]*)*'"), string_format))

            # Ruby comments
            self.highlighting_rules.append((QRegularExpression('#[^\n]*'), comment_format))

            # Ruby symbols
            symbol_format = QTextCharFormat()
            symbol_format.setForeground(QColor("#4FC1FF"))
            self.highlighting_rules.append((QRegularExpression(':[a-zA-Z_]\\w*'), symbol_format))

            # Ruby instance variables
            instance_var_format = QTextCharFormat()
            instance_var_format.setForeground(QColor("#9CDCFE"))
            self.highlighting_rules.append((QRegularExpression('@[a-zA-Z_]\\w*'), instance_var_format))

            # Ruby class variables
            self.highlighting_rules.append((QRegularExpression('@@[a-zA-Z_]\\w*'), instance_var_format))

            # Ruby global variables
            self.highlighting_rules.append((QRegularExpression('\\$[a-zA-Z_]\\w*'), instance_var_format))
            
        elif self.language in ["cpp", "c"]:
            keywords = [
                'alignas', 'alignof', 'and', 'and_eq', 'asm', 'auto', 'bitand',
                'bitor', 'bool', 'break', 'case', 'catch', 'char', 'char16_t',
                'char32_t', 'class', 'compl', 'const', 'constexpr', 'const_cast',
                'continue', 'decltype', 'default', 'delete', 'do', 'double',
                'dynamic_cast', 'else', 'enum', 'explicit', 'export', 'extern',
                'false', 'float', 'for', 'friend', 'goto', 'if', 'inline', 'int',
                'long', 'mutable', 'namespace', 'new', 'noexcept', 'not', 'not_eq',
                'nullptr', 'operator', 'or', 'or_eq', 'private', 'protected',
                'public', 'register', 'reinterpret_cast', 'return', 'short',
                'signed', 'sizeof', 'static', 'static_assert', 'static_cast',
                'struct', 'switch', 'template', 'this', 'thread_local', 'throw',
                'true', 'try', 'typedef', 'typeid', 'typename', 'union', 'unsigned',
                'using', 'virtual', 'void', 'volatile', 'wchar_t', 'while', 'xor',
                'xor_eq', 'override', 'final'
            ]
            
            for word in keywords:
                pattern = QRegularExpression(f'\\b{word}\\b')
                self.highlighting_rules.append((pattern, keyword_format))
            
            # C++ strings
            self.highlighting_rules.append((QRegularExpression('"[^"\\\\]*(\\\\.[^"\\\\]*)*"'), string_format))
            
            # C++ comments
            self.highlighting_rules.append((QRegularExpression('//[^\n]*'), comment_format))
            
            # C++ preprocessor
            preprocessor_format = QTextCharFormat()
            preprocessor_format.setForeground(QColor("#C586C0"))
            self.highlighting_rules.append((QRegularExpression('#\\s*\\w+'), preprocessor_format))
            
        elif self.language == "json":
            # JSON strings (keys and values)
            self.highlighting_rules.append((QRegularExpression('"[^"\\\\]*(\\\\.[^"\\\\]*)*"'), string_format))
            
            # JSON keywords
            json_keywords = ['true', 'false', 'null']
            for word in json_keywords:
                pattern = QRegularExpression(f'\\b{word}\\b')
                self.highlighting_rules.append((pattern, keyword_format))
        
        # Numbers (common to all)
        self.highlighting_rules.append((QRegularExpression('\\b[0-9]+\\.?[0-9]*\\b'), number_format))
    
    def highlightBlock(self, text):
        """Apply syntax highlighting to a block of text"""
        for pattern, format in self.highlighting_rules:
            match_iterator = pattern.globalMatch(text)
            while match_iterator.hasNext():
                match = match_iterator.next()
                self.setFormat(match.capturedStart(), match.capturedLength(), format)


class CodeEditor(QTextEdit):
    """Code editor widget with line numbers and syntax highlighting"""

    modified_changed = pyqtSignal(bool)  # Signal when modified state changes

    def __init__(self, parent=None, language="python"):
        super().__init__(parent)
        self.language = language
        self.file_path = None
        self.is_modified = False

        # Setup font
        font = QFont("Consolas", 10)
        font.setStyleHint(QFont.StyleHint.Monospace)
        self.setFont(font)

        # Setup tab width
        metrics = QFontMetrics(font)
        self.setTabStopDistance(4 * metrics.horizontalAdvance(' '))

        # Setup syntax highlighter
        self.highlighter = SyntaxHighlighter(self.document(), language)

        # Custom undo/redo system
        self.undo_stack = []
        self.redo_stack = []
        self.last_saved_text = ""
        self.text_snapshot_timer = None
        self.pending_snapshot = False

        # Disable built-in undo/redo
        self.setUndoRedoEnabled(False)

        # Track modifications
        self.textChanged.connect(self._on_text_changed)
    
    def _on_text_changed(self):
        """Track when text is modified"""
        if not self.is_modified:
            self.is_modified = True
            self.modified_changed.emit(True)

        # Schedule a snapshot for undo/redo (debounced)
        if not self.pending_snapshot:
            self.pending_snapshot = True
            from PyQt6.QtCore import QTimer
            QTimer.singleShot(500, self._take_snapshot)  # 500ms delay

    def _take_snapshot(self):
        """Take a snapshot of the current text for undo/redo"""
        self.pending_snapshot = False
        current_text = self.toPlainText()

        # Only add to undo stack if text has changed
        if not self.undo_stack or current_text != self.undo_stack[-1]:
            self.undo_stack.append(current_text)
            # Limit undo stack size
            if len(self.undo_stack) > 100:
                self.undo_stack.pop(0)
            # Clear redo stack when new edit is made
            self.redo_stack.clear()

    def custom_undo(self):
        """Custom undo implementation"""
        if len(self.undo_stack) > 1:
            # Move current state to redo stack
            current = self.undo_stack.pop()
            self.redo_stack.append(current)

            # Restore previous state
            previous = self.undo_stack[-1]
            self.setPlainText(previous)
            return True
        return False

    def custom_redo(self):
        """Custom redo implementation"""
        if self.redo_stack:
            # Get next state from redo stack
            next_state = self.redo_stack.pop()
            self.undo_stack.append(next_state)

            # Restore next state
            self.setPlainText(next_state)
            return True
        return False

    def can_undo(self):
        """Check if undo is available"""
        return len(self.undo_stack) > 1

    def can_redo(self):
        """Check if redo is available"""
        return len(self.redo_stack) > 0

    def keyPressEvent(self, event):
        """Override key press to handle custom undo/redo"""
        from PyQt6.QtCore import Qt
        from PyQt6.QtGui import QKeySequence

        # Check for Ctrl+Z (Undo)
        if event.matches(QKeySequence.StandardKey.Undo):
            self.custom_undo()
            event.accept()
            return

        # Check for Ctrl+Shift+Z or Ctrl+Y (Redo)
        if event.matches(QKeySequence.StandardKey.Redo):
            self.custom_redo()
            event.accept()
            return

        # Default handling for other keys
        super().keyPressEvent(event)
    
    def set_language(self, language):
        """Change the syntax highlighting language"""
        self.language = language
        self.highlighter = SyntaxHighlighter(self.document(), language)
    
    def load_file(self, file_path):
        """Load a file into the editor"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            self.setPlainText(content)
            self.file_path = file_path
            self.is_modified = False
            self.modified_changed.emit(False)

            # Reset undo/redo stacks
            self.undo_stack = [content]
            self.redo_stack = []
            self.last_saved_text = content

            # Set language based on file extension
            ext = Path(file_path).suffix.lower()
            lang_map = {
                '.py': 'python',
                '.lua': 'lua',
                '.rb': 'ruby',
                '.cpp': 'cpp', '.cc': 'cpp', '.cxx': 'cpp',
                '.c': 'c',
                '.h': 'cpp', '.hpp': 'cpp', '.hxx': 'cpp',
                '.json': 'json',
                '.md': 'markdown'
            }
            if ext in lang_map:
                self.set_language(lang_map[ext])

            return True
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to load file:\n{str(e)}")
            return False
    
    def save_file(self):
        """Save the current file"""
        if not self.file_path:
            return False
        
        try:
            with open(self.file_path, 'w', encoding='utf-8') as f:
                f.write(self.toPlainText())
            self.is_modified = False
            self.modified_changed.emit(False)
            return True
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to save file:\n{str(e)}")
            return False


class MarkdownPreview(QWidget):
    """Markdown preview widget"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout()
        layout.setContentsMargins(10, 10, 10, 10)
        
        self.preview = QTextEdit()
        self.preview.setReadOnly(True)
        layout.addWidget(self.preview)
        
        self.setLayout(layout)
    
    def update_preview(self, markdown_text):
        """Update the preview with markdown text"""
        try:
            html = markdown.markdown(markdown_text, extensions=['extra', 'codehilite'])
            self.preview.setHtml(html)
        except:
            self.preview.setPlainText(markdown_text)


class NewScriptDialog(QDialog):
    """Dialog for creating a new script file"""
    
    def __init__(self, parent=None, default_location=None):
        super().__init__(parent)
        self.default_location = default_location
        self.setWindowTitle("Create New Script")
        self.setModal(True)
        self.setMinimumWidth(500)
        
        layout = QVBoxLayout()
        
        # Form layout
        form_layout = QFormLayout()
        
        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("script_name")
        form_layout.addRow("Name:", self.name_edit)
        
        self.type_combo = QComboBox()
        self.type_combo.addItems([
            "Python Script (.py)",
            "Lua Script (.lua)",
            "mRuby Script (.rb)",
            "C++ Source (.cpp)",
            "C++ Header (.h)",
            "C Source (.c)",
            "JSON File (.json)",
            "Markdown (.md)",
            "GLSL Shader (.glsl)"
        ])
        self.type_combo.currentTextChanged.connect(self._on_type_changed)
        form_layout.addRow("Type:", self.type_combo)
        
        # Location selection
        location_layout = QHBoxLayout()
        self.location_edit = QLineEdit()
        self.location_edit.setText(str(default_location) if default_location else "")
        self.location_edit.setReadOnly(True)
        location_layout.addWidget(self.location_edit)
        
        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self._browse_location)
        browse_btn.setFixedWidth(80)
        location_layout.addWidget(browse_btn)
        
        form_layout.addRow("Location:", location_layout)
        
        # Create class checkbox (only for C/C++)
        self.create_class_check = QCheckBox("Create Class (generates .cpp/.h or .c/.h pair)")
        self.create_class_check.setVisible(False)
        form_layout.addRow("", self.create_class_check)
        
        layout.addLayout(form_layout)
        
        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
        
        self.setLayout(layout)
    
    def _on_type_changed(self, text):
        """Show/hide create class checkbox based on file type"""
        is_cpp = any(x in text for x in ['.cpp', '.h', '.c'])
        self.create_class_check.setVisible(is_cpp)
    
    def _browse_location(self):
        """Browse for save location"""
        directory = QFileDialog.getExistingDirectory(
            self,
            "Select Save Location",
            self.location_edit.text() if self.location_edit.text() else str(Path.home())
        )
        
        if directory:
            self.location_edit.setText(directory)
    
    def get_script_info(self):
        """Get script creation info"""
        name = self.name_edit.text()
        type_text = self.type_combo.currentText()
        extension = type_text.split('(')[1].split(')')[0]
        create_class = self.create_class_check.isChecked() and self.create_class_check.isVisible()
        location = self.location_edit.text()
        
        return {
            'name': name,
            'extension': extension,
            'create_class': create_class,
            'location': location
        }


class ScriptEditorPanel(EditorPanel):
    """Script editor panel for editing code files"""
    
    # Signals
    script_saved = pyqtSignal(str)  # script path
    script_closed = pyqtSignal(str)  # script path
    
    def __init__(self, parent=None):
        super().__init__("Script Editor", parent)
        self.setObjectName("ScriptEditorPanel")
        self.open_scripts = {}  # path -> editor widget
        self.script_list = []  # List of script paths for sorting
        self.project_root = None  # Will be set by main editor
    
    def _setup_panel(self):
        """Setup script editor panel UI"""
        # Main horizontal layout (script list on left, editor on right)
        main_splitter = QSplitter(Qt.Orientation.Horizontal)
        
        # Left side - Script list
        left_widget = QWidget()
        left_layout = QVBoxLayout()
        left_layout.setContentsMargins(0, 0, 0, 0)
        
        # Script list toolbar
        list_toolbar = QHBoxLayout()
        list_toolbar.setContentsMargins(5, 5, 5, 5)
        
        sort_label = QLabel("Sort:")
        list_toolbar.addWidget(sort_label)
        
        self.sort_combo = QComboBox()
        self.sort_combo.addItems(["Alphabetical", "Recent", "Language", "Inheritance"])
        self.sort_combo.currentTextChanged.connect(self._sort_scripts)
        list_toolbar.addWidget(self.sort_combo)
        
        left_layout.addLayout(list_toolbar)
        
        # Script list
        self.script_list_widget = QListWidget()
        self.script_list_widget.itemClicked.connect(self._on_script_selected)
        left_layout.addWidget(self.script_list_widget)
        
        left_widget.setLayout(left_layout)
        main_splitter.addWidget(left_widget)
        
        # Right side - Editor area
        right_widget = QWidget()
        right_layout = QVBoxLayout()
        right_layout.setContentsMargins(0, 0, 0, 0)
        
        # Toolbar
        toolbar_layout = QHBoxLayout()
        toolbar_layout.setContentsMargins(5, 5, 5, 5)
        
        new_btn = QPushButton("New")
        new_btn.clicked.connect(self._new_script)
        toolbar_layout.addWidget(new_btn)
        
        open_btn = QPushButton("Open")
        open_btn.clicked.connect(self._open_script)
        toolbar_layout.addWidget(open_btn)
        
        save_btn = QPushButton("Save")
        save_btn.clicked.connect(self._save_current_script)
        toolbar_layout.addWidget(save_btn)
        
        save_all_btn = QPushButton("Save All")
        save_all_btn.clicked.connect(self._save_all_scripts)
        toolbar_layout.addWidget(save_all_btn)
        
        close_btn = QPushButton("Close")
        close_btn.clicked.connect(self._close_current_script)
        toolbar_layout.addWidget(close_btn)
        
        toolbar_layout.addStretch()
        
        # Markdown preview toggle
        self.preview_toggle = QPushButton("Preview")
        self.preview_toggle.setCheckable(True)
        self.preview_toggle.setVisible(False)
        self.preview_toggle.toggled.connect(self._toggle_markdown_preview)
        toolbar_layout.addWidget(self.preview_toggle)
        
        right_layout.addLayout(toolbar_layout)
        
        # Tab widget for multiple open scripts
        self.script_tabs = QTabWidget()
        self.script_tabs.setTabsClosable(True)
        self.script_tabs.setMovable(True)
        self.script_tabs.tabCloseRequested.connect(self._close_script_tab)
        self.script_tabs.currentChanged.connect(self._on_tab_changed)
        right_layout.addWidget(self.script_tabs)
        
        right_widget.setLayout(right_layout)
        main_splitter.addWidget(right_widget)
        
        # Set splitter sizes (20% list, 80% editor)
        main_splitter.setStretchFactor(0, 1)
        main_splitter.setStretchFactor(1, 4)
        
        # Add to main layout
        self.content_layout.addWidget(main_splitter)
    
    def open_script(self, file_path):
        """Open a script file"""
        file_path = str(Path(file_path))
        
        # Check if already open
        if file_path in self.open_scripts:
            # Switch to existing tab
            for i in range(self.script_tabs.count()):
                if self.script_tabs.widget(i) == self.open_scripts[file_path]:
                    self.script_tabs.setCurrentIndex(i)
                    return
        
        # Determine if markdown
        is_markdown = file_path.endswith('.md')
        
        if is_markdown:
            # Create splitter for markdown with preview
            splitter = QSplitter(Qt.Orientation.Horizontal)
            
            editor = CodeEditor(language="markdown")
            editor.load_file(file_path)
            splitter.addWidget(editor)
            
            preview = MarkdownPreview()
            preview.update_preview(editor.toPlainText())
            preview.setVisible(False)  # Hidden by default
            splitter.addWidget(preview)
            
            # Connect text changes to preview
            editor.textChanged.connect(lambda: preview.update_preview(editor.toPlainText()))
            
            # Store references
            splitter.editor = editor
            splitter.preview = preview
            
            tab_widget = splitter
        else:
            # Regular code editor
            editor = CodeEditor()
            editor.load_file(file_path)
            tab_widget = editor
        
        # Add tab
        file_name = Path(file_path).name
        tab_index = self.script_tabs.addTab(tab_widget, file_name)
        self.script_tabs.setCurrentIndex(tab_index)
        
        # Connect modified signal to update tab title
        if isinstance(tab_widget, QSplitter):
            tab_widget.editor.modified_changed.connect(lambda modified: self._update_tab_title(tab_index))
        else:
            tab_widget.modified_changed.connect(lambda modified: self._update_tab_title(tab_index))
        
        # Track open script
        self.open_scripts[file_path] = tab_widget
        if file_path not in self.script_list:
            self.script_list.append(file_path)
            self._update_script_list()
    
    def _new_script(self):
        """Create a new script"""
        # Determine default location (project/scripts)
        default_location = None
        if self.project_root:
            scripts_dir = Path(self.project_root) / "scripts"
            scripts_dir.mkdir(exist_ok=True)
            default_location = scripts_dir
        
        dialog = NewScriptDialog(self, default_location)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            info = dialog.get_script_info()
            
            # Build file path
            location = Path(info['location']) if info['location'] else Path.cwd()
            default_name = info['name'] if info['name'] else 'new_script'
            file_path = location / (default_name + info['extension'])
            
            # Create class files if requested
            if info['create_class']:
                class_name = info['name'] if info['name'] else file_path.stem
                
                if info['extension'] in ['.cpp', '.c']:
                    # Create header and source
                    header_ext = '.h' if info['extension'] == '.c' else '.hpp'
                    header_path = file_path.with_suffix(header_ext)
                    source_path = file_path.with_suffix(info['extension'])
                    
                    # Generate header content
                    header_guard = f"{class_name.upper()}_H"
                    header_content = f"#ifndef {header_guard}\n#define {header_guard}\n\n"
                    header_content += f"class {class_name} {{\n"
                    header_content += f"public:\n"
                    header_content += f"    {class_name}();\n"
                    header_content += f"    ~{class_name}();\n\n"
                    header_content += f"private:\n"
                    header_content += f"    // Private members\n"
                    header_content += f"}};\n\n"
                    header_content += f"#endif // {header_guard}\n"
                    
                    # Generate source content
                    source_content = f"#include \"{header_path.name}\"\n\n"
                    source_content += f"{class_name}::{class_name}() {{\n"
                    source_content += f"    // Constructor\n"
                    source_content += f"}}\n\n"
                    source_content += f"{class_name}::~{class_name}() {{\n"
                    source_content += f"    // Destructor\n"
                    source_content += f"}}\n"
                    
                    # Write files
                    try:
                        header_path.write_text(header_content, encoding='utf-8')
                        source_path.write_text(source_content, encoding='utf-8')
                        
                        # Open both files
                        self.open_script(str(header_path))
                        self.open_script(str(source_path))
                        
                        QMessageBox.information(self, "Success", f"Created class files:\n{header_path.name}\n{source_path.name}")
                    except Exception as e:
                        QMessageBox.warning(self, "Error", f"Failed to create class files:\n{str(e)}")
                    
                    return
            
            # Single file creation
            try:
                # Check if file already exists
                if file_path.exists():
                    reply = QMessageBox.question(
                        self,
                        "File Exists",
                        f"File '{file_path.name}' already exists. Overwrite?",
                        QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
                    )
                    if reply == QMessageBox.StandardButton.No:
                        return
                
                # Get template content
                template = self._get_template_content(info['extension'], info['name'] or file_path.stem)
                file_path.write_text(template, encoding='utf-8')
                
                # Open the new file
                self.open_script(str(file_path))
            except Exception as e:
                QMessageBox.warning(self, "Error", f"Failed to create script:\n{str(e)}")
    
    def _open_script(self):
        """Open a script file"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Open Script",
            "",
            "All Files (*.*);;Python (*.py);;Lua (*.lua);;mRuby (*.rb);;C++ (*.cpp *.h *.hpp);;JSON (*.json);;Markdown (*.md)"
        )
        
        if file_path:
            self.open_script(file_path)
    
    def _save_current_script(self):
        """Save the currently active script"""
        current_widget = self.script_tabs.currentWidget()
        if not current_widget:
            return
        
        # Get the editor
        if isinstance(current_widget, QSplitter):
            editor = current_widget.editor
        else:
            editor = current_widget
        
        if isinstance(editor, CodeEditor):
            editor.save_file()
    
    def _save_all_scripts(self):
        """Save all open scripts"""
        for file_path, widget in self.open_scripts.items():
            if isinstance(widget, QSplitter):
                editor = widget.editor
            else:
                editor = widget
            
            if isinstance(editor, CodeEditor) and editor.is_modified:
                editor.save_file()
    
    def _close_current_script(self):
        """Close the currently active script"""
        current_index = self.script_tabs.currentIndex()
        if current_index >= 0:
            self._close_script_tab(current_index)
    
    def _close_script_tab(self, index):
        """Close a script tab"""
        widget = self.script_tabs.widget(index)
        if not widget:
            return
        
        # Get the editor
        if isinstance(widget, QSplitter):
            editor = widget.editor
        else:
            editor = widget
        
        # Check if modified
        if isinstance(editor, CodeEditor) and editor.is_modified:
            reply = QMessageBox.question(
                self,
                "Unsaved Changes",
                f"'{Path(editor.file_path).name}' has unsaved changes. Save before closing?",
                QMessageBox.StandardButton.Save | QMessageBox.StandardButton.Discard | QMessageBox.StandardButton.Cancel
            )
            
            if reply == QMessageBox.StandardButton.Save:
                editor.save_file()
            elif reply == QMessageBox.StandardButton.Cancel:
                return
        
        # Remove from tracking
        if isinstance(editor, CodeEditor) and editor.file_path in self.open_scripts:
            del self.open_scripts[editor.file_path]
            if editor.file_path in self.script_list:
                self.script_list.remove(editor.file_path)
        
        # Remove tab
        self.script_tabs.removeTab(index)
        self._update_script_list()
    
    def _on_tab_changed(self, index):
        """Handle tab change"""
        if index < 0:
            self.preview_toggle.setVisible(False)
            return
        
        widget = self.script_tabs.widget(index)
        
        # Show preview toggle for markdown files
        is_markdown_splitter = isinstance(widget, QSplitter) and hasattr(widget, 'preview')
        self.preview_toggle.setVisible(is_markdown_splitter)
        
        if is_markdown_splitter:
            self.preview_toggle.setChecked(widget.preview.isVisible())
    
    def _toggle_markdown_preview(self, checked):
        """Toggle markdown preview"""
        current_widget = self.script_tabs.currentWidget()
        if isinstance(current_widget, QSplitter) and hasattr(current_widget, 'preview'):
            current_widget.preview.setVisible(checked)
    
    def _update_tab_title(self, index):
        """Update tab title to reflect save state"""
        widget = self.script_tabs.widget(index)
        if isinstance(widget, QSplitter):
            editor = widget.editor
        else:
            editor = widget
        
        if isinstance(editor, CodeEditor) and editor.file_path:
            file_name = Path(editor.file_path).name
            if editor.is_modified:
                self.script_tabs.setTabText(index, f"*{file_name}")
            else:
                self.script_tabs.setTabText(index, file_name)
    
    def _update_script_list(self):
        """Update the script list widget"""
        self.script_list_widget.clear()
        
        for script_path in self.script_list:
            item = QListWidgetItem(Path(script_path).name)
            item.setData(Qt.ItemDataRole.UserRole, script_path)
            self.script_list_widget.addItem(item)
    
    def _on_script_selected(self, item):
        """Handle script selection from list"""
        script_path = item.data(Qt.ItemDataRole.UserRole)
        if script_path in self.open_scripts:
            widget = self.open_scripts[script_path]
            for i in range(self.script_tabs.count()):
                if self.script_tabs.widget(i) == widget:
                    self.script_tabs.setCurrentIndex(i)
                    break
    
    def _sort_scripts(self, sort_mode):
        """Sort the script list"""
        if sort_mode == "Alphabetical":
            self.script_list.sort(key=lambda p: Path(p).name.lower())
        elif sort_mode == "Recent":
            # Reverse order (most recent first) - would need actual tracking
            self.script_list.reverse()
        elif sort_mode == "Language":
            self.script_list.sort(key=lambda p: Path(p).suffix)
        elif sort_mode == "Inheritance":
            # Placeholder - would need actual implementation
            pass
        
        self._update_script_list()
    
    def _get_template_content(self, extension: str, name: str) -> str:
        """Get template content for a new script"""
        templates = {
            '.py': f'"""\n{name}\nPython script\n"""\n\n',
            '.lua': f'-- {name}\n-- Lua script\n\n',
            '.rb': f'# {name}\n# mRuby script\n\n',
            '.cpp': f'// {name}.cpp\n\n',
            '.c': f'// {name}.c\n\n',
            '.h': f'#ifndef {name.upper()}_H\n#define {name.upper()}_H\n\n#endif // {name.upper()}_H\n',
            '.hpp': f'#ifndef {name.upper()}_HPP\n#define {name.upper()}_HPP\n\n#endif // {name.upper()}_HPP\n',
            '.json': '{\n  \n}\n',
            '.md': f'# {name}\n\n',
            '.glsl': '// Shader\n\n',
        }
        return templates.get(extension, '')
