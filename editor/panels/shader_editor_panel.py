"""
Shader Editor Panel
Edit .lsh (Lupine Shader) files with syntax highlighting and validation
"""

import sys
from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                             QPushButton, QTextEdit, QTabWidget, QPlainTextEdit,
                             QSplitter, QFileDialog, QMessageBox, QFrame,
                             QSizePolicy)
from PyQt6.QtCore import Qt, pyqtSignal, QRegularExpression, QRect, QSize
from PyQt6.QtGui import (QFont, QSyntaxHighlighter, QTextCharFormat, QColor,
                         QFontMetrics, QPainter, QTextCursor, QPen,
                         QKeySequence, QShortcut, QTextFormat)
from pathlib import Path
from .base_panel import EditorPanel
from editor.theme import get_theme_manager


class LshSyntaxHighlighter(QSyntaxHighlighter):
    """Syntax highlighter for .lsh (Lupine Shader) files"""

    def __init__(self, document):
        super().__init__(document)
        self.highlighting_rules = []
        self._multiline_comment_start = QRegularExpression(r'/\*')
        self._multiline_comment_end = QRegularExpression(r'\*/')
        self._setup_rules()

    def _setup_rules(self):
        """Setup syntax highlighting rules for .lsh format"""
        self.highlighting_rules = []

        # Get theme colors for consistent styling
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme() if theme_manager else None

        # -- Directive format: #shader, #description, #begin, #end, #input, #output --
        directive_format = QTextCharFormat()
        if theme:
            directive_format.setForeground(QColor(theme.colors.accent_color))
        else:
            directive_format.setForeground(QColor("#C586C0"))
        directive_format.setFontWeight(QFont.Weight.Bold)

        directives = [
            r'#shader\b', r'#description\b', r'#begin\b', r'#end\b',
            r'#input\b', r'#output\b'
        ]
        for d in directives:
            self.highlighting_rules.append((QRegularExpression(d), directive_format))

        # -- Keyword format: types and control flow --
        keyword_format = QTextCharFormat()
        keyword_format.setForeground(QColor("#C586C0"))
        keyword_format.setFontWeight(QFont.Weight.Bold)

        keywords = [
            'void', 'float', 'int', 'bool', 'vec2', 'vec3', 'vec4',
            'mat3', 'mat4', 'sampler2D', 'samplerCube',
            'uniform', 'const', 'if', 'else', 'for', 'while',
            'return', 'struct'
        ]
        for word in keywords:
            pattern = QRegularExpression(f'\\b{word}\\b')
            self.highlighting_rules.append((pattern, keyword_format))

        # -- Macro format: VERTEX_OUTPUT, SAMPLE, MUL, etc. --
        macro_format = QTextCharFormat()
        macro_format.setForeground(QColor("#4EC9B0"))
        macro_format.setFontWeight(QFont.Weight.Bold)

        macros = [
            'VERTEX_OUTPUT', 'SAMPLE', 'SAMPLE_LOD', 'SAMPLE_CUBE',
            'MUL', 'MAT3', 'DISCARD', 'FRAG_COORD', 'INSTANCE_ID', 'VERTEX_ID'
        ]
        for macro in macros:
            pattern = QRegularExpression(f'\\b{macro}\\b')
            self.highlighting_rules.append((pattern, macro_format))

        # -- Built-in function format --
        builtin_format = QTextCharFormat()
        builtin_format.setForeground(QColor("#DCDCAA"))

        builtins = [
            'normalize', 'dot', 'cross', 'length', 'mix', 'clamp',
            'max', 'min', 'pow', 'sqrt', 'abs', 'sin', 'cos', 'tan',
            'atan', 'smoothstep', 'step', 'fract', 'mod', 'floor', 'ceil',
            'texture', 'dFdx', 'dFdy'
        ]
        for fn in builtins:
            pattern = QRegularExpression(f'\\b{fn}\\b')
            self.highlighting_rules.append((pattern, builtin_format))

        # -- Section names after #begin / #end --
        section_format = QTextCharFormat()
        section_format.setForeground(QColor("#4FC1FF"))
        section_format.setFontWeight(QFont.Weight.Bold)
        self.highlighting_rules.append(
            (QRegularExpression(r'#(?:begin|end)\s+(vertex|fragment|properties|shared)'),
             section_format)
        )

        # -- Semantic labels in #input: POSITION, NORMAL, TEXCOORD0, COLOR --
        semantic_format = QTextCharFormat()
        semantic_format.setForeground(QColor("#9CDCFE"))
        self.highlighting_rules.append(
            (QRegularExpression(r'\b(POSITION|NORMAL|TEXCOORD\d+|COLOR)\b'), semantic_format)
        )

        # -- Number format --
        number_format = QTextCharFormat()
        number_format.setForeground(QColor("#B5CEA8"))
        self.highlighting_rules.append(
            (QRegularExpression(r'\b[0-9]+\.?[0-9]*[fF]?\b'), number_format)
        )

        # -- String format (for #shader "Name" and #description "...") --
        string_format = QTextCharFormat()
        string_format.setForeground(QColor("#CE9178"))
        self.highlighting_rules.append(
            (QRegularExpression(r'"[^"\\]*(\\.[^"\\]*)*"'), string_format)
        )

        # -- Single-line comment format --
        self._comment_format = QTextCharFormat()
        self._comment_format.setForeground(QColor("#6A9955"))
        self._comment_format.setFontItalic(True)
        self.highlighting_rules.append(
            (QRegularExpression(r'//[^\n]*'), self._comment_format)
        )

        # Multiline comment format (handled separately in highlightBlock)
        self._multiline_comment_format = QTextCharFormat()
        self._multiline_comment_format.setForeground(QColor("#6A9955"))
        self._multiline_comment_format.setFontItalic(True)

    def highlightBlock(self, text):
        """Apply syntax highlighting to a block of text"""
        # Apply single-line rules
        for pattern, fmt in self.highlighting_rules:
            match_iterator = pattern.globalMatch(text)
            while match_iterator.hasNext():
                match = match_iterator.next()
                self.setFormat(match.capturedStart(), match.capturedLength(), fmt)

        # Handle multi-line /* */ comments
        self.setCurrentBlockState(0)

        start_index = 0
        if self.previousBlockState() != 1:
            match = self._multiline_comment_start.match(text)
            start_index = match.capturedStart() if match.hasMatch() else -1
        else:
            start_index = 0

        while start_index >= 0:
            end_match = self._multiline_comment_end.match(text, start_index)
            if end_match.hasMatch():
                comment_length = end_match.capturedEnd() - start_index
                self.setFormat(start_index, comment_length, self._multiline_comment_format)
                next_match = self._multiline_comment_start.match(text, start_index + comment_length)
                start_index = next_match.capturedStart() if next_match.hasMatch() else -1
            else:
                self.setCurrentBlockState(1)
                comment_length = len(text) - start_index
                self.setFormat(start_index, comment_length, self._multiline_comment_format)
                break


class LineNumberWidget(QWidget):
    """Line number gutter widget for the shader code editor"""

    def __init__(self, editor):
        super().__init__(editor)
        self._editor = editor

    def sizeHint(self):
        return QSize(self._editor.line_number_area_width(), 0)

    def paintEvent(self, event):
        self._editor.line_number_area_paint_event(event)


class ShaderCodeEditor(QPlainTextEdit):
    """Code editor widget with line numbers and .lsh syntax highlighting"""

    modified_changed = pyqtSignal(bool)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.file_path = None
        self.is_modified = False

        # Setup font
        font = QFont("Consolas", 10)
        font.setStyleHint(QFont.StyleHint.Monospace)
        self.setFont(font)

        # Setup tab width
        metrics = QFontMetrics(font)
        self.setTabStopDistance(4 * metrics.horizontalAdvance(' '))

        # Line wrapping off for shader code
        self.setLineWrapMode(QPlainTextEdit.LineWrapMode.NoWrap)

        # Setup syntax highlighter
        self.highlighter = LshSyntaxHighlighter(self.document())

        # Line number area
        self._line_number_area = LineNumberWidget(self)

        self.blockCountChanged.connect(self._update_line_number_area_width)
        self.updateRequest.connect(self._update_line_number_area)
        self.cursorPositionChanged.connect(self._highlight_current_line)

        self._update_line_number_area_width(0)
        self._highlight_current_line()

        # Track modifications
        self.textChanged.connect(self._on_text_changed)

        # Apply theme styling
        self._apply_theme()

    def _apply_theme(self):
        """Apply theme colors to the editor"""
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme() if theme_manager else None

        if theme:
            c = theme.colors
            self.setStyleSheet(f"""
                QPlainTextEdit {{
                    background-color: {c.surface};
                    color: {c.text_primary};
                    border: 1px solid {c.border};
                    selection-background-color: {c.selection};
                }}
            """)
        else:
            self.setStyleSheet("""
                QPlainTextEdit {
                    background-color: #1e1e1e;
                    color: #d4d4d4;
                    border: 1px solid #3d3d3d;
                    selection-background-color: #264f78;
                }
            """)

    def line_number_area_width(self) -> int:
        """Calculate the width needed for line numbers"""
        digits = 1
        max_num = max(1, self.blockCount())
        while max_num >= 10:
            max_num //= 10
            digits += 1
        digits = max(digits, 3)  # Minimum 3 digits wide

        space = 3 + self.fontMetrics().horizontalAdvance('9') * digits + 10
        return space

    def _update_line_number_area_width(self, _):
        """Update viewport margins to make room for line numbers"""
        self.setViewportMargins(self.line_number_area_width(), 0, 0, 0)

    def _update_line_number_area(self, rect, dy):
        """Update line number area when scrolling or resizing"""
        if dy:
            self._line_number_area.scroll(0, dy)
        else:
            self._line_number_area.update(0, rect.y(),
                                          self._line_number_area.width(), rect.height())

        if rect.contains(self.viewport().rect()):
            self._update_line_number_area_width(0)

    def resizeEvent(self, event):
        super().resizeEvent(event)
        cr = self.contentsRect()
        self._line_number_area.setGeometry(
            QRect(cr.left(), cr.top(), self.line_number_area_width(), cr.height())
        )

    def _highlight_current_line(self):
        """Highlight the current line"""
        extra_selections = []

        if not self.isReadOnly():
            selection = QTextEdit.ExtraSelection()

            theme_manager = get_theme_manager()
            theme = theme_manager.get_current_theme() if theme_manager else None

            if theme:
                line_color = QColor(theme.colors.surface_hover)
            else:
                line_color = QColor("#2a2a2a")

            selection.format.setBackground(line_color)
            selection.format.setProperty(QTextFormat.Property.FullWidthSelection, True)
            selection.cursor = self.textCursor()
            selection.cursor.clearSelection()
            extra_selections.append(selection)

        self.setExtraSelections(extra_selections)

    def line_number_area_paint_event(self, event):
        """Paint the line numbers"""
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme() if theme_manager else None

        if theme:
            bg_color = QColor(theme.colors.secondary_color)
            text_color = QColor(theme.colors.text_secondary)
            current_line_color = QColor(theme.colors.text_primary)
        else:
            bg_color = QColor("#252526")
            text_color = QColor("#858585")
            current_line_color = QColor("#c6c6c6")

        painter = QPainter(self._line_number_area)
        painter.fillRect(event.rect(), bg_color)

        block = self.firstVisibleBlock()
        block_number = block.blockNumber()
        top = round(self.blockBoundingGeometry(block).translated(self.contentOffset()).top())
        bottom = top + round(self.blockBoundingRect(block).height())

        current_block = self.textCursor().blockNumber()

        while block.isValid() and top <= event.rect().bottom():
            if block.isVisible() and bottom >= event.rect().top():
                number = str(block_number + 1)

                if block_number == current_block:
                    painter.setPen(current_line_color)
                    font = painter.font()
                    font.setBold(True)
                    painter.setFont(font)
                else:
                    painter.setPen(text_color)
                    font = painter.font()
                    font.setBold(False)
                    painter.setFont(font)

                painter.drawText(0, top, self._line_number_area.width() - 5,
                                 self.fontMetrics().height(),
                                 Qt.AlignmentFlag.AlignRight, number)

            block = block.next()
            top = bottom
            bottom = top + round(self.blockBoundingRect(block).height())
            block_number += 1

        painter.end()

    def _on_text_changed(self):
        """Track when text is modified"""
        if not self.is_modified:
            self.is_modified = True
            self.modified_changed.emit(True)

    def load_file(self, file_path):
        """Load a .lsh file into the editor"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            self.setPlainText(content)
            self.file_path = str(file_path)
            self.is_modified = False
            self.modified_changed.emit(False)
            return True
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to load shader:\n{str(e)}")
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
            QMessageBox.warning(self, "Error", f"Failed to save shader:\n{str(e)}")
            return False

    def keyPressEvent(self, event):
        """Handle key press events for auto-indent and tab insertion"""
        if event.key() == Qt.Key.Key_Tab:
            self.insertPlainText("    ")
            event.accept()
            return
        super().keyPressEvent(event)


class ShaderEditorPanel(EditorPanel):
    """Shader editor panel for editing .lsh (Lupine Shader) files"""

    # Signals
    shader_saved = pyqtSignal(str)   # shader file path
    shader_closed = pyqtSignal(str)  # shader file path

    def __init__(self, parent=None):
        super().__init__("Shader Editor", parent)
        self.setObjectName("ShaderEditorPanel")
        self.open_shaders = {}   # path -> ShaderCodeEditor
        self.project_root = None  # Set by main editor

    def _setup_panel(self):
        """Setup shader editor panel UI"""
        main_widget = QWidget()
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)

        # -- Toolbar --
        toolbar = QHBoxLayout()
        toolbar.setContentsMargins(5, 5, 5, 5)
        toolbar.setSpacing(4)

        new_btn = QPushButton("New Shader")
        new_btn.clicked.connect(self.new_shader)
        toolbar.addWidget(new_btn)

        open_btn = QPushButton("Open")
        open_btn.setProperty("secondary", True)
        open_btn.clicked.connect(lambda: self.open_shader())
        toolbar.addWidget(open_btn)

        save_btn = QPushButton("Save")
        save_btn.setProperty("secondary", True)
        save_btn.clicked.connect(self.save_shader)
        toolbar.addWidget(save_btn)

        save_as_btn = QPushButton("Save As")
        save_as_btn.setProperty("secondary", True)
        save_as_btn.clicked.connect(self.save_shader_as)
        toolbar.addWidget(save_as_btn)

        # Separator
        sep = QFrame()
        sep.setFrameShape(QFrame.Shape.VLine)
        sep.setFrameShadow(QFrame.Shadow.Sunken)
        sep.setFixedWidth(2)
        toolbar.addWidget(sep)

        validate_btn = QPushButton("Validate")
        validate_btn.setProperty("success", True)
        validate_btn.clicked.connect(self.validate_shader)
        toolbar.addWidget(validate_btn)

        toolbar.addStretch()

        # Status label in toolbar
        self._status_label = QLabel("")
        self._status_label.setStyleSheet("font-style: italic;")
        toolbar.addWidget(self._status_label)

        main_layout.addLayout(toolbar)

        # -- Main splitter: editor on top, error output on bottom --
        self._splitter = QSplitter(Qt.Orientation.Vertical)

        # Tab widget for multiple open shaders
        self._shader_tabs = QTabWidget()
        self._shader_tabs.setTabsClosable(True)
        self._shader_tabs.setMovable(True)
        self._shader_tabs.tabCloseRequested.connect(self._close_shader_tab)
        self._shader_tabs.currentChanged.connect(self._on_tab_changed)
        self._splitter.addWidget(self._shader_tabs)

        # Error output area (collapsible)
        self._error_widget = QWidget()
        error_layout = QVBoxLayout()
        error_layout.setContentsMargins(0, 0, 0, 0)
        error_layout.setSpacing(0)

        error_header = QHBoxLayout()
        error_header.setContentsMargins(5, 2, 5, 2)
        error_label = QLabel("Output")
        error_label.setStyleSheet("font-weight: bold;")
        error_header.addWidget(error_label)
        error_header.addStretch()

        self._toggle_error_btn = QPushButton("Hide")
        self._toggle_error_btn.setProperty("secondary", True)
        self._toggle_error_btn.setMinimumWidth(60)
        self._toggle_error_btn.clicked.connect(self._toggle_error_output)
        error_header.addWidget(self._toggle_error_btn)

        clear_error_btn = QPushButton("Clear")
        clear_error_btn.setProperty("secondary", True)
        clear_error_btn.setMinimumWidth(60)
        clear_error_btn.clicked.connect(self._clear_error_output)
        error_header.addWidget(clear_error_btn)

        error_layout.addLayout(error_header)

        self._error_output = QPlainTextEdit()
        self._error_output.setReadOnly(True)
        self._error_output.setMinimumHeight(60)
        font = QFont("Consolas", 9)
        font.setStyleHint(QFont.StyleHint.Monospace)
        self._error_output.setFont(font)

        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme() if theme_manager else None
        if theme:
            c = theme.colors
            self._error_output.setStyleSheet(f"""
                QPlainTextEdit {{
                    background-color: {c.secondary_color};
                    color: {c.text_secondary};
                    border: 1px solid {c.border};
                }}
            """)

        error_layout.addWidget(self._error_output)
        self._error_widget.setLayout(error_layout)
        self._splitter.addWidget(self._error_widget)

        # Set splitter proportions (85% editor, 15% output)
        self._splitter.setStretchFactor(0, 6)
        self._splitter.setStretchFactor(1, 1)
        self._splitter.setSizes([600, 100])

        main_layout.addWidget(self._splitter)

        # -- Status bar --
        status_bar = QHBoxLayout()
        status_bar.setContentsMargins(5, 2, 5, 2)

        self._file_path_label = QLabel("No file open")
        self._file_path_label.setStyleSheet("font-size: 11px;")
        status_bar.addWidget(self._file_path_label)

        status_bar.addStretch()

        self._compile_status_label = QLabel("")
        self._compile_status_label.setStyleSheet("font-size: 11px;")
        status_bar.addWidget(self._compile_status_label)

        main_layout.addLayout(status_bar)

        main_widget.setLayout(main_layout)
        self.content_layout.addWidget(main_widget)

    # =========================================================================
    # Public API
    # =========================================================================

    def handle_save(self) -> bool:
        """Save the active shader when Ctrl+S is pressed with this panel focused"""
        self.save_shader()
        return True

    def handle_save_as(self) -> bool:
        """Save the active shader to a new path when Ctrl+Shift+S is pressed"""
        self.save_shader_as()
        return True

    def new_shader(self):
        """Open the new shader dialog to create a shader from a template"""
        from editor.dialogs.new_shader_dialog import NewShaderDialog

        default_loc = str(self.project_root) if self.project_root else ""
        dialog = NewShaderDialog(self, default_location=default_loc)

        if dialog.exec() == NewShaderDialog.DialogCode.Accepted:
            save_path = dialog.get_save_path()
            source = dialog.get_shader_source()

            # Ensure directory exists
            save_dir = Path(save_path).parent
            save_dir.mkdir(parents=True, exist_ok=True)

            # Write the file
            try:
                with open(save_path, 'w', encoding='utf-8') as f:
                    f.write(source)
            except Exception as e:
                QMessageBox.warning(self, "Error", f"Failed to create shader:\n{str(e)}")
                return

            # Open the newly created shader
            self.open_shader(save_path)
            self._log_output(f"Created new shader: {save_path}")

    def open_shader(self, path=None):
        """Open a .lsh file. Shows file dialog if no path provided."""
        if path is None:
            start_dir = str(self.project_root) if self.project_root else str(Path.home())
            path, _ = QFileDialog.getOpenFileName(
                self,
                "Open Lupine Shader",
                start_dir,
                "Lupine Shader Files (*.lsh);;All Files (*)"
            )
            if not path:
                return

        path = str(Path(path))

        # Check if already open
        if path in self.open_shaders:
            for i in range(self._shader_tabs.count()):
                if self._shader_tabs.widget(i) == self.open_shaders[path]:
                    self._shader_tabs.setCurrentIndex(i)
                    return

        # Create new editor
        editor = ShaderCodeEditor()
        if not editor.load_file(path):
            return

        editor.modified_changed.connect(lambda modified: self._on_editor_modified(editor, modified))

        # Add tab
        tab_name = Path(path).name
        index = self._shader_tabs.addTab(editor, tab_name)
        self._shader_tabs.setCurrentIndex(index)

        self.open_shaders[path] = editor
        self._update_status_bar()

    def save_shader(self):
        """Save the currently active shader"""
        editor = self._current_editor()
        if not editor:
            return

        if not editor.file_path:
            self.save_shader_as()
            return

        if editor.save_file():
            self._update_tab_title(editor)
            self._update_status_bar()
            self.shader_saved.emit(editor.file_path)
            self._log_output(f"Saved: {editor.file_path}")

    def save_shader_as(self):
        """Save the current shader with a new file name"""
        editor = self._current_editor()
        if not editor:
            return

        start_dir = str(Path(editor.file_path).parent) if editor.file_path else ""
        if not start_dir:
            start_dir = str(self.project_root) if self.project_root else str(Path.home())

        path, _ = QFileDialog.getSaveFileName(
            self,
            "Save Lupine Shader As",
            start_dir,
            "Lupine Shader Files (*.lsh);;All Files (*)"
        )
        if not path:
            return

        # Ensure .lsh extension
        if not path.endswith(".lsh"):
            path += ".lsh"

        # Update the editor's file path
        old_path = editor.file_path
        editor.file_path = path

        if editor.save_file():
            # Update tracking
            if old_path and old_path in self.open_shaders:
                del self.open_shaders[old_path]
            self.open_shaders[path] = editor
            self._update_tab_title(editor)
            self._update_status_bar()
            self.shader_saved.emit(path)
            self._log_output(f"Saved as: {path}")

    def validate_shader(self):
        """Parse the current .lsh source and report errors"""
        editor = self._current_editor()
        if not editor:
            self._log_output("No shader open to validate.")
            return

        source = editor.toPlainText()
        errors = self._parse_lsh(source)

        if not errors:
            self._compile_status_label.setText("Valid")
            self._compile_status_label.setStyleSheet("font-size: 11px; color: #4caf50;")
            self._log_output("Validation passed: No errors found.")
        else:
            self._compile_status_label.setText(f"{len(errors)} error(s)")
            self._compile_status_label.setStyleSheet("font-size: 11px; color: #d32f2f;")
            self._log_output("Validation failed:")
            for err in errors:
                self._log_output(f"  - {err}")

    def get_current_source(self) -> str:
        """Get the source code of the currently active editor"""
        editor = self._current_editor()
        if editor:
            return editor.toPlainText()
        return ""

    def set_current_source(self, source: str):
        """Set the source code of the currently active editor"""
        editor = self._current_editor()
        if editor:
            editor.setPlainText(source)

    def on_text_changed(self):
        """Called externally when text content changes (e.g. from undo system)"""
        editor = self._current_editor()
        if editor and not editor.is_modified:
            editor.is_modified = True
            editor.modified_changed.emit(True)

    # =========================================================================
    # Internal helpers
    # =========================================================================

    def _current_editor(self) -> ShaderCodeEditor:
        """Get the currently active ShaderCodeEditor, or None"""
        widget = self._shader_tabs.currentWidget()
        if isinstance(widget, ShaderCodeEditor):
            return widget
        return None

    def _on_tab_changed(self, index):
        """Handle tab selection change"""
        self._update_status_bar()

    def _on_editor_modified(self, editor, modified):
        """Handle editor modified state change"""
        self._update_tab_title(editor)
        self._update_status_bar()

    def _update_tab_title(self, editor):
        """Update the tab title to reflect modified state"""
        for i in range(self._shader_tabs.count()):
            if self._shader_tabs.widget(i) == editor:
                name = Path(editor.file_path).name if editor.file_path else "Untitled"
                if editor.is_modified:
                    name += " *"
                self._shader_tabs.setTabText(i, name)
                break

    def _update_status_bar(self):
        """Update the status bar with current file info"""
        editor = self._current_editor()
        if editor and editor.file_path:
            self._file_path_label.setText(editor.file_path)
        else:
            self._file_path_label.setText("No file open")
        # Reset compile status when switching tabs
        self._compile_status_label.setText("")
        self._compile_status_label.setStyleSheet("font-size: 11px;")

    def _close_shader_tab(self, index):
        """Close a shader tab, prompting to save if modified"""
        editor = self._shader_tabs.widget(index)
        if not isinstance(editor, ShaderCodeEditor):
            self._shader_tabs.removeTab(index)
            return

        if editor.is_modified:
            name = Path(editor.file_path).name if editor.file_path else "Untitled"
            reply = QMessageBox.question(
                self,
                "Unsaved Changes",
                f"'{name}' has unsaved changes.\nDo you want to save before closing?",
                QMessageBox.StandardButton.Save |
                QMessageBox.StandardButton.Discard |
                QMessageBox.StandardButton.Cancel
            )

            if reply == QMessageBox.StandardButton.Save:
                if editor.file_path:
                    editor.save_file()
                else:
                    self._shader_tabs.setCurrentIndex(index)
                    self.save_shader_as()
                    # If user cancelled save-as, don't close
                    if editor.is_modified:
                        return
            elif reply == QMessageBox.StandardButton.Cancel:
                return

        # Remove from tracking
        if editor.file_path and editor.file_path in self.open_shaders:
            del self.open_shaders[editor.file_path]
            self.shader_closed.emit(editor.file_path)

        self._shader_tabs.removeTab(index)
        self._update_status_bar()

    def _toggle_error_output(self):
        """Toggle the error output panel visibility"""
        if self._error_output.isVisible():
            self._error_output.hide()
            self._toggle_error_btn.setText("Show")
        else:
            self._error_output.show()
            self._toggle_error_btn.setText("Hide")

    def _clear_error_output(self):
        """Clear the error output"""
        self._error_output.clear()

    def _log_output(self, message: str):
        """Append a message to the error output area"""
        self._error_output.appendPlainText(message)

    # =========================================================================
    # .lsh Validation
    # =========================================================================

    def _parse_lsh(self, source: str) -> list:
        """
        Parse .lsh source and return a list of error strings.
        Returns an empty list if the shader is valid.
        """
        errors = []
        lines = source.split('\n')

        has_shader = False
        has_description = False
        has_vertex = False
        has_fragment = False

        open_blocks = []  # Stack of (block_name, line_number)

        for i, line in enumerate(lines):
            stripped = line.strip()
            line_num = i + 1

            # Check for #shader directive
            if stripped.startswith('#shader'):
                has_shader = True
                # Verify it has a quoted name
                rest = stripped[len('#shader'):].strip()
                if not (rest.startswith('"') and rest.endswith('"') and len(rest) > 2):
                    errors.append(f"Line {line_num}: #shader requires a quoted name, e.g. #shader \"MyShader\"")

            # Check for #description directive
            elif stripped.startswith('#description'):
                has_description = True
                rest = stripped[len('#description'):].strip()
                if not (rest.startswith('"') and rest.endswith('"') and len(rest) > 2):
                    errors.append(f"Line {line_num}: #description requires a quoted string")

            # Check for #begin
            elif stripped.startswith('#begin'):
                parts = stripped.split()
                if len(parts) < 2:
                    errors.append(f"Line {line_num}: #begin requires a section name (vertex, fragment, properties, shared)")
                else:
                    section = parts[1]
                    valid_sections = ['vertex', 'fragment', 'properties', 'shared']
                    if section not in valid_sections:
                        errors.append(f"Line {line_num}: Unknown section '{section}'. Valid: {', '.join(valid_sections)}")
                    open_blocks.append((section, line_num))

                    if section == 'vertex':
                        has_vertex = True
                    elif section == 'fragment':
                        has_fragment = True

            # Check for #end
            elif stripped.startswith('#end'):
                parts = stripped.split()
                if len(parts) < 2:
                    errors.append(f"Line {line_num}: #end requires a section name")
                else:
                    section = parts[1]
                    if not open_blocks:
                        errors.append(f"Line {line_num}: #end {section} without matching #begin")
                    else:
                        expected_section, begin_line = open_blocks.pop()
                        if section != expected_section:
                            errors.append(
                                f"Line {line_num}: #end {section} does not match "
                                f"#begin {expected_section} (line {begin_line})"
                            )

        # Check for unclosed blocks
        for section, begin_line in open_blocks:
            errors.append(f"Line {begin_line}: #begin {section} is never closed with #end {section}")

        # Check required directives
        if not has_shader:
            errors.append("Missing #shader directive")
        if not has_description:
            errors.append("Missing #description directive")
        if not has_vertex:
            errors.append("Missing #begin vertex / #end vertex block")
        if not has_fragment:
            errors.append("Missing #begin fragment / #end fragment block")

        return errors
