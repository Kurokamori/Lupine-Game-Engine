"""
Localization Editor Dialog

Unity-like localization authoring for Lupine projects:
  - Multiple string tables / groups (one file per table) under the project's
    localization directory, in native JSON (.loctable) or plain CSV (.csv).
  - A key x locale grid with inline editing, add/remove/rename/duplicate keys.
  - Easy language adding (adds a locale column + project locale).
  - Tagging (per-key metadata) plus tag + text filtering and a missing-translation
    audit (highlights untranslated cells, "show only missing" filter, counts).
  - Plural-form editing per locale (CLDR-style categories).
  - CSV import/export per table, and a project-wide "CSV mode" option.
  - Viewport preview: switch the previewed locale live via the editor bridge.

The dialog reads and writes the engine's data files directly (the same files the
runtime LocalizationManager loads): <tables_dir>/*.loctable / *.csv and
localization.json at the project root.
"""

import csv
import io
import json
from pathlib import Path
from typing import Dict, List, Optional, Any

from PyQt6.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QPushButton,
                             QTableWidget, QTableWidgetItem, QLabel, QWidget,
                             QLineEdit, QMessageBox, QComboBox, QCheckBox,
                             QGroupBox, QFormLayout, QInputDialog, QFileDialog,
                             QHeaderView, QAbstractItemView, QDialogButtonBox)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QColor, QBrush

try:
    from theme import get_theme_manager
except Exception:
    get_theme_manager = None


RESERVED_COLUMNS = ["Key", "Tags", "Comment"]

COMMON_LOCALES = [
    ("en", "English"), ("en-US", "English (US)"), ("en-GB", "English (UK)"),
    ("fr", "French"), ("es", "Spanish"), ("es-419", "Spanish (Latin America)"),
    ("de", "German"), ("it", "Italian"), ("pt", "Portuguese"),
    ("pt-BR", "Portuguese (Brazil)"), ("ru", "Russian"), ("uk", "Ukrainian"),
    ("pl", "Polish"), ("nl", "Dutch"), ("sv", "Swedish"), ("da", "Danish"),
    ("fi", "Finnish"), ("nb", "Norwegian"), ("cs", "Czech"), ("sk", "Slovak"),
    ("tr", "Turkish"), ("el", "Greek"), ("ja", "Japanese"), ("zh", "Chinese"),
    ("zh-CN", "Chinese (Simplified)"), ("zh-TW", "Chinese (Traditional)"),
    ("ko", "Korean"), ("ar", "Arabic"), ("he", "Hebrew"), ("hi", "Hindi"),
    ("th", "Thai"), ("vi", "Vietnamese"), ("id", "Indonesian"),
]

DEFAULT_CONFIG = {
    "enabled": True,
    "default_locale": "en",
    "fallback_locale": "en",
    "locales": ["en"],
    "tables_dir": "localization",
    "csv_mode": False,
    "pseudolocalization": False,
}


def plural_categories(locale: str) -> List[str]:
    """Plural categories that apply to a locale (mirrors core PluralRules)."""
    lang = locale.replace("_", "-").split("-")[0].lower()
    no_plural = {"ja", "zh", "ko", "vi", "th", "id", "ms", "lo", "km", "my"}
    if lang in no_plural:
        return ["other"]
    if lang == "ar":
        return ["zero", "one", "two", "few", "many", "other"]
    if lang == "cy":
        return ["zero", "one", "two", "few", "many", "other"]
    if lang in {"ru", "uk", "be", "sr", "hr", "bs", "pl"}:
        return ["one", "few", "many", "other"]
    if lang in {"cs", "sk", "lt"}:
        return ["one", "few", "other"]
    return ["one", "other"]


class AddLanguageDialog(QDialog):
    """Pick a locale code from common languages or type a custom BCP-47 tag."""

    def __init__(self, existing: List[str], parent=None):
        super().__init__(parent)
        self.existing = set(existing)
        self.setWindowTitle("Add Language")
        self.setModal(True)
        self.setMinimumWidth(360)

        layout = QVBoxLayout()
        layout.addWidget(QLabel("Select a language or enter a custom locale code:"))

        self.combo = QComboBox()
        self.combo.setEditable(True)
        for code, name in COMMON_LOCALES:
            if code not in self.existing:
                self.combo.addItem(f"{name}  ({code})", code)
        layout.addWidget(self.combo)

        hint = QLabel("Examples: en, en-US, fr, pt-BR, zh-CN")
        hint.setProperty("secondary", True)
        layout.addWidget(hint)

        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
        self.setLayout(layout)

    def get_locale(self) -> str:
        idx = self.combo.currentIndex()
        data = self.combo.itemData(idx)
        text = self.combo.currentText().strip()
        # If the user kept a list entry, use its code; otherwise parse free text.
        if data and text == self.combo.itemText(idx):
            return str(data)
        # Allow "Name (code)" or a bare code.
        if "(" in text and text.endswith(")"):
            return text[text.rfind("(") + 1:-1].strip()
        return text


class PluralEditorDialog(QDialog):
    """Edit plural forms for one key across all locales."""

    def __init__(self, key: str, locales: List[str], plurals: Dict[str, Dict[str, str]],
                 parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"Plural Forms - {key}")
        self.setModal(True)
        self.setMinimumSize(520, 420)
        self._plurals = {loc: dict(forms) for loc, forms in (plurals or {}).items()}
        self._locales = locales
        self._edits: Dict[str, Dict[str, QLineEdit]] = {}

        layout = QVBoxLayout()
        info = QLabel(
            "Plural forms are chosen by the active locale and a {count} value.\n"
            "Use {count} (and any other {name} placeholders) inside each form.")
        info.setProperty("secondary", True)
        layout.addWidget(info)

        self.locale_combo = QComboBox()
        for loc in locales:
            self.locale_combo.addItem(loc)
        self.locale_combo.currentTextChanged.connect(self._show_locale)
        form_top = QFormLayout()
        form_top.addRow("Locale:", self.locale_combo)
        layout.addLayout(form_top)

        self.forms_group = QGroupBox("Forms")
        self.forms_layout = QFormLayout()
        self.forms_group.setLayout(self.forms_layout)
        layout.addWidget(self.forms_group)
        layout.addStretch()

        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self._on_accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
        self.setLayout(layout)

        if locales:
            self._show_locale(locales[0])

    def _show_locale(self, locale: str):
        if not locale:
            return
        self._commit_visible()
        # Rebuild the form fields for the selected locale.
        while self.forms_layout.rowCount() > 0:
            self.forms_layout.removeRow(0)
        self._edits.setdefault(locale, {})
        existing = self._plurals.get(locale, {})
        for cat in plural_categories(locale):
            edit = QLineEdit(existing.get(cat, ""))
            edit.setPlaceholderText(f"{cat} form, e.g. {{count}} item(s)")
            self.forms_layout.addRow(cat, edit)
            self._edits[locale][cat] = edit
        self._current_locale = locale

    def _commit_visible(self):
        loc = getattr(self, "_current_locale", None)
        if not loc or loc not in self._edits:
            return
        forms = {}
        for cat, edit in self._edits[loc].items():
            text = edit.text().strip()
            if text:
                forms[cat] = text
        if forms:
            self._plurals[loc] = forms
        elif loc in self._plurals:
            del self._plurals[loc]

    def _on_accept(self):
        self._commit_visible()
        self.accept()

    def get_plurals(self) -> Dict[str, Dict[str, str]]:
        return self._plurals


class LocalizationEditorDialog(QDialog):
    """Main localization editor."""

    # Emitted with the updated localization config when the user saves.
    localization_changed = pyqtSignal(dict)
    # Emitted when the previewed locale changes or after a save, so the host can
    # refresh the viewport.
    localization_applied = pyqtSignal()

    def __init__(self, project, editor_bridge=None, parent=None):
        super().__init__(parent)
        self.project = project
        self.editor_bridge = editor_bridge
        self.project_dir = project.get_directory() if project else ""

        self.config: Dict[str, Any] = dict(DEFAULT_CONFIG)
        if project and getattr(project, "localization", None):
            self.config.update(project.localization)

        self.tables: List[Dict[str, Any]] = []
        self.deleted_paths: List[str] = []
        self.current_table_index: int = -1
        self._loading = False
        self._suppress_commit = False

        self.setWindowTitle("Localization")
        self.setModal(True)
        self.setMinimumSize(960, 640)

        self._setup_ui()
        self._load_tables()
        self._refresh_table_selector()
        if self.tables:
            self._select_table(0)
        self._refresh_preview_combo()

    # ------------------------------------------------------------------ UI

    def _setup_ui(self):
        layout = QVBoxLayout()

        title = QLabel("Localization")
        title.setStyleSheet("font-size: 16px; font-weight: bold;")
        layout.addWidget(title)

        # --- Table / preview toolbar ---
        toolbar = QHBoxLayout()
        toolbar.addWidget(QLabel("Table:"))
        self.table_combo = QComboBox()
        self.table_combo.setMinimumWidth(160)
        self.table_combo.currentIndexChanged.connect(self._on_table_combo_changed)
        toolbar.addWidget(self.table_combo)

        new_table_btn = QPushButton("New Table")
        new_table_btn.clicked.connect(self._on_new_table)
        toolbar.addWidget(new_table_btn)

        del_table_btn = QPushButton("Delete Table")
        del_table_btn.setProperty("danger", True)
        del_table_btn.clicked.connect(self._on_delete_table)
        toolbar.addWidget(del_table_btn)

        import_btn = QPushButton("Import CSV")
        import_btn.setProperty("secondary", True)
        import_btn.clicked.connect(self._on_import_csv)
        toolbar.addWidget(import_btn)

        export_btn = QPushButton("Export CSV")
        export_btn.setProperty("secondary", True)
        export_btn.clicked.connect(self._on_export_csv)
        toolbar.addWidget(export_btn)

        toolbar.addStretch()

        toolbar.addWidget(QLabel("Preview:"))
        self.preview_combo = QComboBox()
        self.preview_combo.setMinimumWidth(120)
        self.preview_combo.currentTextChanged.connect(self._on_preview_changed)
        toolbar.addWidget(self.preview_combo)

        self.pseudo_check = QCheckBox("Pseudo")
        self.pseudo_check.setToolTip("Preview pseudolocalization (layout testing)")
        self.pseudo_check.setChecked(bool(self.config.get("pseudolocalization", False)))
        self.pseudo_check.toggled.connect(self._on_pseudo_toggled)
        toolbar.addWidget(self.pseudo_check)

        layout.addLayout(toolbar)

        # --- Filter bar ---
        filter_bar = QHBoxLayout()
        filter_bar.addWidget(QLabel("Search:"))
        self.search_edit = QLineEdit()
        self.search_edit.setPlaceholderText("Filter by key, tag, or text...")
        self.search_edit.textChanged.connect(self._apply_filter)
        filter_bar.addWidget(self.search_edit, 2)

        filter_bar.addWidget(QLabel("Tag:"))
        self.tag_filter_combo = QComboBox()
        self.tag_filter_combo.addItem("All Tags", "")
        self.tag_filter_combo.currentIndexChanged.connect(self._apply_filter)
        filter_bar.addWidget(self.tag_filter_combo, 1)

        self.missing_check = QCheckBox("Only missing")
        self.missing_check.toggled.connect(self._apply_filter)
        filter_bar.addWidget(self.missing_check)

        self.status_label = QLabel("")
        self.status_label.setProperty("secondary", True)
        filter_bar.addWidget(self.status_label)
        filter_bar.addStretch()
        layout.addLayout(filter_bar)

        # --- Grid ---
        self.grid = QTableWidget()
        self.grid.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.grid.setAlternatingRowColors(True)
        self.grid.itemChanged.connect(self._on_item_changed)
        self.grid.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Interactive)
        layout.addWidget(self.grid, 1)

        # --- Key actions ---
        key_bar = QHBoxLayout()
        add_key_btn = QPushButton("Add Key")
        add_key_btn.clicked.connect(self._on_add_key)
        key_bar.addWidget(add_key_btn)

        remove_key_btn = QPushButton("Remove Key")
        remove_key_btn.setProperty("danger", True)
        remove_key_btn.clicked.connect(self._on_remove_key)
        key_bar.addWidget(remove_key_btn)

        dup_key_btn = QPushButton("Duplicate Key")
        dup_key_btn.setProperty("secondary", True)
        dup_key_btn.clicked.connect(self._on_duplicate_key)
        key_bar.addWidget(dup_key_btn)

        plural_btn = QPushButton("Edit Plurals...")
        plural_btn.setProperty("secondary", True)
        plural_btn.clicked.connect(self._on_edit_plurals)
        key_bar.addWidget(plural_btn)

        key_bar.addStretch()

        add_lang_btn = QPushButton("Add Language")
        add_lang_btn.clicked.connect(self._on_add_language)
        key_bar.addWidget(add_lang_btn)

        remove_lang_btn = QPushButton("Remove Language")
        remove_lang_btn.setProperty("secondary", True)
        remove_lang_btn.clicked.connect(self._on_remove_language)
        key_bar.addWidget(remove_lang_btn)

        layout.addLayout(key_bar)

        # --- Dialog buttons ---
        bottom = QHBoxLayout()
        bottom.addStretch()
        close_btn = QPushButton("Close")
        close_btn.setProperty("secondary", True)
        close_btn.clicked.connect(self.reject)
        bottom.addWidget(close_btn)
        save_btn = QPushButton("Save")
        save_btn.clicked.connect(self._on_save)
        bottom.addWidget(save_btn)
        layout.addLayout(bottom)

        self.setLayout(layout)

    # -------------------------------------------------------------- helpers

    def _tables_dir(self) -> Path:
        return Path(self.project_dir) / self.config.get("tables_dir", "localization")

    def _theme_colors(self):
        if get_theme_manager:
            try:
                return get_theme_manager().get_current_theme().colors
            except Exception:
                return None
        return None

    def _current_table(self) -> Optional[Dict[str, Any]]:
        if 0 <= self.current_table_index < len(self.tables):
            return self.tables[self.current_table_index]
        return None

    def _display_locales(self, table: Dict[str, Any]) -> List[str]:
        """Locale column order: project locales first, then table-only locales."""
        ordered = list(self.config.get("locales", []))
        for loc in table.get("locales", []):
            if loc not in ordered:
                ordered.append(loc)
        if not ordered:
            ordered = ["en"]
        return ordered

    # ------------------------------------------------------------- loading

    def _new_table_model(self, name: str) -> Dict[str, Any]:
        return {
            "name": name,
            "source_path": None,
            "format": "csv" if self.config.get("csv_mode") else "json",
            "locales": list(self.config.get("locales", ["en"])),
            "entries": [],
        }

    def _load_tables(self):
        self.tables = []
        tdir = self._tables_dir()
        if tdir.exists():
            for path in sorted(tdir.iterdir()):
                if not path.is_file():
                    continue
                suffix = path.suffix.lower()
                try:
                    if suffix == ".loctable":
                        self.tables.append(self._load_json_table(path))
                    elif suffix == ".csv":
                        self.tables.append(self._load_csv_table(path))
                except Exception as exc:
                    print(f"Localization: failed to load {path}: {exc}")
        if not self.tables:
            self.tables.append(self._new_table_model("strings"))

    def _load_json_table(self, path: Path) -> Dict[str, Any]:
        with open(path, "r", encoding="utf-8") as fh:
            data = json.load(fh)
        table = {
            "name": data.get("name", path.stem),
            "source_path": str(path),
            "format": "json",
            "locales": list(data.get("locales", [])),
            "entries": [],
        }
        for entry in data.get("entries", []):
            if "key" not in entry:
                continue
            table["entries"].append({
                "key": entry.get("key", ""),
                "tags": list(entry.get("tags", [])),
                "comment": entry.get("comment", ""),
                "values": dict(entry.get("values", {})),
                "plurals": {loc: dict(forms) for loc, forms in entry.get("plurals", {}).items()},
            })
        return table

    def _load_csv_table(self, path: Path) -> Dict[str, Any]:
        table = {
            "name": path.stem,
            "source_path": str(path),
            "format": "csv",
            "locales": [],
            "entries": [],
        }
        with open(path, "r", encoding="utf-8", newline="") as fh:
            reader = csv.reader(fh)
            rows = list(reader)
        if not rows:
            return table
        header = rows[0]
        key_col = tags_col = comment_col = -1
        locale_cols: List[tuple] = []
        for idx, name in enumerate(header):
            low = name.strip().lower()
            if low == "key":
                key_col = idx
            elif low == "tags":
                tags_col = idx
            elif low in ("comment", "description"):
                comment_col = idx
            elif name.strip():
                locale_cols.append((idx, name.strip()))
                table["locales"].append(name.strip())
        if key_col < 0:
            return table
        for row in rows[1:]:
            if len(row) <= key_col or not row[key_col].strip():
                continue
            entry = {
                "key": row[key_col].strip(),
                "tags": [t.strip() for t in (row[tags_col].split(";") if 0 <= tags_col < len(row) else []) if t.strip()],
                "comment": row[comment_col] if 0 <= comment_col < len(row) else "",
                "values": {},
                "plurals": {},
            }
            for idx, loc in locale_cols:
                if idx < len(row) and row[idx] != "":
                    entry["values"][loc] = row[idx]
            table["entries"].append(entry)
        return table

    # --------------------------------------------------------- selectors

    def _refresh_table_selector(self):
        self._suppress_commit = True
        self.table_combo.blockSignals(True)
        self.table_combo.clear()
        for table in self.tables:
            self.table_combo.addItem(table["name"])
        self.table_combo.blockSignals(False)
        self._suppress_commit = False

    def _refresh_preview_combo(self):
        self.preview_combo.blockSignals(True)
        current = self.preview_combo.currentText()
        self.preview_combo.clear()
        locales = list(self.config.get("locales", []))
        for table in self.tables:
            for loc in table.get("locales", []):
                if loc not in locales:
                    locales.append(loc)
        for loc in locales:
            self.preview_combo.addItem(loc)
        target = current or self.config.get("default_locale", "en")
        idx = self.preview_combo.findText(target)
        if idx >= 0:
            self.preview_combo.setCurrentIndex(idx)
        self.preview_combo.blockSignals(False)

    def _on_table_combo_changed(self, index: int):
        if self._suppress_commit:
            return
        if index < 0:
            return
        # Commit the currently displayed table before switching.
        self._commit_grid_to_model()
        self._select_table(index)

    def _select_table(self, index: int):
        self.current_table_index = index
        self._build_grid()
        self._refresh_tag_filter()
        self._apply_filter()

    # ------------------------------------------------------------- grid

    def _build_grid(self):
        table = self._current_table()
        self._loading = True
        self.grid.clear()
        if not table:
            self.grid.setRowCount(0)
            self.grid.setColumnCount(0)
            self._loading = False
            return

        locales = self._display_locales(table)
        headers = list(RESERVED_COLUMNS) + locales
        self.grid.setColumnCount(len(headers))
        self.grid.setHorizontalHeaderLabels(headers)
        self.grid.setRowCount(len(table["entries"]))

        for row, entry in enumerate(table["entries"]):
            self._populate_row(row, entry, locales)

        self.grid.resizeColumnsToContents()
        for col in range(len(headers)):
            if self.grid.columnWidth(col) > 320:
                self.grid.setColumnWidth(col, 320)
        self._loading = False

    def _populate_row(self, row: int, entry: Dict[str, Any], locales: List[str]):
        key_item = QTableWidgetItem(entry.get("key", ""))
        # Stash the per-row plurals dict so it survives renames and edits.
        key_item.setData(Qt.ItemDataRole.UserRole, entry.get("plurals", {}))
        self.grid.setItem(row, 0, key_item)

        self.grid.setItem(row, 1, QTableWidgetItem(";".join(entry.get("tags", []))))
        self.grid.setItem(row, 2, QTableWidgetItem(entry.get("comment", "")))

        for col, loc in enumerate(locales):
            value = entry.get("values", {}).get(loc, "")
            self.grid.setItem(row, len(RESERVED_COLUMNS) + col, QTableWidgetItem(value))

    def _commit_grid_to_model(self):
        """Read every grid row back into the current table model."""
        table = self._current_table()
        if not table:
            return
        locales = self._display_locales(table)
        entries: List[Dict[str, Any]] = []
        seen_locales = set()
        for row in range(self.grid.rowCount()):
            key_item = self.grid.item(row, 0)
            if key_item is None:
                continue
            key = key_item.text().strip()
            if not key:
                continue
            tags_item = self.grid.item(row, 1)
            comment_item = self.grid.item(row, 2)
            plurals = key_item.data(Qt.ItemDataRole.UserRole) or {}
            values: Dict[str, str] = {}
            for col, loc in enumerate(locales):
                cell = self.grid.item(row, len(RESERVED_COLUMNS) + col)
                text = cell.text() if cell else ""
                if text != "":
                    values[loc] = text
                    seen_locales.add(loc)
            entries.append({
                "key": key,
                "tags": [t.strip() for t in (tags_item.text().split(";") if tags_item else []) if t.strip()],
                "comment": comment_item.text() if comment_item else "",
                "values": values,
                "plurals": plurals,
            })
        table["entries"] = entries
        # Keep every displayed locale as part of the table so empty columns persist.
        table["locales"] = list(locales)

    # ---------------------------------------------------------- filtering

    def _refresh_tag_filter(self):
        table = self._current_table()
        self.tag_filter_combo.blockSignals(True)
        current = self.tag_filter_combo.currentData()
        self.tag_filter_combo.clear()
        self.tag_filter_combo.addItem("All Tags", "")
        tags = set()
        if table:
            for entry in table["entries"]:
                for tag in entry.get("tags", []):
                    tags.add(tag)
        for tag in sorted(tags):
            self.tag_filter_combo.addItem(tag, tag)
        if current:
            idx = self.tag_filter_combo.findData(current)
            if idx >= 0:
                self.tag_filter_combo.setCurrentIndex(idx)
        self.tag_filter_combo.blockSignals(False)

    def _apply_filter(self):
        table = self._current_table()
        if not table:
            self.status_label.setText("")
            return
        search = self.search_edit.text().strip().lower()
        tag_filter = self.tag_filter_combo.currentData() or ""
        only_missing = self.missing_check.isChecked()
        locales = self._display_locales(table)
        default_locale = self.config.get("default_locale", "en")

        colors = self._theme_colors()
        missing_brush = QBrush(QColor(colors.warning_color)) if colors else QBrush(QColor("#5a4500"))
        clear_brush = QBrush()

        visible_count = 0
        total_cells = 0
        filled_cells = 0

        # Setting cell backgrounds emits itemChanged; block to avoid re-entrancy.
        self.grid.blockSignals(True)
        for row in range(self.grid.rowCount()):
            key_item = self.grid.item(row, 0)
            if key_item is None:
                continue
            key = key_item.text().lower()
            tags_item = self.grid.item(row, 1)
            tags_text = tags_item.text().lower() if tags_item else ""
            row_tags = [t.strip() for t in (tags_item.text().split(";") if tags_item else []) if t.strip()]

            row_missing = False
            haystack = key + " " + tags_text
            for col, loc in enumerate(locales):
                cell = self.grid.item(row, len(RESERVED_COLUMNS) + col)
                text = cell.text() if cell else ""
                haystack += " " + text.lower()
                total_cells += 1
                if text.strip():
                    filled_cells += 1
                else:
                    row_missing = True
                if cell is not None:
                    cell.setBackground(missing_brush if not text.strip() else clear_brush)

            match = True
            if search and search not in haystack:
                match = False
            if tag_filter and tag_filter not in row_tags:
                match = False
            if only_missing and not row_missing:
                match = False

            self.grid.setRowHidden(row, not match)
            if match:
                visible_count += 1

        self.grid.blockSignals(False)

        pct = (filled_cells * 100 // total_cells) if total_cells else 100
        self.status_label.setText(
            f"{visible_count} keys shown - {filled_cells}/{total_cells} cells translated ({pct}%)")

    # ----------------------------------------------------------- edits

    def _on_item_changed(self, item: QTableWidgetItem):
        if self._loading:
            return
        # Re-evaluate tag list and missing highlight when a cell changes.
        if item.column() == 1:
            self._refresh_tag_filter()
        self._apply_filter()

    def _on_add_key(self):
        table = self._current_table()
        if not table:
            return
        self._commit_grid_to_model()
        base = "new_key"
        existing = {e["key"] for e in table["entries"]}
        name = base
        n = 1
        while name in existing:
            name = f"{base}_{n}"
            n += 1
        table["entries"].append({"key": name, "tags": [], "comment": "", "values": {}, "plurals": {}})
        self._build_grid()
        self._apply_filter()
        # Select and scroll to the new row.
        last = self.grid.rowCount() - 1
        if last >= 0:
            self.grid.setCurrentCell(last, 0)
            self.grid.scrollToItem(self.grid.item(last, 0))

    def _selected_rows(self) -> List[int]:
        rows = sorted({idx.row() for idx in self.grid.selectedIndexes()})
        return rows

    def _on_remove_key(self):
        rows = self._selected_rows()
        if not rows:
            return
        if QMessageBox.question(self, "Remove Key",
                                f"Remove {len(rows)} key(s)?") != QMessageBox.StandardButton.Yes:
            return
        self._commit_grid_to_model()
        table = self._current_table()
        for row in sorted(rows, reverse=True):
            if 0 <= row < len(table["entries"]):
                del table["entries"][row]
        self._build_grid()
        self._refresh_tag_filter()
        self._apply_filter()

    def _on_duplicate_key(self):
        rows = self._selected_rows()
        if not rows:
            return
        self._commit_grid_to_model()
        table = self._current_table()
        existing = {e["key"] for e in table["entries"]}
        for row in rows:
            if 0 <= row < len(table["entries"]):
                src = table["entries"][row]
                new_key = src["key"] + "_copy"
                n = 1
                while new_key in existing:
                    new_key = f"{src['key']}_copy{n}"
                    n += 1
                existing.add(new_key)
                table["entries"].append({
                    "key": new_key,
                    "tags": list(src["tags"]),
                    "comment": src["comment"],
                    "values": dict(src["values"]),
                    "plurals": {loc: dict(f) for loc, f in src["plurals"].items()},
                })
        self._build_grid()
        self._refresh_tag_filter()
        self._apply_filter()

    def _on_edit_plurals(self):
        rows = self._selected_rows()
        if not rows:
            QMessageBox.information(self, "Edit Plurals", "Select a key first.")
            return
        row = rows[0]
        key_item = self.grid.item(row, 0)
        if key_item is None:
            return
        table = self._current_table()
        locales = self._display_locales(table)
        plurals = key_item.data(Qt.ItemDataRole.UserRole) or {}
        dialog = PluralEditorDialog(key_item.text(), locales, plurals, self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            key_item.setData(Qt.ItemDataRole.UserRole, dialog.get_plurals())

    # --------------------------------------------------------- languages

    def _on_add_language(self):
        table = self._current_table()
        if not table:
            return
        existing = self._display_locales(table)
        dialog = AddLanguageDialog(existing, self)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return
        locale = dialog.get_locale().strip()
        if not locale:
            return
        if locale in existing:
            QMessageBox.information(self, "Add Language", f"'{locale}' already exists.")
            return
        self._commit_grid_to_model()
        if locale not in self.config["locales"]:
            self.config["locales"].append(locale)
        if locale not in table["locales"]:
            table["locales"].append(locale)
        self._build_grid()
        self._refresh_preview_combo()
        self._apply_filter()

    def _on_remove_language(self):
        table = self._current_table()
        if not table:
            return
        locales = self._display_locales(table)
        if not locales:
            return
        locale, ok = QInputDialog.getItem(self, "Remove Language",
                                          "Remove which language from this table?",
                                          locales, 0, False)
        if not ok or not locale:
            return
        if QMessageBox.question(
                self, "Remove Language",
                f"Remove all '{locale}' translations from table '{table['name']}'?"
                ) != QMessageBox.StandardButton.Yes:
            return
        self._commit_grid_to_model()
        if locale in table["locales"]:
            table["locales"].remove(locale)
        for entry in table["entries"]:
            entry["values"].pop(locale, None)
            entry["plurals"].pop(locale, None)
        self._build_grid()
        self._apply_filter()

    # ------------------------------------------------------------- tables

    def _on_new_table(self):
        name, ok = QInputDialog.getText(self, "New Table", "Table name:")
        if not ok or not name.strip():
            return
        name = name.strip()
        if any(t["name"] == name for t in self.tables):
            QMessageBox.warning(self, "New Table", f"A table named '{name}' already exists.")
            return
        self._commit_grid_to_model()
        self.tables.append(self._new_table_model(name))
        self._refresh_table_selector()
        self.table_combo.setCurrentIndex(len(self.tables) - 1)

    def _on_delete_table(self):
        table = self._current_table()
        if not table:
            return
        if len(self.tables) <= 1:
            QMessageBox.information(self, "Delete Table", "A project needs at least one table.")
            return
        if QMessageBox.question(
                self, "Delete Table",
                f"Delete table '{table['name']}'? This removes its file on save."
                ) != QMessageBox.StandardButton.Yes:
            return
        if table.get("source_path"):
            self.deleted_paths.append(table["source_path"])
        del self.tables[self.current_table_index]
        self.current_table_index = -1
        self._refresh_table_selector()
        self._select_table(0)

    def _on_import_csv(self):
        table = self._current_table()
        if not table:
            return
        path, _ = QFileDialog.getOpenFileName(self, "Import CSV", self.project_dir,
                                              "CSV Files (*.csv)")
        if not path:
            return
        try:
            imported = self._load_csv_table(Path(path))
        except Exception as exc:
            QMessageBox.critical(self, "Import CSV", f"Failed to read CSV:\n{exc}")
            return
        if QMessageBox.question(
                self, "Import CSV",
                f"Replace the contents of '{table['name']}' with {len(imported['entries'])} "
                f"keys from this CSV?") != QMessageBox.StandardButton.Yes:
            return
        table["entries"] = imported["entries"]
        for loc in imported["locales"]:
            if loc not in table["locales"]:
                table["locales"].append(loc)
            if loc not in self.config["locales"]:
                self.config["locales"].append(loc)
        self._build_grid()
        self._refresh_tag_filter()
        self._refresh_preview_combo()
        self._apply_filter()

    def _on_export_csv(self):
        table = self._current_table()
        if not table:
            return
        self._commit_grid_to_model()
        default_name = f"{table['name']}.csv"
        path, _ = QFileDialog.getSaveFileName(self, "Export CSV",
                                              str(Path(self.project_dir) / default_name),
                                              "CSV Files (*.csv)")
        if not path:
            return
        try:
            with open(path, "w", encoding="utf-8", newline="") as fh:
                self._write_csv(fh, table)
        except Exception as exc:
            QMessageBox.critical(self, "Export CSV", f"Failed to write CSV:\n{exc}")
            return
        QMessageBox.information(self, "Export CSV", f"Exported to:\n{path}")

    # --------------------------------------------------------- preview

    def _on_preview_changed(self, locale: str):
        if not locale or not self.editor_bridge:
            return
        try:
            self.editor_bridge.set_preview_locale(locale)
            self.localization_applied.emit()
        except Exception as exc:
            print(f"Localization: preview failed: {exc}")

    def _on_pseudo_toggled(self, enabled: bool):
        self.config["pseudolocalization"] = enabled
        if self.editor_bridge:
            try:
                if hasattr(self.editor_bridge, "set_pseudolocalization"):
                    self.editor_bridge.set_pseudolocalization(enabled)
                self.localization_applied.emit()
            except Exception:
                pass

    # ------------------------------------------------------------ saving

    def _write_csv(self, fh, table: Dict[str, Any]):
        writer = csv.writer(fh)
        locales = self._display_locales(table)
        writer.writerow(["Key", "Tags", "Comment"] + locales)
        for entry in table["entries"]:
            row = [entry["key"], ";".join(entry.get("tags", [])), entry.get("comment", "")]
            for loc in locales:
                row.append(entry.get("values", {}).get(loc, ""))
            writer.writerow(row)

    def _table_to_json(self, table: Dict[str, Any]) -> Dict[str, Any]:
        obj = {
            "lupine_loctable": 1,
            "name": table["name"],
            "locales": self._display_locales(table),
        }
        entries = []
        for entry in table["entries"]:
            entry_obj: Dict[str, Any] = {"key": entry["key"]}
            if entry.get("tags"):
                entry_obj["tags"] = entry["tags"]
            if entry.get("comment"):
                entry_obj["comment"] = entry["comment"]
            entry_obj["values"] = {k: v for k, v in entry.get("values", {}).items() if v != ""}
            plurals = {loc: forms for loc, forms in entry.get("plurals", {}).items() if forms}
            if plurals:
                entry_obj["plurals"] = plurals
            entries.append(entry_obj)
        obj["entries"] = entries
        return obj

    def _on_save(self):
        self._commit_grid_to_model()

        tdir = self._tables_dir()
        try:
            tdir.mkdir(parents=True, exist_ok=True)
        except Exception as exc:
            QMessageBox.critical(self, "Save", f"Could not create '{tdir}':\n{exc}")
            return

        # Remove tables the user deleted.
        for path in self.deleted_paths:
            try:
                Path(path).unlink(missing_ok=True)
            except Exception:
                pass
        self.deleted_paths = []

        # Roll every table's locales up into the project config.
        for table in self.tables:
            for loc in table.get("locales", []):
                if loc not in self.config["locales"]:
                    self.config["locales"].append(loc)

        try:
            for table in self.tables:
                fmt = table.get("format", "json")
                ext = ".csv" if fmt == "csv" else ".loctable"
                dest = tdir / f"{table['name']}{ext}"
                if fmt == "csv":
                    with open(dest, "w", encoding="utf-8", newline="") as fh:
                        self._write_csv(fh, table)
                else:
                    with open(dest, "w", encoding="utf-8") as fh:
                        json.dump(self._table_to_json(table), fh, indent=2, ensure_ascii=False)
                table["source_path"] = str(dest)
        except Exception as exc:
            QMessageBox.critical(self, "Save", f"Failed to write tables:\n{exc}")
            return

        # Write the project-root localization.json config the engine reads.
        config_out = {
            "lupine_localization": 1,
            "enabled": bool(self.config.get("enabled", True)),
            "default_locale": self.config.get("default_locale", "en"),
            "fallback_locale": self.config.get("fallback_locale", "en"),
            "locales": self.config.get("locales", ["en"]),
            "tables_dir": self.config.get("tables_dir", "localization"),
            "csv_mode": bool(self.config.get("csv_mode", False)),
            "pseudolocalization": bool(self.config.get("pseudolocalization", False)),
        }
        try:
            with open(Path(self.project_dir) / "localization.json", "w", encoding="utf-8") as fh:
                json.dump(config_out, fh, indent=2, ensure_ascii=False)
        except Exception as exc:
            QMessageBox.critical(self, "Save", f"Failed to write localization.json:\n{exc}")
            return

        if self.project is not None:
            self.project.localization = dict(self.config)
        self.localization_changed.emit(dict(self.config))

        # Refresh the editor viewport so Localization Key fields update.
        if self.editor_bridge:
            try:
                self.editor_bridge.reload_localization()
                preview = self.preview_combo.currentText()
                if preview:
                    self.editor_bridge.set_preview_locale(preview)
                self.localization_applied.emit()
            except Exception as exc:
                print(f"Localization: reload failed: {exc}")

        QMessageBox.information(self, "Localization", "Localization saved.")
