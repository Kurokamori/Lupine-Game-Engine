"""
Script Editor Panel
Edit scripts and code files with syntax highlighting
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                             QPushButton, QTextEdit, QComboBox, QTabWidget,
                             QSplitter, QFileDialog, QMessageBox, QDialog,
                             QFormLayout, QCheckBox, QDialogButtonBox, QListWidget,
                             QListWidgetItem, QLineEdit, QFrame,
                             QToolButton, QSizePolicy)
from PyQt6.QtCore import Qt, pyqtSignal, QRegularExpression, QTimer
from PyQt6.QtGui import (QFont, QSyntaxHighlighter, QTextCharFormat, QColor,
                        QFontMetrics, QPalette, QTextCursor, QTextDocument,
                        QKeySequence, QIcon, QShortcut)
from pathlib import Path
from .base_panel import EditorPanel
from editor.theme import get_theme_manager
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

        elif self.language == "lsh":
            # -- Directive format (purple, like preprocessor) --
            directive_format = QTextCharFormat()
            directive_format.setForeground(QColor("#C586C0"))
            directive_format.setFontWeight(QFont.Weight.Bold)
            self.highlighting_rules.append(
                (QRegularExpression(r'#shader|#description|#begin|#end|#input|#output|#feature'), directive_format))

            # -- Macro format (cyan) --
            macro_format = QTextCharFormat()
            macro_format.setForeground(QColor("#4FC1FF"))
            macro_format.setFontWeight(QFont.Weight.Bold)
            self.highlighting_rules.append(
                (QRegularExpression(
                    r'\bVERTEX_OUTPUT\b|\bSAMPLE\b|\bSAMPLE_LOD\b|\bSAMPLE_CUBE\b'
                    r'|\bMUL\b|\bMAT3\b|\bDISCARD\b|\bFRAG_COORD\b|\bINSTANCE_ID\b|\bVERTEX_ID\b'),
                 macro_format))

            # -- Keywords --
            lsh_keywords = [
                'void', 'float', 'int', 'bool', 'vec2', 'vec3', 'vec4',
                'ivec2', 'ivec3', 'ivec4', 'mat3', 'mat4', 'sampler2D',
                'samplerCube', 'uniform', 'const', 'struct', 'layout',
                'in', 'out', 'if', 'else', 'for', 'while', 'return',
                'discard', 'precision', 'highp', 'mediump', 'lowp'
            ]
            for word in lsh_keywords:
                pattern = QRegularExpression(f'\\b{word}\\b')
                self.highlighting_rules.append((pattern, keyword_format))

            # -- Built-in functions (function_format) --
            lsh_builtins = [
                'normalize', 'dot', 'cross', 'length', 'mix', 'clamp',
                'max', 'min', 'pow', 'sqrt', 'abs', 'sin', 'cos', 'tan',
                'atan', 'smoothstep', 'step', 'fract', 'mod', 'floor',
                'ceil', 'texture', 'dFdx', 'dFdy', 'reflect', 'refract'
            ]
            for fn in lsh_builtins:
                pattern = QRegularExpression(f'\\b{fn}\\b')
                self.highlighting_rules.append((pattern, function_format))

            # -- Semantic labels (class_format - teal) --
            self.highlighting_rules.append(
                (QRegularExpression(
                    r'\bPOSITION\b|\bNORMAL\b|\bTEXCOORD[0-9]\b|\bCOLOR\b'
                    r'|\bTANGENT\b|\bBLENDINDICES\b|\bBLENDWEIGHT\b'),
                 class_format))

            # -- Strings --
            self.highlighting_rules.append((QRegularExpression('"[^"\\\\]*(\\\\.[^"\\\\]*)*"'), string_format))

            # -- Single-line comments --
            self.highlighting_rules.append((QRegularExpression('//[^\n]*'), comment_format))

            # -- Multi-line comment markers (used in highlightBlock) --
            self._lsh_multiline_comment_start = QRegularExpression(r'/\*')
            self._lsh_multiline_comment_end = QRegularExpression(r'\*/')
            self._lsh_multiline_comment_format = QTextCharFormat()
            self._lsh_multiline_comment_format.setForeground(QColor("#6A9955"))
            self._lsh_multiline_comment_format.setFontItalic(True)

        # Numbers (common to all)
        self.highlighting_rules.append((QRegularExpression('\\b[0-9]+\\.?[0-9]*\\b'), number_format))
    
    def highlightBlock(self, text):
        """Apply syntax highlighting to a block of text"""
        for pattern, fmt in self.highlighting_rules:
            match_iterator = pattern.globalMatch(text)
            while match_iterator.hasNext():
                match = match_iterator.next()
                self.setFormat(match.capturedStart(), match.capturedLength(), fmt)

        # Handle multi-line /* */ comments for LSH and C/C++
        if self.language in ("lsh", "cpp", "c"):
            if self.language == "lsh":
                ml_start = self._lsh_multiline_comment_start
                ml_end = self._lsh_multiline_comment_end
                ml_fmt = self._lsh_multiline_comment_format
            else:
                # For C/C++ we create the patterns on the fly (they share the same format)
                if not hasattr(self, '_cpp_ml_start'):
                    self._cpp_ml_start = QRegularExpression(r'/\*')
                    self._cpp_ml_end = QRegularExpression(r'\*/')
                    self._cpp_ml_fmt = QTextCharFormat()
                    self._cpp_ml_fmt.setForeground(QColor("#6A9955"))
                    self._cpp_ml_fmt.setFontItalic(True)
                ml_start = self._cpp_ml_start
                ml_end = self._cpp_ml_end
                ml_fmt = self._cpp_ml_fmt

            self.setCurrentBlockState(0)

            start_index = 0
            if self.previousBlockState() != 1:
                match = ml_start.match(text)
                start_index = match.capturedStart() if match.hasMatch() else -1
            else:
                start_index = 0

            while start_index >= 0:
                end_match = ml_end.match(text, start_index)
                if end_match.hasMatch():
                    comment_length = end_match.capturedEnd() - start_index
                    self.setFormat(start_index, comment_length, ml_fmt)
                    next_match = ml_start.match(text, start_index + comment_length)
                    start_index = next_match.capturedStart() if next_match.hasMatch() else -1
                else:
                    self.setCurrentBlockState(1)
                    comment_length = len(text) - start_index
                    self.setFormat(start_index, comment_length, ml_fmt)
                    break


class SearchReplaceBar(QFrame):
    """Professional search and replace bar for the code editor"""

    # Signals
    find_requested = pyqtSignal(str, bool, bool, bool)  # text, case_sensitive, whole_word, forward
    replace_requested = pyqtSignal(str, str)  # find_text, replace_text
    replace_all_requested = pyqtSignal(str, str, bool, bool)  # find, replace, case_sensitive, whole_word
    close_requested = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFrameShape(QFrame.Shape.StyledPanel)
        self.setFrameShadow(QFrame.Shadow.Raised)
        self.setObjectName("SearchReplaceBar")

        # Apply theme-based styling
        self._apply_theme_style()

        self._setup_ui()
        self._replace_visible = False
        self._match_count = 0
        self._current_match = 0

    def _apply_theme_style(self):
        """Apply theme-based styling to the search bar"""
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()
        if not theme:
            return

        c = theme.colors

        self.setStyleSheet(f"""
            #SearchReplaceBar {{
                background-color: {c.secondary_color};
                border: 1px solid {c.border};
                border-radius: 4px;
                padding: 4px;
            }}
            #SearchReplaceBar QLineEdit {{
                background-color: {c.surface};
                border: 1px solid {c.border};
                border-radius: 3px;
                padding: 4px 8px;
                color: {c.text_primary};
                selection-background-color: {c.selection};
            }}
            #SearchReplaceBar QLineEdit:focus {{
                border: 1px solid {c.border_focus};
            }}
            #SearchReplaceBar QPushButton, #SearchReplaceBar QToolButton {{
                background-color: {c.surface};
                border: 1px solid {c.border};
                border-radius: 3px;
                padding: 4px 8px;
                color: {c.text_primary};
                min-width: 24px;
            }}
            #SearchReplaceBar QPushButton:hover, #SearchReplaceBar QToolButton:hover {{
                background-color: {c.surface_hover};
                border-color: {c.border_focus};
            }}
            #SearchReplaceBar QPushButton:pressed, #SearchReplaceBar QToolButton:pressed {{
                background-color: {c.tertiary_color};
            }}
            #SearchReplaceBar QPushButton:checked, #SearchReplaceBar QToolButton:checked {{
                background-color: {c.accent_color};
                border-color: {c.accent_color};
                color: {c.text_on_accent};
            }}
            #SearchReplaceBar QPushButton:checked:hover, #SearchReplaceBar QToolButton:checked:hover {{
                background-color: {c.accent_hover};
                border-color: {c.accent_hover};
            }}
            #SearchReplaceBar QLabel {{
                background-color: transparent;
                color: {c.text_primary};
            }}
        """)

        # Store theme colors for use in match highlighting
        self._theme_colors = c

    def refresh_theme(self):
        """Refresh the theme styling (call when theme changes)"""
        self._apply_theme_style()

    def _setup_ui(self):
        """Setup the search/replace bar UI"""
        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(8, 6, 8, 6)
        main_layout.setSpacing(4)

        # Search row
        search_row = QHBoxLayout()
        search_row.setSpacing(4)

        # Search input
        self.search_input = QLineEdit()
        self.search_input.setPlaceholderText("Find...")
        self.search_input.setMinimumWidth(200)
        self.search_input.textChanged.connect(self._on_search_text_changed)
        self.search_input.returnPressed.connect(self._find_next)
        search_row.addWidget(self.search_input)

        # Match info label
        self.match_label = QLabel("")
        self.match_label.setMinimumWidth(80)
        self.match_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        search_row.addWidget(self.match_label)

        # Case sensitive toggle
        self.case_sensitive_btn = QToolButton()
        self.case_sensitive_btn.setText("Aa")
        self.case_sensitive_btn.setToolTip("Match Case (Alt+C)")
        self.case_sensitive_btn.setCheckable(True)
        self.case_sensitive_btn.toggled.connect(self._on_options_changed)
        search_row.addWidget(self.case_sensitive_btn)

        # Whole word toggle
        self.whole_word_btn = QToolButton()
        self.whole_word_btn.setText("W")
        self.whole_word_btn.setToolTip("Match Whole Word (Alt+W)")
        self.whole_word_btn.setCheckable(True)
        self.whole_word_btn.toggled.connect(self._on_options_changed)
        search_row.addWidget(self.whole_word_btn)

        # Regex toggle
        self.regex_btn = QToolButton()
        self.regex_btn.setText(".*")
        self.regex_btn.setToolTip("Use Regular Expression (Alt+R)")
        self.regex_btn.setCheckable(True)
        self.regex_btn.toggled.connect(self._on_options_changed)
        search_row.addWidget(self.regex_btn)

        search_row.addSpacing(8)

        # Find previous button
        self.find_prev_btn = QPushButton("<")
        self.find_prev_btn.setToolTip("Find Previous (Shift+F3)")
        self.find_prev_btn.setFixedWidth(30)
        self.find_prev_btn.clicked.connect(self._find_previous)
        search_row.addWidget(self.find_prev_btn)

        # Find next button
        self.find_next_btn = QPushButton(">")
        self.find_next_btn.setToolTip("Find Next (F3 or Enter)")
        self.find_next_btn.setFixedWidth(30)
        self.find_next_btn.clicked.connect(self._find_next)
        search_row.addWidget(self.find_next_btn)

        search_row.addSpacing(8)

        # Toggle replace button
        self.toggle_replace_btn = QToolButton()
        self.toggle_replace_btn.setText("Replace")
        self.toggle_replace_btn.setToolTip("Toggle Replace (Ctrl+H)")
        self.toggle_replace_btn.setCheckable(True)
        self.toggle_replace_btn.toggled.connect(self._toggle_replace)
        search_row.addWidget(self.toggle_replace_btn)

        search_row.addStretch()

        # Close button
        self.close_btn = QPushButton("x")
        self.close_btn.setToolTip("Close (Escape)")
        self.close_btn.setFixedWidth(24)
        self.close_btn.clicked.connect(self._close)
        search_row.addWidget(self.close_btn)

        main_layout.addLayout(search_row)

        # Replace row (hidden by default)
        self.replace_widget = QWidget()
        replace_row = QHBoxLayout(self.replace_widget)
        replace_row.setContentsMargins(0, 0, 0, 0)
        replace_row.setSpacing(4)

        # Replace input
        self.replace_input = QLineEdit()
        self.replace_input.setPlaceholderText("Replace with...")
        self.replace_input.setMinimumWidth(200)
        self.replace_input.returnPressed.connect(self._replace_next)
        replace_row.addWidget(self.replace_input)

        # Spacer to align with match label
        replace_row.addSpacing(80 + 4)  # match_label width + spacing

        # Replace button
        self.replace_btn = QPushButton("Replace")
        self.replace_btn.setToolTip("Replace Current (Ctrl+Shift+1)")
        self.replace_btn.clicked.connect(self._replace_next)
        replace_row.addWidget(self.replace_btn)

        # Replace all button
        self.replace_all_btn = QPushButton("Replace All")
        self.replace_all_btn.setToolTip("Replace All (Ctrl+Shift+Enter)")
        self.replace_all_btn.clicked.connect(self._replace_all)
        replace_row.addWidget(self.replace_all_btn)

        replace_row.addStretch()

        self.replace_widget.setVisible(False)
        main_layout.addWidget(self.replace_widget)

        # Setup keyboard shortcuts within the bar
        self._setup_shortcuts()

    def _setup_shortcuts(self):
        """Setup keyboard shortcuts"""
        # Alt+C for case sensitive
        case_shortcut = QShortcut(QKeySequence("Alt+C"), self)
        case_shortcut.activated.connect(lambda: self.case_sensitive_btn.toggle())

        # Alt+W for whole word
        word_shortcut = QShortcut(QKeySequence("Alt+W"), self)
        word_shortcut.activated.connect(lambda: self.whole_word_btn.toggle())

        # Alt+R for regex
        regex_shortcut = QShortcut(QKeySequence("Alt+R"), self)
        regex_shortcut.activated.connect(lambda: self.regex_btn.toggle())

    def _on_search_text_changed(self, text):
        """Handle search text changes - trigger incremental search"""
        if text:
            self.find_requested.emit(text, self.case_sensitive_btn.isChecked(),
                                     self.whole_word_btn.isChecked(), True)
        else:
            self._update_match_info(0, 0)

    def _on_options_changed(self):
        """Handle search option changes"""
        text = self.search_input.text()
        if text:
            self.find_requested.emit(text, self.case_sensitive_btn.isChecked(),
                                     self.whole_word_btn.isChecked(), True)

    def _find_next(self):
        """Find next occurrence"""
        text = self.search_input.text()
        if text:
            self.find_requested.emit(text, self.case_sensitive_btn.isChecked(),
                                     self.whole_word_btn.isChecked(), True)

    def _find_previous(self):
        """Find previous occurrence"""
        text = self.search_input.text()
        if text:
            self.find_requested.emit(text, self.case_sensitive_btn.isChecked(),
                                     self.whole_word_btn.isChecked(), False)

    def _toggle_replace(self, checked):
        """Toggle replace panel visibility"""
        self._replace_visible = checked
        self.replace_widget.setVisible(checked)
        if checked:
            self.replace_input.setFocus()

    def _replace_next(self):
        """Replace current occurrence and find next"""
        find_text = self.search_input.text()
        replace_text = self.replace_input.text()
        if find_text:
            self.replace_requested.emit(find_text, replace_text)

    def _replace_all(self):
        """Replace all occurrences"""
        find_text = self.search_input.text()
        replace_text = self.replace_input.text()
        if find_text:
            self.replace_all_requested.emit(find_text, replace_text,
                                           self.case_sensitive_btn.isChecked(),
                                           self.whole_word_btn.isChecked())

    def _close(self):
        """Close the search bar"""
        self.close_requested.emit()
        self.hide()

    def show_search(self, selected_text=""):
        """Show the search bar, optionally with selected text"""
        self.show()
        if selected_text:
            self.search_input.setText(selected_text)
        self.search_input.setFocus()
        self.search_input.selectAll()

    def show_replace(self, selected_text=""):
        """Show the search bar with replace panel"""
        self.show()
        self.toggle_replace_btn.setChecked(True)
        if selected_text:
            self.search_input.setText(selected_text)
        self.search_input.setFocus()
        self.search_input.selectAll()

    def _update_match_info(self, current, total):
        """Update the match count label"""
        self._current_match = current
        self._match_count = total

        # Get theme colors
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()
        error_color = theme.colors.error_color if theme else "#d32f2f"
        text_color = theme.colors.text_primary if theme else "#cccccc"

        if total == 0:
            if self.search_input.text():
                self.match_label.setText("No results")
                self.match_label.setStyleSheet(f"color: {error_color};")
            else:
                self.match_label.setText("")
                self.match_label.setStyleSheet("")
        else:
            self.match_label.setText(f"{current} of {total}")
            self.match_label.setStyleSheet(f"color: {text_color};")

    def update_match_count(self, current, total):
        """Public method to update match count from editor"""
        self._update_match_info(current, total)

    def get_search_text(self):
        """Get the current search text"""
        return self.search_input.text()

    def is_case_sensitive(self):
        """Check if case sensitive search is enabled"""
        return self.case_sensitive_btn.isChecked()

    def is_whole_word(self):
        """Check if whole word search is enabled"""
        return self.whole_word_btn.isChecked()

    def is_regex(self):
        """Check if regex search is enabled"""
        return self.regex_btn.isChecked()

    def keyPressEvent(self, event):
        """Handle key press events"""
        if event.key() == Qt.Key.Key_Escape:
            self._close()
            event.accept()
        elif event.key() == Qt.Key.Key_F3:
            if event.modifiers() & Qt.KeyboardModifier.ShiftModifier:
                self._find_previous()
            else:
                self._find_next()
            event.accept()
        else:
            super().keyPressEvent(event)


class CodeEditor(QTextEdit):
    """Code editor widget with line numbers and syntax highlighting"""

    modified_changed = pyqtSignal(bool)  # Signal when modified state changes

    # Signals for search
    search_match_changed = pyqtSignal(int, int)  # current_match, total_matches

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

        # Search state
        self._search_text = ""
        self._search_matches = []  # List of (start, end) positions
        self._current_match_index = -1
        self._search_case_sensitive = False
        self._search_whole_word = False
        self._search_regex = False
        self._search_extra_selections = []

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

    # ==================== Search/Replace Methods ====================

    def find_all(self, text, case_sensitive=False, whole_word=False, use_regex=False):
        """Find all occurrences of text and highlight them"""
        self._search_text = text
        self._search_case_sensitive = case_sensitive
        self._search_whole_word = whole_word
        self._search_regex = use_regex
        self._search_matches = []

        if not text:
            self._clear_search_highlights()
            self.search_match_changed.emit(0, 0)
            return []

        document = self.document()
        content = document.toPlainText()

        # Build the search pattern
        if use_regex:
            try:
                pattern = text
            except Exception:
                self._clear_search_highlights()
                self.search_match_changed.emit(0, 0)
                return []
        else:
            pattern = QRegularExpression.escape(text)

        if whole_word:
            pattern = f"\\b{pattern}\\b"

        # Create regex with appropriate flags
        regex = QRegularExpression(pattern)
        if not case_sensitive:
            regex.setPatternOptions(QRegularExpression.PatternOption.CaseInsensitiveOption)

        # Find all matches
        match_iterator = regex.globalMatch(content)
        while match_iterator.hasNext():
            match = match_iterator.next()
            self._search_matches.append((match.capturedStart(), match.capturedEnd()))

        # Highlight all matches
        self._highlight_search_matches()

        # Update current match index
        if self._search_matches:
            # Find the match closest to current cursor position
            cursor_pos = self.textCursor().position()
            self._current_match_index = 0
            for i, (start, end) in enumerate(self._search_matches):
                if start >= cursor_pos:
                    self._current_match_index = i
                    break
            self.search_match_changed.emit(self._current_match_index + 1, len(self._search_matches))
        else:
            self._current_match_index = -1
            self.search_match_changed.emit(0, 0)

        return self._search_matches

    def find_next(self, text, case_sensitive=False, whole_word=False, use_regex=False):
        """Find and jump to the next occurrence"""
        # If search text changed, do a fresh search
        if (text != self._search_text or
            case_sensitive != self._search_case_sensitive or
            whole_word != self._search_whole_word or
            use_regex != self._search_regex):
            self.find_all(text, case_sensitive, whole_word, use_regex)

        if not self._search_matches:
            return False

        # Move to next match
        self._current_match_index = (self._current_match_index + 1) % len(self._search_matches)
        self._jump_to_match(self._current_match_index)
        self.search_match_changed.emit(self._current_match_index + 1, len(self._search_matches))
        return True

    def find_previous(self, text, case_sensitive=False, whole_word=False, use_regex=False):
        """Find and jump to the previous occurrence"""
        # If search text changed, do a fresh search
        if (text != self._search_text or
            case_sensitive != self._search_case_sensitive or
            whole_word != self._search_whole_word or
            use_regex != self._search_regex):
            self.find_all(text, case_sensitive, whole_word, use_regex)

        if not self._search_matches:
            return False

        # Move to previous match
        self._current_match_index = (self._current_match_index - 1) % len(self._search_matches)
        self._jump_to_match(self._current_match_index)
        self.search_match_changed.emit(self._current_match_index + 1, len(self._search_matches))
        return True

    def _jump_to_match(self, index):
        """Jump to a specific match by index"""
        if 0 <= index < len(self._search_matches):
            start, end = self._search_matches[index]
            cursor = self.textCursor()
            cursor.setPosition(start)
            cursor.setPosition(end, QTextCursor.MoveMode.KeepAnchor)
            self.setTextCursor(cursor)
            self.ensureCursorVisible()
            self._highlight_search_matches()  # Refresh highlights to show current match

    def _highlight_search_matches(self):
        """Highlight all search matches with different colors for current match"""
        extra_selections = []

        # Get theme colors for highlighting
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()

        if theme:
            c = theme.colors
            # Use selection colors from theme
            match_bg = c.selection_inactive  # Muted highlight for other matches
            current_match_bg = c.selection  # Active selection for current match
            current_match_fg = c.text_primary
        else:
            # Fallback colors
            match_bg = "#5a5a32"
            current_match_bg = "#515c6a"
            current_match_fg = "#ffffff"

        # Background color for all matches
        match_format = QTextCharFormat()
        match_format.setBackground(QColor(match_bg))

        # Background color for current match
        current_match_format = QTextCharFormat()
        current_match_format.setBackground(QColor(current_match_bg))
        current_match_format.setForeground(QColor(current_match_fg))

        for i, (start, end) in enumerate(self._search_matches):
            selection = QTextEdit.ExtraSelection()
            cursor = self.textCursor()
            cursor.setPosition(start)
            cursor.setPosition(end, QTextCursor.MoveMode.KeepAnchor)
            selection.cursor = cursor

            if i == self._current_match_index:
                selection.format = current_match_format
            else:
                selection.format = match_format

            extra_selections.append(selection)

        self._search_extra_selections = extra_selections
        self.setExtraSelections(extra_selections)

    def _clear_search_highlights(self):
        """Clear all search highlights"""
        self._search_extra_selections = []
        self.setExtraSelections([])
        self._search_matches = []
        self._current_match_index = -1

    def clear_search(self):
        """Public method to clear search state"""
        self._search_text = ""
        self._clear_search_highlights()

    def replace_current(self, find_text, replace_text):
        """Replace the current selected match and find next"""
        cursor = self.textCursor()

        # Check if current selection matches the find text
        if cursor.hasSelection():
            selected_text = cursor.selectedText()

            # Check if selection matches (considering case sensitivity)
            matches = False
            if self._search_case_sensitive:
                matches = (selected_text == find_text)
            else:
                matches = (selected_text.lower() == find_text.lower())

            if matches:
                # Replace the selection
                cursor.insertText(replace_text)
                self.setTextCursor(cursor)

                # Refresh search to update match positions
                self.find_all(find_text, self._search_case_sensitive,
                             self._search_whole_word, self._search_regex)

                # Find next
                if self._search_matches:
                    # Adjust index since we just replaced one
                    if self._current_match_index >= len(self._search_matches):
                        self._current_match_index = 0
                    self._jump_to_match(self._current_match_index)
                    self.search_match_changed.emit(self._current_match_index + 1, len(self._search_matches))
                return True

        # If no valid selection, just find next
        return self.find_next(find_text, self._search_case_sensitive,
                             self._search_whole_word, self._search_regex)

    def replace_all(self, find_text, replace_text, case_sensitive=False, whole_word=False):
        """Replace all occurrences of find_text with replace_text"""
        if not find_text:
            return 0

        # Build the search pattern
        pattern = QRegularExpression.escape(find_text)
        if whole_word:
            pattern = f"\\b{pattern}\\b"

        regex = QRegularExpression(pattern)
        if not case_sensitive:
            regex.setPatternOptions(QRegularExpression.PatternOption.CaseInsensitiveOption)

        content = self.toPlainText()

        # Count matches before replacement
        match_count = 0
        match_iterator = regex.globalMatch(content)
        while match_iterator.hasNext():
            match_iterator.next()
            match_count += 1

        if match_count == 0:
            return 0

        # Perform replacement
        new_content = regex.globalMatch(content)
        result = content
        offset = 0

        match_iterator = regex.globalMatch(content)
        while match_iterator.hasNext():
            match = match_iterator.next()
            start = match.capturedStart() + offset
            end = match.capturedEnd() + offset
            result = result[:start] + replace_text + result[end:]
            offset += len(replace_text) - (match.capturedEnd() - match.capturedStart())

        # Update the document
        cursor = self.textCursor()
        cursor.beginEditBlock()
        cursor.select(QTextCursor.SelectionType.Document)
        cursor.insertText(result)
        cursor.endEditBlock()

        # Clear search state
        self._clear_search_highlights()
        self.search_match_changed.emit(0, 0)

        return match_count

    def get_selected_text(self):
        """Get the currently selected text"""
        cursor = self.textCursor()
        return cursor.selectedText() if cursor.hasSelection() else ""

    # ==================== End Search/Replace Methods ====================

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
                '.md': 'markdown',
                '.lsh': 'lsh',
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
            "GLSL Shader (.glsl)",
            "Lupine Shader (.lsh)"
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

        # Separator before validate button
        sep = QFrame()
        sep.setFrameShape(QFrame.Shape.VLine)
        sep.setFrameShadow(QFrame.Shadow.Sunken)
        sep.setFixedWidth(2)
        toolbar_layout.addWidget(sep)

        # Validate Shader button (only visible for .lsh files)
        self.validate_shader_btn = QPushButton("Validate Shader")
        self.validate_shader_btn.setToolTip("Validate .lsh shader structure")
        self.validate_shader_btn.clicked.connect(self._validate_current_shader)
        self.validate_shader_btn.setVisible(False)
        toolbar_layout.addWidget(self.validate_shader_btn)

        # Validation status label
        self._validate_status_label = QLabel("")
        self._validate_status_label.setStyleSheet("font-size: 11px; font-style: italic;")
        self._validate_status_label.setVisible(False)
        toolbar_layout.addWidget(self._validate_status_label)

        toolbar_layout.addStretch()

        # Markdown preview toggle
        self.preview_toggle = QPushButton("Preview")
        self.preview_toggle.setCheckable(True)
        self.preview_toggle.setVisible(False)
        self.preview_toggle.toggled.connect(self._toggle_markdown_preview)
        toolbar_layout.addWidget(self.preview_toggle)
        
        right_layout.addLayout(toolbar_layout)

        # Search/Replace bar (hidden by default)
        self.search_bar = SearchReplaceBar()
        self.search_bar.hide()
        self.search_bar.find_requested.connect(self._on_find_requested)
        self.search_bar.replace_requested.connect(self._on_replace_requested)
        self.search_bar.replace_all_requested.connect(self._on_replace_all_requested)
        self.search_bar.close_requested.connect(self._on_search_closed)
        right_layout.addWidget(self.search_bar)

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

        # Setup keyboard shortcuts
        self._setup_search_shortcuts()
    
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
            "All Files (*.*);;Python (*.py);;Lua (*.lua);;mRuby (*.rb);;C++ (*.cpp *.h *.hpp);;JSON (*.json);;Lupine Shader (*.lsh);;Markdown (*.md)"
        )
        
        if file_path:
            self.open_script(file_path)
    
    def handle_save(self) -> bool:
        """Save the active script when Ctrl+S is pressed with this panel focused"""
        self._save_current_script()
        return True

    def handle_save_as(self) -> bool:
        """Scripts have no separate save-as path; save the active script in place"""
        self._save_current_script()
        return True

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
            if editor.save_file():
                # Emit signal to notify that the script was saved
                if editor.file_path:
                    self.script_saved.emit(editor.file_path)

    def _save_all_scripts(self):
        """Save all open scripts"""
        for file_path, widget in self.open_scripts.items():
            if isinstance(widget, QSplitter):
                editor = widget.editor
            else:
                editor = widget

            if isinstance(editor, CodeEditor) and editor.is_modified:
                if editor.save_file():
                    # Emit signal to notify that the script was saved
                    if editor.file_path:
                        self.script_saved.emit(editor.file_path)
    
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
            self.validate_shader_btn.setVisible(False)
            self._validate_status_label.setVisible(False)
            return

        widget = self.script_tabs.widget(index)

        # Show preview toggle for markdown files
        is_markdown_splitter = isinstance(widget, QSplitter) and hasattr(widget, 'preview')
        self.preview_toggle.setVisible(is_markdown_splitter)

        if is_markdown_splitter:
            self.preview_toggle.setChecked(widget.preview.isVisible())

        # Show/hide Validate Shader button for .lsh files
        editor = self._get_current_editor()
        is_lsh = (isinstance(editor, CodeEditor) and editor.file_path and
                  editor.file_path.lower().endswith('.lsh'))
        self.validate_shader_btn.setVisible(is_lsh)
        self._validate_status_label.setVisible(is_lsh)
        if not is_lsh:
            self._validate_status_label.setText("")
    
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

    # ==================== Shader Validation ====================

    def _validate_current_shader(self):
        """Validate the current .lsh shader file structure"""
        editor = self._get_current_editor()
        if not editor or not isinstance(editor, CodeEditor):
            return
        if not editor.file_path or not editor.file_path.lower().endswith('.lsh'):
            return

        source = editor.toPlainText()
        errors = self._parse_lsh(source)

        if not errors:
            self._validate_status_label.setText("Valid")
            self._validate_status_label.setStyleSheet("font-size: 11px; color: #4caf50; font-weight: bold;")
            QMessageBox.information(self, "Shader Validation", "Shader is valid. No errors found.")
        else:
            self._validate_status_label.setText(f"{len(errors)} error(s)")
            self._validate_status_label.setStyleSheet("font-size: 11px; color: #d32f2f; font-weight: bold;")
            error_text = "Shader validation failed:\n\n" + "\n".join(f"  - {e}" for e in errors)
            QMessageBox.warning(self, "Shader Validation", error_text)

    def _parse_lsh(self, source: str) -> list:
        """
        Parse .lsh source and return a list of error strings.
        Returns an empty list if the shader is valid.
        """
        errors = []
        lines = source.split('\n')

        has_shader = False
        has_vertex = False
        has_fragment = False

        open_blocks = []  # Stack of (block_name, line_number)

        for i, line in enumerate(lines):
            stripped = line.strip()
            line_num = i + 1

            # Skip comments
            if stripped.startswith('//'):
                continue

            # Check for #shader directive
            if stripped.startswith('#shader'):
                has_shader = True
                rest = stripped[len('#shader'):].strip()
                if not (rest.startswith('"') and rest.endswith('"') and len(rest) > 2):
                    errors.append(f"Line {line_num}: #shader requires a quoted name, e.g. #shader \"MyShader\"")

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
        if not has_vertex:
            errors.append("Missing #begin vertex / #end vertex block")
        if not has_fragment:
            errors.append("Missing #begin fragment / #end fragment block")

        return errors

    # ==================== Search/Replace Methods ====================

    def _setup_search_shortcuts(self):
        """Setup keyboard shortcuts for search functionality"""
        # Ctrl+F - Find
        find_shortcut = QShortcut(QKeySequence("Ctrl+F"), self)
        find_shortcut.activated.connect(self.show_search)

        # Ctrl+H - Find and Replace
        replace_shortcut = QShortcut(QKeySequence("Ctrl+H"), self)
        replace_shortcut.activated.connect(self.show_replace)

        # F3 - Find Next
        find_next_shortcut = QShortcut(QKeySequence("F3"), self)
        find_next_shortcut.activated.connect(self._find_next)

        # Shift+F3 - Find Previous
        find_prev_shortcut = QShortcut(QKeySequence("Shift+F3"), self)
        find_prev_shortcut.activated.connect(self._find_previous)

        # Escape - Close search bar (also handled in search bar itself)
        escape_shortcut = QShortcut(QKeySequence("Escape"), self)
        escape_shortcut.activated.connect(self._close_search_bar)

        # Ctrl+G - Go to line (bonus feature)
        goto_shortcut = QShortcut(QKeySequence("Ctrl+G"), self)
        goto_shortcut.activated.connect(self._show_goto_line_dialog)

    def _get_current_editor(self):
        """Get the currently active CodeEditor"""
        current_widget = self.script_tabs.currentWidget()
        if not current_widget:
            return None

        if isinstance(current_widget, QSplitter):
            return current_widget.editor
        elif isinstance(current_widget, CodeEditor):
            return current_widget
        return None

    def show_search(self):
        """Show the search bar"""
        editor = self._get_current_editor()
        if editor:
            selected_text = editor.get_selected_text()
            self.search_bar.show_search(selected_text)
            # Connect match updates
            try:
                editor.search_match_changed.disconnect(self.search_bar.update_match_count)
            except TypeError:
                pass  # Not connected
            editor.search_match_changed.connect(self.search_bar.update_match_count)

    def show_replace(self):
        """Show the search bar with replace panel"""
        editor = self._get_current_editor()
        if editor:
            selected_text = editor.get_selected_text()
            self.search_bar.show_replace(selected_text)
            # Connect match updates
            try:
                editor.search_match_changed.disconnect(self.search_bar.update_match_count)
            except TypeError:
                pass  # Not connected
            editor.search_match_changed.connect(self.search_bar.update_match_count)

    def _on_find_requested(self, text, case_sensitive, whole_word, forward):
        """Handle find request from search bar"""
        editor = self._get_current_editor()
        if not editor:
            return

        use_regex = self.search_bar.is_regex()

        if forward:
            editor.find_next(text, case_sensitive, whole_word, use_regex)
        else:
            editor.find_previous(text, case_sensitive, whole_word, use_regex)

    def _on_replace_requested(self, find_text, replace_text):
        """Handle replace request from search bar"""
        editor = self._get_current_editor()
        if editor:
            editor.replace_current(find_text, replace_text)

    def _on_replace_all_requested(self, find_text, replace_text, case_sensitive, whole_word):
        """Handle replace all request from search bar"""
        editor = self._get_current_editor()
        if editor:
            count = editor.replace_all(find_text, replace_text, case_sensitive, whole_word)
            if count > 0:
                QMessageBox.information(
                    self,
                    "Replace All",
                    f"Replaced {count} occurrence{'s' if count != 1 else ''}."
                )
            else:
                QMessageBox.information(
                    self,
                    "Replace All",
                    "No occurrences found."
                )

    def _on_search_closed(self):
        """Handle search bar close"""
        editor = self._get_current_editor()
        if editor:
            editor.clear_search()
            editor.setFocus()

    def _close_search_bar(self):
        """Close the search bar if visible"""
        if self.search_bar.isVisible():
            self.search_bar.hide()
            self._on_search_closed()

    def _find_next(self):
        """Find next occurrence using current search text"""
        if self.search_bar.isVisible():
            text = self.search_bar.get_search_text()
            if text:
                editor = self._get_current_editor()
                if editor:
                    editor.find_next(
                        text,
                        self.search_bar.is_case_sensitive(),
                        self.search_bar.is_whole_word(),
                        self.search_bar.is_regex()
                    )

    def _find_previous(self):
        """Find previous occurrence using current search text"""
        if self.search_bar.isVisible():
            text = self.search_bar.get_search_text()
            if text:
                editor = self._get_current_editor()
                if editor:
                    editor.find_previous(
                        text,
                        self.search_bar.is_case_sensitive(),
                        self.search_bar.is_whole_word(),
                        self.search_bar.is_regex()
                    )

    def _show_goto_line_dialog(self):
        """Show go to line dialog"""
        editor = self._get_current_editor()
        if not editor:
            return

        # Get current line count
        doc = editor.document()
        line_count = doc.blockCount()

        # Get current line
        cursor = editor.textCursor()
        current_line = cursor.blockNumber() + 1

        # Create dialog
        dialog = QDialog(self)
        dialog.setWindowTitle("Go to Line")
        dialog.setModal(True)
        dialog.setFixedWidth(300)

        layout = QVBoxLayout(dialog)

        label = QLabel(f"Enter line number (1-{line_count}):")
        layout.addWidget(label)

        line_input = QLineEdit()
        line_input.setText(str(current_line))
        line_input.selectAll()
        layout.addWidget(line_input)

        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(dialog.accept)
        buttons.rejected.connect(dialog.reject)
        layout.addWidget(buttons)

        if dialog.exec() == QDialog.DialogCode.Accepted:
            try:
                line_num = int(line_input.text())
                if 1 <= line_num <= line_count:
                    # Move cursor to line
                    block = doc.findBlockByLineNumber(line_num - 1)
                    cursor = editor.textCursor()
                    cursor.setPosition(block.position())
                    editor.setTextCursor(cursor)
                    editor.ensureCursorVisible()
                    editor.setFocus()
            except ValueError:
                pass

    # ==================== End Search/Replace Methods ====================

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
            '.lsh': (
                f'#shader "{name}"\n'
                f'#description "Custom shader"\n'
                f'\n'
                f'#begin properties\n'
                f'    uniform mat4 u_ViewProjection;\n'
                f'    uniform mat4 u_Model;\n'
                f'    uniform vec4 u_TintColor = vec4(1.0);\n'
                f'#end properties\n'
                f'\n'
                f'#begin vertex\n'
                f'    #input vec3 a_Position : POSITION 0\n'
                f'    #input vec3 a_Normal : NORMAL 1\n'
                f'    #input vec2 a_TexCoord : TEXCOORD0 2\n'
                f'    #input vec4 a_Color : COLOR 3\n'
                f'\n'
                f'    #output vec2 v_TexCoord\n'
                f'    #output vec4 v_Color\n'
                f'\n'
                f'    void main() {{\n'
                f'        v_TexCoord = a_TexCoord;\n'
                f'        v_Color = a_Color;\n'
                f'        VERTEX_OUTPUT = MUL(u_ViewProjection, MUL(u_Model, vec4(a_Position, 1.0)));\n'
                f'    }}\n'
                f'#end vertex\n'
                f'\n'
                f'#begin fragment\n'
                f'    #output vec4 FragColor\n'
                f'\n'
                f'    void main() {{\n'
                f'        FragColor = v_Color * u_TintColor;\n'
                f'    }}\n'
                f'#end fragment\n'
            ),
        }
        return templates.get(extension, '')
