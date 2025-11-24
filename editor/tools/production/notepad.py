"""
Notepad Tool
A comprehensive note-taking and code editing tool with syntax highlighting
Supports multiple file types with persistent sessions
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                             QPushButton, QTextEdit, QComboBox, QTabWidget,
                             QSplitter, QFileDialog, QMessageBox, QDialog,
                             QFormLayout, QCheckBox, QDialogButtonBox, QListWidget,
                             QListWidgetItem, QLineEdit, QToolButton, QMenu)
from PyQt6.QtCore import Qt, pyqtSignal, QRegularExpression
from PyQt6.QtGui import (QFont, QSyntaxHighlighter, QTextCharFormat, QColor,
                        QFontMetrics, QPalette, QAction)
from pathlib import Path
import json
import sys
import os

# Import base panel
editor_path = Path(__file__).parent.parent.parent
if str(editor_path) not in sys.path:
    sys.path.insert(0, str(editor_path))

from panels.base_panel import EditorPanel
import markdown


class SyntaxHighlighter(QSyntaxHighlighter):
    """Syntax highlighter with support for multiple languages"""

    def __init__(self, document, language="text"):
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

        elif self.language in ["ruby", "mruby"]:
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
            self.highlighting_rules.append((QRegularExpression('@@[a-zA-Z_]\\w*'), instance_var_format))
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

        # Numbers (common to all)
        self.highlighting_rules.append((QRegularExpression('\\b[0-9]+\\.?[0-9]*\\b'), number_format))

    def highlightBlock(self, text):
        """Apply syntax highlighting to a block of text"""
        for pattern, format in self.highlighting_rules:
            match_iterator = pattern.globalMatch(text)
            while match_iterator.hasNext():
                match = match_iterator.next()
                self.setFormat(match.capturedStart(), match.capturedLength(), format)


class NoteEditor(QTextEdit):
    """Note editor widget with syntax highlighting"""

    modified_changed = pyqtSignal(bool)

    def __init__(self, parent=None, language="text"):
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

        # Track modifications
        self.textChanged.connect(self._on_text_changed)

    def _on_text_changed(self):
        """Track when text is modified"""
        if not self.is_modified:
            self.is_modified = True
            self.modified_changed.emit(True)

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

            # Set language based on file extension
            ext = Path(file_path).suffix.lower()
            lang_map = {
                '.py': 'python',
                '.lua': 'lua',
                '.rb': 'ruby',
                '.cpp': 'cpp', '.cc': 'cpp', '.cxx': 'cpp',
                '.c': 'c',
                '.h': 'cpp', '.hpp': 'cpp', '.hxx': 'cpp',
                '.md': 'markdown',
                '.txt': 'text'
            }
            if ext in lang_map:
                self.set_language(lang_map[ext])

            return True
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to load file:\n{str(e)}")
            return False

    def save_file(self, file_path=None):
        """Save the current file"""
        if file_path:
            self.file_path = file_path

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


class NewNoteDialog(QDialog):
    """Dialog for creating a new note"""

    def __init__(self, parent=None, default_location=None):
        super().__init__(parent)
        self.default_location = default_location
        self.setWindowTitle("Create New Note")
        self.setModal(True)
        self.setMinimumWidth(500)

        layout = QVBoxLayout()

        # Form layout
        form_layout = QFormLayout()

        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("note_name")
        form_layout.addRow("Name:", self.name_edit)

        self.type_combo = QComboBox()
        self.type_combo.addItems([
            "Text File (.txt)",
            "Markdown (.md)",
            "Python Script (.py)",
            "Lua Script (.lua)",
            "mRuby Script (.rb)",
            "C++ Source (.cpp)",
            "C++ Header (.hpp)",
            "C Header (.h)",
            "C Source (.c)"
        ])
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

        layout.addLayout(form_layout)

        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self.setLayout(layout)

    def _browse_location(self):
        """Browse for save location"""
        directory = QFileDialog.getExistingDirectory(
            self,
            "Select Save Location",
            self.location_edit.text() if self.location_edit.text() else str(Path.home())
        )

        if directory:
            self.location_edit.setText(directory)

    def get_note_info(self):
        """Get note creation info"""
        name = self.name_edit.text()
        type_text = self.type_combo.currentText()
        extension = type_text.split('(')[1].split(')')[0]
        location = self.location_edit.text()

        return {
            'name': name,
            'extension': extension,
            'location': location
        }


class NotepadTool(EditorPanel):
    """Notepad tool for taking notes and editing various file types"""

    def __init__(self, parent=None, project_root=None):
        self.project_root = project_root
        self.open_notes = {}  # path -> editor widget
        self.note_list = []  # List of note paths
        self.session_file = None

        super().__init__("Notepad", parent)
        self.setObjectName("NotepadTool")

        # Load session if project root is set
        if self.project_root:
            self._load_session()

    def _setup_panel(self):
        """Setup notepad panel UI"""
        # Main layout
        main_widget = QWidget()
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 0, 0, 0)

        # Toolbar
        toolbar = QHBoxLayout()
        toolbar.setContentsMargins(5, 5, 5, 5)

        new_btn = QPushButton("New")
        new_btn.clicked.connect(self._new_note)
        toolbar.addWidget(new_btn)

        open_btn = QPushButton("Open")
        open_btn.clicked.connect(self._open_note)
        toolbar.addWidget(open_btn)

        save_btn = QPushButton("Save")
        save_btn.clicked.connect(self._save_current_note)
        toolbar.addWidget(save_btn)

        save_as_btn = QPushButton("Save As...")
        save_as_btn.clicked.connect(self._save_as_note)
        toolbar.addWidget(save_as_btn)

        save_all_btn = QPushButton("Save All")
        save_all_btn.clicked.connect(self._save_all_notes)
        toolbar.addWidget(save_all_btn)

        close_btn = QPushButton("Close")
        close_btn.clicked.connect(self._close_current_note)
        toolbar.addWidget(close_btn)

        toolbar.addStretch()

        # Markdown preview controls
        self.split_preview_btn = QPushButton("Split Preview")
        self.split_preview_btn.setCheckable(True)
        self.split_preview_btn.setVisible(False)
        self.split_preview_btn.toggled.connect(self._toggle_split_preview)
        toolbar.addWidget(self.split_preview_btn)

        self.full_preview_btn = QPushButton("Full Preview")
        self.full_preview_btn.setCheckable(True)
        self.full_preview_btn.setVisible(False)
        self.full_preview_btn.toggled.connect(self._toggle_full_preview)
        toolbar.addWidget(self.full_preview_btn)

        main_layout.addLayout(toolbar)

        # Tab widget for notes
        self.note_tabs = QTabWidget()
        self.note_tabs.setTabsClosable(True)
        self.note_tabs.setMovable(True)
        self.note_tabs.tabCloseRequested.connect(self._close_note_tab)
        self.note_tabs.currentChanged.connect(self._on_tab_changed)
        main_layout.addWidget(self.note_tabs)

        main_widget.setLayout(main_layout)
        self.content_layout.addWidget(main_widget)

    def _new_note(self):
        """Create a new note"""
        # Determine default location
        default_location = None
        if self.project_root:
            notes_dir = Path(self.project_root) / "production" / "notes"
            notes_dir.mkdir(parents=True, exist_ok=True)
            default_location = notes_dir

        dialog = NewNoteDialog(self, default_location)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            info = dialog.get_note_info()

            # Build file path
            location = Path(info['location']) if info['location'] else Path.cwd()
            default_name = info['name'] if info['name'] else 'new_note'
            file_path = location / (default_name + info['extension'])

            try:
                # Check if file exists
                if file_path.exists():
                    reply = QMessageBox.question(
                        self,
                        "File Exists",
                        f"File '{file_path.name}' already exists. Overwrite?",
                        QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
                    )
                    if reply == QMessageBox.StandardButton.No:
                        return

                # Create file with template content
                template = self._get_template_content(info['extension'], default_name)
                file_path.write_text(template, encoding='utf-8')

                # Open the new file
                self.open_note(str(file_path))
            except Exception as e:
                QMessageBox.warning(self, "Error", f"Failed to create note:\n{str(e)}")

    def _open_note(self):
        """Open a note file"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Open Note",
            str(self._get_notes_directory()) if self.project_root else "",
            "All Files (*.*);;Text (*.txt);;Markdown (*.md);;Python (*.py);;Lua (*.lua);;mRuby (*.rb);;C++ (*.cpp *.h *.hpp);;C (*.c *.h)"
        )

        if file_path:
            self.open_note(file_path)

    def open_note(self, file_path):
        """Open a note file in the editor"""
        file_path = str(Path(file_path).resolve())

        # Check if already open
        if file_path in self.open_notes:
            # Switch to existing tab
            for i in range(self.note_tabs.count()):
                widget = self.note_tabs.widget(i)
                if isinstance(widget, QSplitter):
                    editor = widget.editor
                else:
                    editor = widget
                if editor.file_path == file_path:
                    self.note_tabs.setCurrentIndex(i)
                    return

        # Determine if markdown
        is_markdown = file_path.endswith('.md')

        if is_markdown:
            # Create splitter for markdown with preview
            splitter = QSplitter(Qt.Orientation.Horizontal)

            editor = NoteEditor(language="markdown")
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
            # Regular editor
            editor = NoteEditor()
            editor.load_file(file_path)
            tab_widget = editor

        # Add tab
        file_name = Path(file_path).name
        tab_index = self.note_tabs.addTab(tab_widget, file_name)
        self.note_tabs.setCurrentIndex(tab_index)

        # Connect modified signal
        if isinstance(tab_widget, QSplitter):
            tab_widget.editor.modified_changed.connect(lambda modified: self._update_tab_title(tab_index))
        else:
            tab_widget.modified_changed.connect(lambda modified: self._update_tab_title(tab_index))

        # Track open note
        self.open_notes[file_path] = tab_widget
        if file_path not in self.note_list:
            self.note_list.append(file_path)

        # Save session
        self._save_session()

    def _save_current_note(self):
        """Save the currently active note"""
        current_widget = self.note_tabs.currentWidget()
        if not current_widget:
            return

        # Get the editor
        if isinstance(current_widget, QSplitter):
            editor = current_widget.editor
        else:
            editor = current_widget

        if isinstance(editor, NoteEditor):
            editor.save_file()

    def _save_as_note(self):
        """Save current note with a new name"""
        current_widget = self.note_tabs.currentWidget()
        if not current_widget:
            return

        # Get the editor
        if isinstance(current_widget, QSplitter):
            editor = current_widget.editor
        else:
            editor = current_widget

        if not isinstance(editor, NoteEditor):
            return

        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Save Note As",
            str(self._get_notes_directory()) if self.project_root else "",
            "All Files (*.*)"
        )

        if file_path:
            # Remove old path from tracking
            if editor.file_path in self.open_notes:
                del self.open_notes[editor.file_path]
                if editor.file_path in self.note_list:
                    self.note_list.remove(editor.file_path)

            # Save with new path
            if editor.save_file(file_path):
                # Update tracking
                self.open_notes[file_path] = current_widget
                self.note_list.append(file_path)

                # Update tab title
                tab_index = self.note_tabs.currentIndex()
                self.note_tabs.setTabText(tab_index, Path(file_path).name)

                self._save_session()

    def _save_all_notes(self):
        """Save all open notes"""
        for file_path, widget in self.open_notes.items():
            if isinstance(widget, QSplitter):
                editor = widget.editor
            else:
                editor = widget

            if isinstance(editor, NoteEditor) and editor.is_modified:
                editor.save_file()

    def _close_current_note(self):
        """Close the currently active note"""
        current_index = self.note_tabs.currentIndex()
        if current_index >= 0:
            self._close_note_tab(current_index)

    def _close_note_tab(self, index):
        """Close a note tab"""
        widget = self.note_tabs.widget(index)
        if not widget:
            return

        # Get the editor
        if isinstance(widget, QSplitter):
            editor = widget.editor
        else:
            editor = widget

        # Check if modified
        if isinstance(editor, NoteEditor) and editor.is_modified:
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
        if isinstance(editor, NoteEditor) and editor.file_path in self.open_notes:
            del self.open_notes[editor.file_path]
            if editor.file_path in self.note_list:
                self.note_list.remove(editor.file_path)

        # Remove tab
        self.note_tabs.removeTab(index)

        # Save session
        self._save_session()

    def _on_tab_changed(self, index):
        """Handle tab change"""
        if index < 0:
            self.split_preview_btn.setVisible(False)
            self.full_preview_btn.setVisible(False)
            return

        widget = self.note_tabs.widget(index)

        # Show preview buttons for markdown files
        is_markdown_splitter = isinstance(widget, QSplitter) and hasattr(widget, 'preview')
        self.split_preview_btn.setVisible(is_markdown_splitter)
        self.full_preview_btn.setVisible(is_markdown_splitter)

        if is_markdown_splitter:
            # Update button states
            preview_visible = widget.preview.isVisible()
            editor_visible = widget.editor.isVisible()

            self.split_preview_btn.setChecked(preview_visible and editor_visible)
            self.full_preview_btn.setChecked(preview_visible and not editor_visible)

    def _toggle_split_preview(self, checked):
        """Toggle split-screen markdown preview"""
        current_widget = self.note_tabs.currentWidget()
        if isinstance(current_widget, QSplitter) and hasattr(current_widget, 'preview'):
            if checked:
                current_widget.editor.setVisible(True)
                current_widget.preview.setVisible(True)
                self.full_preview_btn.setChecked(False)
            else:
                current_widget.preview.setVisible(False)

    def _toggle_full_preview(self, checked):
        """Toggle full-screen markdown preview"""
        current_widget = self.note_tabs.currentWidget()
        if isinstance(current_widget, QSplitter) and hasattr(current_widget, 'preview'):
            if checked:
                current_widget.editor.setVisible(False)
                current_widget.preview.setVisible(True)
                self.split_preview_btn.setChecked(False)
            else:
                current_widget.editor.setVisible(True)
                current_widget.preview.setVisible(False)

    def _update_tab_title(self, index):
        """Update tab title to reflect save state"""
        widget = self.note_tabs.widget(index)
        if isinstance(widget, QSplitter):
            editor = widget.editor
        else:
            editor = widget

        if isinstance(editor, NoteEditor) and editor.file_path:
            file_name = Path(editor.file_path).name
            if editor.is_modified:
                self.note_tabs.setTabText(index, f"*{file_name}")
            else:
                self.note_tabs.setTabText(index, file_name)

    def _get_notes_directory(self):
        """Get the notes directory path"""
        if self.project_root:
            notes_dir = Path(self.project_root) / "production" / "notes"
            notes_dir.mkdir(parents=True, exist_ok=True)
            return notes_dir
        return Path.home()

    def _get_template_content(self, extension: str, name: str) -> str:
        """Get template content for a new note"""
        templates = {
            '.txt': '',
            '.md': f'# {name}\n\n',
            '.py': f'"""\n{name}\nPython script\n"""\n\n',
            '.lua': f'-- {name}\n-- Lua script\n\n',
            '.rb': f'# {name}\n# mRuby script\n\n',
            '.cpp': f'// {name}.cpp\n\n',
            '.c': f'// {name}.c\n\n',
            '.h': f'#ifndef {name.upper()}_H\n#define {name.upper()}_H\n\n#endif // {name.upper()}_H\n',
            '.hpp': f'#ifndef {name.upper()}_HPP\n#define {name.upper()}_HPP\n\n#endif // {name.upper()}_HPP\n',
        }
        return templates.get(extension, '')

    def _save_session(self):
        """Save the current session (open notes)"""
        if not self.project_root:
            return

        session_data = {
            'open_notes': list(self.open_notes.keys())
        }

        try:
            session_file = Path(self.project_root) / "production" / "notepad_session.json"
            session_file.parent.mkdir(parents=True, exist_ok=True)

            with open(session_file, 'w', encoding='utf-8') as f:
                json.dump(session_data, f, indent=2)
        except Exception as e:
            print(f"Failed to save notepad session: {e}")

    def _load_session(self):
        """Load the previous session (open notes)"""
        if not self.project_root:
            return

        try:
            session_file = Path(self.project_root) / "production" / "notepad_session.json"

            if session_file.exists():
                with open(session_file, 'r', encoding='utf-8') as f:
                    session_data = json.load(f)

                # Reopen notes from previous session
                for note_path in session_data.get('open_notes', []):
                    if Path(note_path).exists():
                        self.open_note(note_path)
        except Exception as e:
            print(f"Failed to load notepad session: {e}")

    def get_state(self) -> dict:
        """Get panel state for serialization"""
        state = super().get_state()
        state['open_notes'] = list(self.open_notes.keys())
        return state

    def set_state(self, state: dict):
        """Restore panel state"""
        super().set_state(state)

        # Reopen notes
        for note_path in state.get('open_notes', []):
            if Path(note_path).exists():
                self.open_note(note_path)
