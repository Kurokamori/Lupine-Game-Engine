"""
Feature and Bug Tracker Tool
A comprehensive system for tracking features, sub-features, and bugs
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                             QPushButton, QTabWidget, QTreeWidget, QTreeWidgetItem,
                             QDialog, QFormLayout, QLineEdit, QTextEdit, QComboBox,
                             QDialogButtonBox, QMessageBox, QMenu, QSplitter,
                             QGroupBox, QScrollArea, QFrame, QInputDialog, QSpinBox,
                             QCheckBox, QListWidget, QListWidgetItem, QProgressBar)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QAction, QColor, QBrush, QFont
from pathlib import Path
import json
import sys
from datetime import datetime
from typing import List, Dict, Optional, Any

# Import base panel
editor_path = Path(__file__).parent.parent.parent
if str(editor_path) not in sys.path:
    sys.path.insert(0, str(editor_path))

from panels.base_panel import EditorPanel
from theme import get_theme_manager


# ===== DATA MODELS =====

class Bug:
    """Represents a bug"""

    def __init__(self, description: str, priority: str = "Medium",
                 state: str = "Found", notes: str = ""):
        self.description = description
        self.priority = priority
        self.state = state
        self.notes = notes
        self.created_date = datetime.now().isoformat()
        self.modified_date = datetime.now().isoformat()

    def to_dict(self) -> dict:
        return {
            'description': self.description,
            'priority': self.priority,
            'state': self.state,
            'notes': self.notes,
            'created_date': self.created_date,
            'modified_date': self.modified_date
        }

    @staticmethod
    def from_dict(data: dict) -> 'Bug':
        bug = Bug(
            description=data.get('description', ''),
            priority=data.get('priority', 'Medium'),
            state=data.get('state', 'Found'),
            notes=data.get('notes', '')
        )
        bug.created_date = data.get('created_date', datetime.now().isoformat())
        bug.modified_date = data.get('modified_date', datetime.now().isoformat())
        return bug


class SubFeature:
    """Represents a sub-feature"""

    def __init__(self, name: str, description: str = "", priority: str = "Medium",
                 percent_complete: int = 0, state: str = "Planned"):
        self.name = name
        self.description = description
        self.priority = priority
        self.percent_complete = percent_complete
        self.state = state
        self.bugs: List[Bug] = []
        self.created_date = datetime.now().isoformat()
        self.modified_date = datetime.now().isoformat()

    def get_bug_count(self) -> int:
        """Get total bug count"""
        return len(self.bugs)

    def get_open_bug_count(self) -> int:
        """Get count of non-resolved bugs"""
        return len([b for b in self.bugs if b.state != "Resolved"])

    def to_dict(self) -> dict:
        return {
            'name': self.name,
            'description': self.description,
            'priority': self.priority,
            'percent_complete': self.percent_complete,
            'state': self.state,
            'bugs': [bug.to_dict() for bug in self.bugs],
            'created_date': self.created_date,
            'modified_date': self.modified_date
        }

    @staticmethod
    def from_dict(data: dict) -> 'SubFeature':
        sub_feature = SubFeature(
            name=data.get('name', 'Untitled'),
            description=data.get('description', ''),
            priority=data.get('priority', 'Medium'),
            percent_complete=data.get('percent_complete', 0),
            state=data.get('state', 'Planned')
        )
        sub_feature.bugs = [Bug.from_dict(b) for b in data.get('bugs', [])]
        sub_feature.created_date = data.get('created_date', datetime.now().isoformat())
        sub_feature.modified_date = data.get('modified_date', datetime.now().isoformat())
        return sub_feature


class Feature:
    """Represents a feature"""

    def __init__(self, name: str, description: str = "", priority: str = "Medium",
                 percent_complete: int = 0):
        self.name = name
        self.description = description
        self.priority = priority
        self.percent_complete = percent_complete
        self.sub_features: List[SubFeature] = []
        self.bugs: List[Bug] = []
        self.created_date = datetime.now().isoformat()
        self.modified_date = datetime.now().isoformat()

    def get_total_bug_count(self) -> int:
        """Get total bug count including sub-features"""
        total = len(self.bugs)
        for sf in self.sub_features:
            total += sf.get_bug_count()
        return total

    def get_open_bug_count(self) -> int:
        """Get count of non-resolved bugs"""
        total = len([b for b in self.bugs if b.state != "Resolved"])
        for sf in self.sub_features:
            total += sf.get_open_bug_count()
        return total

    def to_dict(self) -> dict:
        return {
            'name': self.name,
            'description': self.description,
            'priority': self.priority,
            'percent_complete': self.percent_complete,
            'sub_features': [sf.to_dict() for sf in self.sub_features],
            'bugs': [bug.to_dict() for bug in self.bugs],
            'created_date': self.created_date,
            'modified_date': self.modified_date
        }

    @staticmethod
    def from_dict(data: dict) -> 'Feature':
        feature = Feature(
            name=data.get('name', 'Untitled'),
            description=data.get('description', ''),
            priority=data.get('priority', 'Medium'),
            percent_complete=data.get('percent_complete', 0)
        )
        feature.sub_features = [SubFeature.from_dict(sf) for sf in data.get('sub_features', [])]
        feature.bugs = [Bug.from_dict(b) for b in data.get('bugs', [])]
        feature.created_date = data.get('created_date', datetime.now().isoformat())
        feature.modified_date = data.get('modified_date', datetime.now().isoformat())
        return feature


class FeatureTracker:
    """Manages features and bugs"""

    def __init__(self, name: str, file_path: Optional[str] = None):
        self.name = name
        self.file_path = file_path
        self.features: List[Feature] = []

        # Get theme colors for default priorities
        theme = get_theme_manager().get_current_theme()
        if theme:
            c = theme.colors
            self.priorities: Dict[str, str] = {
                "Low": c.success_color,
                "Medium": c.warning_color,
                "High": c.error_color
            }
        else:
            self.priorities: Dict[str, str] = {
                "Low": "#4CAF50",
                "Medium": "#FFC107",
                "High": "#F44336"
            }

        # Default bug states
        self.bug_states: List[str] = ["Found", "Replicated", "In Progress", "Needs Testing", "Resolved"]

        # Default sub-feature states
        self.subfeature_states: List[str] = ["Planned", "In Progress", "Testing", "Complete"]

        self.modified = False

    def add_feature(self, feature: Feature):
        """Add a feature"""
        self.features.append(feature)
        self.modified = True

    def remove_feature(self, feature: Feature):
        """Remove a feature"""
        if feature in self.features:
            self.features.remove(feature)
            self.modified = True

    def add_priority(self, priority: str, color: str):
        """Add a priority level"""
        if priority and priority not in self.priorities:
            self.priorities[priority] = color
            self.modified = True

    def remove_priority(self, priority: str):
        """Remove a priority level"""
        if priority in self.priorities and priority not in ["Low", "Medium", "High"]:
            del self.priorities[priority]
            self.modified = True

    def add_bug_state(self, state: str):
        """Add a bug state"""
        if state and state not in self.bug_states:
            self.bug_states.append(state)
            self.modified = True

    def remove_bug_state(self, state: str):
        """Remove a bug state"""
        defaults = ["Found", "Replicated", "In Progress", "Needs Testing", "Resolved"]
        if state in self.bug_states and state not in defaults:
            self.bug_states.remove(state)
            self.modified = True

    def add_subfeature_state(self, state: str):
        """Add a sub-feature state"""
        if state and state not in self.subfeature_states:
            self.subfeature_states.append(state)
            self.modified = True

    def remove_subfeature_state(self, state: str):
        """Remove a sub-feature state"""
        defaults = ["Planned", "In Progress", "Testing", "Complete"]
        if state in self.subfeature_states and state not in defaults:
            self.subfeature_states.remove(state)
            self.modified = True

    def to_dict(self) -> dict:
        return {
            'name': self.name,
            'features': [f.to_dict() for f in self.features],
            'priorities': self.priorities,
            'bug_states': self.bug_states,
            'subfeature_states': self.subfeature_states
        }

    @staticmethod
    def from_dict(data: dict, file_path: Optional[str] = None) -> 'FeatureTracker':
        tracker = FeatureTracker(data.get('name', 'Untitled'), file_path)
        tracker.features = [Feature.from_dict(f) for f in data.get('features', [])]

        # Use saved priorities if available
        if 'priorities' in data:
            tracker.priorities = data['priorities']

        tracker.bug_states = data.get('bug_states', ["Found", "Replicated", "In Progress", "Needs Testing", "Resolved"])
        tracker.subfeature_states = data.get('subfeature_states', ["Planned", "In Progress", "Testing", "Complete"])
        tracker.modified = False
        return tracker

    def save(self) -> bool:
        """Save tracker to file"""
        if not self.file_path:
            return False

        try:
            with open(self.file_path, 'w', encoding='utf-8') as f:
                json.dump(self.to_dict(), f, indent=2)
            self.modified = False
            return True
        except Exception as e:
            print(f"Failed to save feature tracker: {e}")
            return False

    @staticmethod
    def load(file_path: str) -> Optional['FeatureTracker']:
        """Load tracker from file"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
            return FeatureTracker.from_dict(data, file_path)
        except Exception as e:
            print(f"Failed to load feature tracker: {e}")
            return None


# ===== DIALOGS =====

class BugEditDialog(QDialog):
    """Dialog for creating/editing bugs"""

    def __init__(self, bug: Optional[Bug] = None, tracker: Optional[FeatureTracker] = None, parent=None):
        super().__init__(parent)
        self.bug = bug
        self.tracker = tracker
        self.setWindowTitle("Edit Bug" if bug else "New Bug")
        self.setModal(True)
        self.setMinimumWidth(500)

        self.setup_ui()

        if bug:
            self.load_bug_data()

    def setup_ui(self):
        """Setup dialog UI"""
        layout = QVBoxLayout()

        form_layout = QFormLayout()

        # Description
        self.description_edit = QLineEdit()
        self.description_edit.setPlaceholderText("Bug description")
        form_layout.addRow("Description:", self.description_edit)

        # Priority
        self.priority_combo = QComboBox()
        if self.tracker:
            self.priority_combo.addItems(list(self.tracker.priorities.keys()))
        form_layout.addRow("Priority:", self.priority_combo)

        # State
        self.state_combo = QComboBox()
        if self.tracker:
            self.state_combo.addItems(self.tracker.bug_states)
        form_layout.addRow("State:", self.state_combo)

        # Notes
        self.notes_edit = QTextEdit()
        self.notes_edit.setPlaceholderText("Additional notes")
        self.notes_edit.setMaximumHeight(150)
        form_layout.addRow("Notes:", self.notes_edit)

        layout.addLayout(form_layout)

        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self.setLayout(layout)

    def load_bug_data(self):
        """Load bug data into fields"""
        if not self.bug:
            return

        self.description_edit.setText(self.bug.description)

        index = self.priority_combo.findText(self.bug.priority)
        if index >= 0:
            self.priority_combo.setCurrentIndex(index)

        index = self.state_combo.findText(self.bug.state)
        if index >= 0:
            self.state_combo.setCurrentIndex(index)

        self.notes_edit.setPlainText(self.bug.notes)

    def get_bug(self) -> Bug:
        """Get bug from dialog fields"""
        description = self.description_edit.text().strip() or "Untitled Bug"
        priority = self.priority_combo.currentText()
        state = self.state_combo.currentText()
        notes = self.notes_edit.toPlainText().strip()

        if self.bug:
            # Update existing bug
            self.bug.description = description
            self.bug.priority = priority
            self.bug.state = state
            self.bug.notes = notes
            self.bug.modified_date = datetime.now().isoformat()
            return self.bug
        else:
            # Create new bug
            return Bug(
                description=description,
                priority=priority,
                state=state,
                notes=notes
            )


class SubFeatureEditDialog(QDialog):
    """Dialog for creating/editing sub-features"""

    def __init__(self, sub_feature: Optional[SubFeature] = None,
                 tracker: Optional[FeatureTracker] = None, parent=None):
        super().__init__(parent)
        self.sub_feature = sub_feature
        self.tracker = tracker
        self.setWindowTitle("Edit Sub-Feature" if sub_feature else "New Sub-Feature")
        self.setModal(True)
        self.setMinimumWidth(600)

        self.setup_ui()

        if sub_feature:
            self.load_subfeature_data()

    def setup_ui(self):
        """Setup dialog UI"""
        layout = QVBoxLayout()

        form_layout = QFormLayout()

        # Name
        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("Sub-feature name")
        form_layout.addRow("Name:", self.name_edit)

        # Description
        self.description_edit = QTextEdit()
        self.description_edit.setPlaceholderText("Sub-feature description")
        self.description_edit.setMaximumHeight(80)
        form_layout.addRow("Description:", self.description_edit)

        # Priority
        self.priority_combo = QComboBox()
        if self.tracker:
            self.priority_combo.addItems(list(self.tracker.priorities.keys()))
        form_layout.addRow("Priority:", self.priority_combo)

        # State
        self.state_combo = QComboBox()
        if self.tracker:
            self.state_combo.addItems(self.tracker.subfeature_states)
        form_layout.addRow("State:", self.state_combo)

        # Progress
        progress_layout = QHBoxLayout()
        self.progress_spin = QSpinBox()
        self.progress_spin.setRange(0, 100)
        self.progress_spin.setSuffix("%")
        progress_layout.addWidget(self.progress_spin)
        form_layout.addRow("Progress:", progress_layout)

        layout.addLayout(form_layout)

        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self.setLayout(layout)

    def load_subfeature_data(self):
        """Load sub-feature data into fields"""
        if not self.sub_feature:
            return

        self.name_edit.setText(self.sub_feature.name)
        self.description_edit.setPlainText(self.sub_feature.description)

        index = self.priority_combo.findText(self.sub_feature.priority)
        if index >= 0:
            self.priority_combo.setCurrentIndex(index)

        index = self.state_combo.findText(self.sub_feature.state)
        if index >= 0:
            self.state_combo.setCurrentIndex(index)

        self.progress_spin.setValue(self.sub_feature.percent_complete)

    def get_subfeature(self) -> SubFeature:
        """Get sub-feature from dialog fields"""
        name = self.name_edit.text().strip() or "Untitled Sub-Feature"
        description = self.description_edit.toPlainText().strip()
        priority = self.priority_combo.currentText()
        state = self.state_combo.currentText()
        percent_complete = self.progress_spin.value()

        if self.sub_feature:
            # Update existing sub-feature
            self.sub_feature.name = name
            self.sub_feature.description = description
            self.sub_feature.priority = priority
            self.sub_feature.state = state
            self.sub_feature.percent_complete = percent_complete
            self.sub_feature.modified_date = datetime.now().isoformat()
            return self.sub_feature
        else:
            # Create new sub-feature
            return SubFeature(
                name=name,
                description=description,
                priority=priority,
                percent_complete=percent_complete,
                state=state
            )


class FeatureEditDialog(QDialog):
    """Dialog for creating/editing features"""

    def __init__(self, feature: Optional[Feature] = None,
                 tracker: Optional[FeatureTracker] = None, parent=None):
        super().__init__(parent)
        self.feature = feature
        self.tracker = tracker
        self.setWindowTitle("Edit Feature" if feature else "New Feature")
        self.setModal(True)
        self.setMinimumWidth(600)

        self.setup_ui()

        if feature:
            self.load_feature_data()

    def setup_ui(self):
        """Setup dialog UI"""
        layout = QVBoxLayout()

        form_layout = QFormLayout()

        # Name
        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("Feature name")
        form_layout.addRow("Name:", self.name_edit)

        # Description
        self.description_edit = QTextEdit()
        self.description_edit.setPlaceholderText("Feature description")
        self.description_edit.setMaximumHeight(100)
        form_layout.addRow("Description:", self.description_edit)

        # Priority
        self.priority_combo = QComboBox()
        if self.tracker:
            self.priority_combo.addItems(list(self.tracker.priorities.keys()))
        form_layout.addRow("Priority:", self.priority_combo)

        # Progress
        progress_layout = QHBoxLayout()
        self.progress_spin = QSpinBox()
        self.progress_spin.setRange(0, 100)
        self.progress_spin.setSuffix("%")
        progress_layout.addWidget(self.progress_spin)
        form_layout.addRow("Progress:", progress_layout)

        layout.addLayout(form_layout)

        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self.setLayout(layout)

    def load_feature_data(self):
        """Load feature data into fields"""
        if not self.feature:
            return

        self.name_edit.setText(self.feature.name)
        self.description_edit.setPlainText(self.feature.description)

        index = self.priority_combo.findText(self.feature.priority)
        if index >= 0:
            self.priority_combo.setCurrentIndex(index)

        self.progress_spin.setValue(self.feature.percent_complete)

    def get_feature(self) -> Feature:
        """Get feature from dialog fields"""
        name = self.name_edit.text().strip() or "Untitled Feature"
        description = self.description_edit.toPlainText().strip()
        priority = self.priority_combo.currentText()
        percent_complete = self.progress_spin.value()

        if self.feature:
            # Update existing feature
            self.feature.name = name
            self.feature.description = description
            self.feature.priority = priority
            self.feature.percent_complete = percent_complete
            self.feature.modified_date = datetime.now().isoformat()
            return self.feature
        else:
            # Create new feature
            return Feature(
                name=name,
                description=description,
                priority=priority,
                percent_complete=percent_complete
            )


class ManageStatesDialog(QDialog):
    """Dialog for managing custom states"""

    def __init__(self, tracker: FeatureTracker, parent=None):
        super().__init__(parent)
        self.tracker = tracker
        self.setWindowTitle("Manage Custom States")
        self.setModal(True)
        self.setMinimumWidth(500)

        self.setup_ui()

    def setup_ui(self):
        """Setup dialog UI"""
        layout = QVBoxLayout()

        # Tabs for bug states and sub-feature states
        tabs = QTabWidget()

        # Bug states tab
        bug_states_widget = QWidget()
        bug_layout = QVBoxLayout()

        add_bug_layout = QHBoxLayout()
        self.bug_state_input = QLineEdit()
        self.bug_state_input.setPlaceholderText("New bug state")
        add_bug_layout.addWidget(self.bug_state_input)

        add_bug_btn = QPushButton("Add")
        add_bug_btn.clicked.connect(self._add_bug_state)
        add_bug_layout.addWidget(add_bug_btn)

        bug_layout.addLayout(add_bug_layout)

        self.bug_states_list = QListWidget()
        self._load_bug_states()
        bug_layout.addWidget(self.bug_states_list)

        remove_bug_btn = QPushButton("Remove Selected")
        remove_bug_btn.clicked.connect(self._remove_bug_state)
        bug_layout.addWidget(remove_bug_btn)

        bug_states_widget.setLayout(bug_layout)
        tabs.addTab(bug_states_widget, "Bug States")

        # Sub-feature states tab
        sf_states_widget = QWidget()
        sf_layout = QVBoxLayout()

        add_sf_layout = QHBoxLayout()
        self.sf_state_input = QLineEdit()
        self.sf_state_input.setPlaceholderText("New sub-feature state")
        add_sf_layout.addWidget(self.sf_state_input)

        add_sf_btn = QPushButton("Add")
        add_sf_btn.clicked.connect(self._add_sf_state)
        add_sf_layout.addWidget(add_sf_btn)

        sf_layout.addLayout(add_sf_layout)

        self.sf_states_list = QListWidget()
        self._load_sf_states()
        sf_layout.addWidget(self.sf_states_list)

        remove_sf_btn = QPushButton("Remove Selected")
        remove_sf_btn.clicked.connect(self._remove_sf_state)
        sf_layout.addWidget(remove_sf_btn)

        sf_states_widget.setLayout(sf_layout)
        tabs.addTab(sf_states_widget, "Sub-Feature States")

        layout.addWidget(tabs)

        # Close button
        close_btn = QPushButton("Close")
        close_btn.clicked.connect(self.accept)
        layout.addWidget(close_btn)

        self.setLayout(layout)

    def _load_bug_states(self):
        """Load bug states into list"""
        self.bug_states_list.clear()
        for state in self.tracker.bug_states:
            self.bug_states_list.addItem(state)

    def _load_sf_states(self):
        """Load sub-feature states into list"""
        self.sf_states_list.clear()
        for state in self.tracker.subfeature_states:
            self.sf_states_list.addItem(state)

    def _add_bug_state(self):
        """Add a bug state"""
        state = self.bug_state_input.text().strip()
        if state:
            self.tracker.add_bug_state(state)
            self._load_bug_states()
            self.bug_state_input.clear()

    def _remove_bug_state(self):
        """Remove selected bug state"""
        current = self.bug_states_list.currentItem()
        if current:
            state = current.text()
            defaults = ["Found", "Replicated", "In Progress", "Needs Testing", "Resolved"]
            if state in defaults:
                QMessageBox.warning(self, "Cannot Remove", "Cannot remove default bug states.")
                return

            self.tracker.remove_bug_state(state)
            self._load_bug_states()

    def _add_sf_state(self):
        """Add a sub-feature state"""
        state = self.sf_state_input.text().strip()
        if state:
            self.tracker.add_subfeature_state(state)
            self._load_sf_states()
            self.sf_state_input.clear()

    def _remove_sf_state(self):
        """Remove selected sub-feature state"""
        current = self.sf_states_list.currentItem()
        if current:
            state = current.text()
            defaults = ["Planned", "In Progress", "Testing", "Complete"]
            if state in defaults:
                QMessageBox.warning(self, "Cannot Remove", "Cannot remove default sub-feature states.")
                return

            self.tracker.remove_subfeature_state(state)
            self._load_sf_states()


# ===== MAIN PANEL =====

class FeatureBugTrackerTool(EditorPanel):
    """Feature and Bug Tracker tool for production management"""

    def __init__(self, parent=None, project_root=None):
        self.project_root = project_root
        self.tracker: Optional[FeatureTracker] = None
        self.show_resolved_bugs = False

        super().__init__("Feature & Bug Tracker", parent)
        self.setObjectName("FeatureBugTrackerTool")

        # Load session
        if self.project_root:
            self._load_session()

    def _setup_panel(self):
        """Setup tracker panel UI"""
        main_widget = QWidget()
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 0, 0, 0)

        # Toolbar
        toolbar = QHBoxLayout()
        toolbar.setContentsMargins(5, 5, 5, 5)

        new_tracker_btn = QPushButton("New Tracker")
        new_tracker_btn.clicked.connect(self._new_tracker)
        toolbar.addWidget(new_tracker_btn)

        open_tracker_btn = QPushButton("Open Tracker")
        open_tracker_btn.clicked.connect(self._open_tracker)
        toolbar.addWidget(open_tracker_btn)

        save_btn = QPushButton("Save")
        save_btn.clicked.connect(self._save_tracker)
        toolbar.addWidget(save_btn)

        toolbar.addStretch()

        # Manage button
        manage_btn = QPushButton("Manage")
        manage_menu = QMenu()

        manage_states_action = QAction("Custom States", self)
        manage_states_action.triggered.connect(self._manage_states)
        manage_menu.addAction(manage_states_action)

        manage_btn.setMenu(manage_menu)
        toolbar.addWidget(manage_btn)

        main_layout.addLayout(toolbar)

        # Content area
        self.content_stack = QWidget()
        content_layout = QVBoxLayout()
        content_layout.setContentsMargins(5, 5, 5, 5)

        # Empty state
        self.empty_label = QLabel("No tracker loaded. Create or open a tracker to get started.")
        self.empty_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        content_layout.addWidget(self.empty_label)

        # Tracker content (initially hidden)
        self.tracker_widget = QWidget()
        self.tracker_widget.setVisible(False)
        tracker_layout = QVBoxLayout()
        tracker_layout.setContentsMargins(0, 0, 0, 0)

        # Split between overview and detail
        splitter = QSplitter(Qt.Orientation.Horizontal)

        # Overview panel
        overview_widget = self._create_overview_widget()
        splitter.addWidget(overview_widget)

        # Detail panel
        detail_widget = self._create_detail_widget()
        splitter.addWidget(detail_widget)

        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 2)

        tracker_layout.addWidget(splitter)
        self.tracker_widget.setLayout(tracker_layout)
        content_layout.addWidget(self.tracker_widget)

        self.content_stack.setLayout(content_layout)
        main_layout.addWidget(self.content_stack)

        main_widget.setLayout(main_layout)
        self.content_layout.addWidget(main_widget)

    def _create_overview_widget(self) -> QWidget:
        """Create the overview panel"""
        widget = QWidget()
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)

        # Header
        header = QLabel("Features")
        header_font = QFont()
        header_font.setPointSize(10)
        header_font.setBold(True)
        header.setFont(header_font)
        layout.addWidget(header)

        # Feature controls
        controls = QHBoxLayout()

        add_feature_btn = QPushButton("Add Feature")
        add_feature_btn.clicked.connect(self._add_feature)
        controls.addWidget(add_feature_btn)

        edit_feature_btn = QPushButton("Edit")
        edit_feature_btn.clicked.connect(self._edit_selected)
        controls.addWidget(edit_feature_btn)

        delete_btn = QPushButton("Delete")
        delete_btn.clicked.connect(self._delete_selected)
        controls.addWidget(delete_btn)

        layout.addLayout(controls)

        # Feature tree
        self.feature_tree = QTreeWidget()
        self.feature_tree.setHeaderLabels(["Name", "Progress", "Bugs"])
        self.feature_tree.setColumnWidth(0, 200)
        self.feature_tree.setColumnWidth(1, 80)
        self.feature_tree.itemSelectionChanged.connect(self._on_feature_selected)
        self.feature_tree.itemDoubleClicked.connect(self._on_item_double_clicked)

        # Get theme colors for styling
        theme = get_theme_manager().get_current_theme()
        if theme:
            c = theme.colors
            hover_bg = c.surface_hover
            select_bg = c.selection
        else:
            hover_bg = "#28233a"
            select_bg = "#4a3f68"

        self.feature_tree.setStyleSheet(f"""
            QTreeWidget::item:hover {{
                background-color: {hover_bg};
            }}
            QTreeWidget::item:selected {{
                background-color: {select_bg};
            }}
        """)

        layout.addWidget(self.feature_tree)

        widget.setLayout(layout)
        return widget

    def _create_detail_widget(self) -> QWidget:
        """Create the detail panel"""
        widget = QWidget()
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)

        # Header
        header_layout = QHBoxLayout()
        self.detail_header = QLabel("Select a feature or sub-feature to view details")
        header_font = QFont()
        header_font.setPointSize(10)
        header_font.setBold(True)
        self.detail_header.setFont(header_font)
        header_layout.addWidget(self.detail_header)
        header_layout.addStretch()

        layout.addLayout(header_layout)

        # Scroll area for details
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)

        self.detail_content = QWidget()
        self.detail_layout = QVBoxLayout()
        self.detail_layout.setContentsMargins(5, 5, 5, 5)

        # Placeholder
        placeholder = QLabel("No selection")
        placeholder.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.detail_layout.addWidget(placeholder)

        self.detail_content.setLayout(self.detail_layout)
        scroll.setWidget(self.detail_content)
        layout.addWidget(scroll)

        widget.setLayout(layout)
        return widget

    def _refresh_overview(self):
        """Refresh the feature overview tree"""
        if not self.tracker:
            return

        self.feature_tree.clear()

        for feature in self.tracker.features:
            feature_item = self._create_feature_item(feature)
            self.feature_tree.addTopLevelItem(feature_item)

            # Add sub-features
            for sub_feature in feature.sub_features:
                sf_item = self._create_subfeature_item(sub_feature)
                feature_item.addChild(sf_item)

            feature_item.setExpanded(True)

    def _create_feature_item(self, feature: Feature) -> QTreeWidgetItem:
        """Create tree item for a feature"""
        item = QTreeWidgetItem()
        item.setText(0, feature.name)
        item.setText(1, f"{feature.percent_complete}%")

        # Bug count (open/total)
        open_bugs = feature.get_open_bug_count()
        total_bugs = feature.get_total_bug_count()
        item.setText(2, f"{open_bugs}/{total_bugs}")

        # Store reference
        item.setData(0, Qt.ItemDataRole.UserRole, feature)
        item.setData(0, Qt.ItemDataRole.UserRole + 1, "feature")

        # Color by priority
        if self.tracker and feature.priority in self.tracker.priorities:
            color = QColor(self.tracker.priorities[feature.priority])
            item.setForeground(0, QBrush(color))

        return item

    def _create_subfeature_item(self, sub_feature: SubFeature) -> QTreeWidgetItem:
        """Create tree item for a sub-feature"""
        item = QTreeWidgetItem()
        item.setText(0, f"  └─ {sub_feature.name}")
        item.setText(1, f"{sub_feature.percent_complete}%")

        # Bug count (open/total)
        open_bugs = sub_feature.get_open_bug_count()
        total_bugs = sub_feature.get_bug_count()
        item.setText(2, f"{open_bugs}/{total_bugs}")

        # Store reference
        item.setData(0, Qt.ItemDataRole.UserRole, sub_feature)
        item.setData(0, Qt.ItemDataRole.UserRole + 1, "subfeature")

        # Color by priority
        if self.tracker and sub_feature.priority in self.tracker.priorities:
            color = QColor(self.tracker.priorities[sub_feature.priority])
            item.setForeground(0, QBrush(color))

        return item

    def _on_feature_selected(self):
        """Handle feature selection"""
        current = self.feature_tree.currentItem()
        if not current:
            return

        item_type = current.data(0, Qt.ItemDataRole.UserRole + 1)
        data = current.data(0, Qt.ItemDataRole.UserRole)

        if item_type == "feature":
            self._show_feature_detail(data)
        elif item_type == "subfeature":
            self._show_subfeature_detail(data)

    def _on_item_double_clicked(self, item: QTreeWidgetItem, column: int):
        """Handle double click on item"""
        self._edit_selected()

    def _show_feature_detail(self, feature: Feature):
        """Show feature details"""
        # Clear existing detail layout
        while self.detail_layout.count():
            child = self.detail_layout.takeAt(0)
            if child.widget():
                child.widget().deleteLater()

        self.detail_header.setText(f"Feature: {feature.name}")

        # Description
        desc_group = QGroupBox("Description")
        desc_layout = QVBoxLayout()
        desc_label = QLabel(feature.description or "No description")
        desc_label.setWordWrap(True)
        desc_layout.addWidget(desc_label)
        desc_group.setLayout(desc_layout)
        self.detail_layout.addWidget(desc_group)

        # Info
        info_group = QGroupBox("Information")
        info_layout = QFormLayout()
        info_layout.addRow("Priority:", QLabel(feature.priority))
        info_layout.addRow("Progress:", QLabel(f"{feature.percent_complete}%"))
        info_layout.addRow("Total Bugs:", QLabel(str(feature.get_total_bug_count())))
        info_layout.addRow("Open Bugs:", QLabel(str(feature.get_open_bug_count())))
        info_group.setLayout(info_layout)
        self.detail_layout.addWidget(info_group)

        # Sub-features
        sf_group = QGroupBox(f"Sub-Features ({len(feature.sub_features)})")
        sf_layout = QVBoxLayout()

        sf_controls = QHBoxLayout()
        add_sf_btn = QPushButton("Add Sub-Feature")
        add_sf_btn.clicked.connect(lambda: self._add_subfeature(feature))
        sf_controls.addWidget(add_sf_btn)
        sf_layout.addLayout(sf_controls)

        if feature.sub_features:
            for sf in feature.sub_features:
                sf_item = QLabel(f"• {sf.name} ({sf.state}) - {sf.percent_complete}% - {sf.get_open_bug_count()}/{sf.get_bug_count()} bugs")
                sf_layout.addWidget(sf_item)
        else:
            sf_layout.addWidget(QLabel("No sub-features"))

        sf_group.setLayout(sf_layout)
        self.detail_layout.addWidget(sf_group)

        # Bugs
        self._add_bug_section(self.detail_layout, feature, feature.bugs, "Feature Bugs")

        self.detail_layout.addStretch()

    def _show_subfeature_detail(self, sub_feature: SubFeature):
        """Show sub-feature details"""
        # Clear existing detail layout
        while self.detail_layout.count():
            child = self.detail_layout.takeAt(0)
            if child.widget():
                child.widget().deleteLater()

        self.detail_header.setText(f"Sub-Feature: {sub_feature.name}")

        # Description
        desc_group = QGroupBox("Description")
        desc_layout = QVBoxLayout()
        desc_label = QLabel(sub_feature.description or "No description")
        desc_label.setWordWrap(True)
        desc_layout.addWidget(desc_label)
        desc_group.setLayout(desc_layout)
        self.detail_layout.addWidget(desc_group)

        # Info
        info_group = QGroupBox("Information")
        info_layout = QFormLayout()
        info_layout.addRow("Priority:", QLabel(sub_feature.priority))
        info_layout.addRow("State:", QLabel(sub_feature.state))
        info_layout.addRow("Progress:", QLabel(f"{sub_feature.percent_complete}%"))
        info_layout.addRow("Total Bugs:", QLabel(str(sub_feature.get_bug_count())))
        info_layout.addRow("Open Bugs:", QLabel(str(sub_feature.get_open_bug_count())))
        info_group.setLayout(info_layout)
        self.detail_layout.addWidget(info_group)

        # Bugs
        self._add_bug_section(self.detail_layout, sub_feature, sub_feature.bugs, "Sub-Feature Bugs")

        self.detail_layout.addStretch()

    def _add_bug_section(self, parent_layout, parent_obj, bugs: List[Bug], title: str):
        """Add bug section to detail panel"""
        bug_group = QGroupBox(f"{title} ({len(bugs)})")
        bug_layout = QVBoxLayout()

        # Controls
        bug_controls = QHBoxLayout()
        add_bug_btn = QPushButton("Add Bug")
        add_bug_btn.clicked.connect(lambda: self._add_bug(parent_obj))
        bug_controls.addWidget(add_bug_btn)

        # Toggle resolved bugs
        toggle_resolved_btn = QPushButton("Show Resolved" if not self.show_resolved_bugs else "Hide Resolved")
        toggle_resolved_btn.clicked.connect(self._toggle_resolved_bugs)
        bug_controls.addWidget(toggle_resolved_btn)

        bug_layout.addLayout(bug_controls)

        # Open bugs
        open_bugs = [b for b in bugs if b.state != "Resolved"]
        if open_bugs:
            for bug in open_bugs:
                bug_widget = self._create_bug_widget(bug, parent_obj)
                bug_layout.addWidget(bug_widget)
        else:
            bug_layout.addWidget(QLabel("No open bugs"))

        # Resolved bugs (collapsible)
        resolved_bugs = [b for b in bugs if b.state == "Resolved"]
        if resolved_bugs and self.show_resolved_bugs:
            resolved_label = QLabel(f"Resolved ({len(resolved_bugs)})")
            resolved_font = QFont()
            resolved_font.setBold(True)
            resolved_label.setFont(resolved_font)
            bug_layout.addWidget(resolved_label)

            for bug in resolved_bugs:
                bug_widget = self._create_bug_widget(bug, parent_obj)
                bug_layout.addWidget(bug_widget)

        bug_group.setLayout(bug_layout)
        parent_layout.addWidget(bug_group)

    def _create_bug_widget(self, bug: Bug, parent_obj) -> QWidget:
        """Create widget for a bug"""
        widget = QFrame()
        widget.setFrameShape(QFrame.Shape.StyledPanel)
        layout = QHBoxLayout()
        layout.setContentsMargins(5, 5, 5, 5)

        # Bug info
        info_layout = QVBoxLayout()

        desc_label = QLabel(bug.description)
        desc_font = QFont()
        desc_font.setBold(True)
        desc_label.setFont(desc_font)
        info_layout.addWidget(desc_label)

        state_priority_label = QLabel(f"{bug.state} | Priority: {bug.priority}")
        info_layout.addWidget(state_priority_label)

        if bug.notes:
            notes_label = QLabel(f"Notes: {bug.notes}")
            notes_label.setWordWrap(True)
            info_layout.addWidget(notes_label)

        layout.addLayout(info_layout)
        layout.addStretch()

        # Actions
        edit_btn = QPushButton("Edit")
        edit_btn.setFixedWidth(60)
        edit_btn.clicked.connect(lambda: self._edit_bug(bug, parent_obj))
        layout.addWidget(edit_btn)

        delete_btn = QPushButton("Delete")
        delete_btn.setFixedWidth(60)
        delete_btn.clicked.connect(lambda: self._delete_bug(bug, parent_obj))
        layout.addWidget(delete_btn)

        # Color by priority
        if self.tracker and bug.priority in self.tracker.priorities:
            color = self.tracker.priorities[bug.priority]
            widget.setStyleSheet(f"QFrame {{ border-left: 3px solid {color}; }}")

        widget.setLayout(layout)
        return widget

    def _toggle_resolved_bugs(self):
        """Toggle showing resolved bugs"""
        self.show_resolved_bugs = not self.show_resolved_bugs
        self._on_feature_selected()  # Refresh detail view

    def _new_tracker(self):
        """Create a new tracker"""
        name, ok = QInputDialog.getText(self, "New Tracker", "Tracker name:")
        if not ok or not name:
            return

        # Determine file path
        if self.project_root:
            tracker_dir = Path(self.project_root) / "production" / "trackers"
            tracker_dir.mkdir(parents=True, exist_ok=True)
            file_path = tracker_dir / f"{name}.json"
        else:
            from PyQt6.QtWidgets import QFileDialog
            file_path, _ = QFileDialog.getSaveFileName(
                self,
                "Save Tracker",
                f"{name}.json",
                "JSON Files (*.json)"
            )
            if not file_path:
                return

        # Create tracker
        self.tracker = FeatureTracker(name, str(file_path))

        # Save immediately
        if not self.tracker.save():
            QMessageBox.warning(self, "Error", "Failed to create tracker.")
            return

        # Show tracker UI
        self.empty_label.setVisible(False)
        self.tracker_widget.setVisible(True)
        self._refresh_overview()

        # Save session
        self._save_session()

    def _open_tracker(self):
        """Open an existing tracker"""
        from PyQt6.QtWidgets import QFileDialog
        start_dir = str(Path(self.project_root) / "production" / "trackers") if self.project_root else ""

        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Open Tracker",
            start_dir,
            "JSON Files (*.json)"
        )

        if not file_path:
            return

        # Load tracker
        self.tracker = FeatureTracker.load(file_path)
        if not self.tracker:
            QMessageBox.warning(self, "Error", "Failed to load tracker.")
            return

        # Show tracker UI
        self.empty_label.setVisible(False)
        self.tracker_widget.setVisible(True)
        self._refresh_overview()

        # Save session
        self._save_session()

    def _save_tracker(self):
        """Save the current tracker"""
        if not self.tracker:
            QMessageBox.warning(self, "No Tracker", "No tracker is currently loaded.")
            return

        if self.tracker.save():
            QMessageBox.information(self, "Saved", f"Tracker '{self.tracker.name}' saved successfully.")
        else:
            QMessageBox.warning(self, "Error", "Failed to save tracker.")

    def _add_feature(self):
        """Add a new feature"""
        if not self.tracker:
            return

        dialog = FeatureEditDialog(None, self.tracker, self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            feature = dialog.get_feature()
            self.tracker.add_feature(feature)
            self._refresh_overview()
            self.tracker.save()

    def _add_subfeature(self, feature: Feature):
        """Add a sub-feature to a feature"""
        if not self.tracker:
            return

        dialog = SubFeatureEditDialog(None, self.tracker, self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            sub_feature = dialog.get_subfeature()
            feature.sub_features.append(sub_feature)
            feature.modified_date = datetime.now().isoformat()
            self.tracker.modified = True
            self._refresh_overview()
            self._show_feature_detail(feature)
            self.tracker.save()

    def _add_bug(self, parent_obj):
        """Add a bug to a feature or sub-feature"""
        if not self.tracker:
            return

        dialog = BugEditDialog(None, self.tracker, self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            bug = dialog.get_bug()
            parent_obj.bugs.append(bug)
            parent_obj.modified_date = datetime.now().isoformat()
            self.tracker.modified = True
            self._refresh_overview()

            # Refresh detail view
            if isinstance(parent_obj, Feature):
                self._show_feature_detail(parent_obj)
            else:
                self._show_subfeature_detail(parent_obj)

            self.tracker.save()

    def _edit_selected(self):
        """Edit the selected item"""
        current = self.feature_tree.currentItem()
        if not current:
            return

        item_type = current.data(0, Qt.ItemDataRole.UserRole + 1)
        data = current.data(0, Qt.ItemDataRole.UserRole)

        if item_type == "feature":
            dialog = FeatureEditDialog(data, self.tracker, self)
            if dialog.exec() == QDialog.DialogCode.Accepted:
                self.tracker.modified = True
                self._refresh_overview()
                self._show_feature_detail(data)
                self.tracker.save()
        elif item_type == "subfeature":
            dialog = SubFeatureEditDialog(data, self.tracker, self)
            if dialog.exec() == QDialog.DialogCode.Accepted:
                self.tracker.modified = True
                self._refresh_overview()
                self._show_subfeature_detail(data)
                self.tracker.save()

    def _edit_bug(self, bug: Bug, parent_obj):
        """Edit a bug"""
        if not self.tracker:
            return

        dialog = BugEditDialog(bug, self.tracker, self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            self.tracker.modified = True
            self._refresh_overview()

            # Refresh detail view
            if isinstance(parent_obj, Feature):
                self._show_feature_detail(parent_obj)
            else:
                self._show_subfeature_detail(parent_obj)

            self.tracker.save()

    def _delete_selected(self):
        """Delete the selected item"""
        current = self.feature_tree.currentItem()
        if not current:
            return

        item_type = current.data(0, Qt.ItemDataRole.UserRole + 1)
        data = current.data(0, Qt.ItemDataRole.UserRole)

        if item_type == "feature":
            reply = QMessageBox.question(
                self,
                "Confirm Deletion",
                f"Delete feature '{data.name}' and all its sub-features and bugs?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )

            if reply == QMessageBox.StandardButton.Yes:
                self.tracker.remove_feature(data)
                self._refresh_overview()
                self.tracker.save()

        elif item_type == "subfeature":
            # Find parent feature
            parent_item = current.parent()
            if parent_item:
                parent_feature = parent_item.data(0, Qt.ItemDataRole.UserRole)

                reply = QMessageBox.question(
                    self,
                    "Confirm Deletion",
                    f"Delete sub-feature '{data.name}' and all its bugs?",
                    QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
                )

                if reply == QMessageBox.StandardButton.Yes:
                    parent_feature.sub_features.remove(data)
                    parent_feature.modified_date = datetime.now().isoformat()
                    self.tracker.modified = True
                    self._refresh_overview()
                    self.tracker.save()

    def _delete_bug(self, bug: Bug, parent_obj):
        """Delete a bug"""
        reply = QMessageBox.question(
            self,
            "Confirm Deletion",
            f"Delete bug '{bug.description}'?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )

        if reply == QMessageBox.StandardButton.Yes:
            parent_obj.bugs.remove(bug)
            parent_obj.modified_date = datetime.now().isoformat()
            self.tracker.modified = True
            self._refresh_overview()

            # Refresh detail view
            if isinstance(parent_obj, Feature):
                self._show_feature_detail(parent_obj)
            else:
                self._show_subfeature_detail(parent_obj)

            self.tracker.save()

    def _manage_states(self):
        """Manage custom states"""
        if not self.tracker:
            QMessageBox.warning(self, "No Tracker", "No tracker is currently loaded.")
            return

        dialog = ManageStatesDialog(self.tracker, self)
        dialog.exec()

        if self.tracker.modified:
            self.tracker.save()

    def _save_session(self):
        """Save the current session"""
        if not self.project_root or not self.tracker:
            return

        session_data = {
            'tracker_path': self.tracker.file_path
        }

        try:
            session_file = Path(self.project_root) / "production" / "tracker_session.json"
            session_file.parent.mkdir(parents=True, exist_ok=True)

            with open(session_file, 'w', encoding='utf-8') as f:
                json.dump(session_data, f, indent=2)
        except Exception as e:
            print(f"Failed to save tracker session: {e}")

    def _load_session(self):
        """Load the previous session"""
        if not self.project_root:
            return

        try:
            session_file = Path(self.project_root) / "production" / "tracker_session.json"

            if session_file.exists():
                with open(session_file, 'r', encoding='utf-8') as f:
                    session_data = json.load(f)

                tracker_path = session_data.get('tracker_path')
                if tracker_path and Path(tracker_path).exists():
                    self.tracker = FeatureTracker.load(tracker_path)
                    if self.tracker:
                        self.empty_label.setVisible(False)
                        self.tracker_widget.setVisible(True)
                        self._refresh_overview()
        except Exception as e:
            print(f"Failed to load tracker session: {e}")
