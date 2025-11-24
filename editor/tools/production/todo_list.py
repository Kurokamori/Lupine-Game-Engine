"""
To Do List Tool
A comprehensive task management system for production tracking
Supports multiple lists, categories, priorities, and file linking
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                             QPushButton, QTabWidget, QTreeWidget, QTreeWidgetItem,
                             QDialog, QFormLayout, QLineEdit, QTextEdit, QComboBox,
                             QDateEdit, QSpinBox, QCheckBox, QDialogButtonBox,
                             QMessageBox, QMenu, QSplitter, QListWidget, QListWidgetItem,
                             QToolButton, QFileDialog, QGroupBox, QScrollArea,
                             QFrame, QInputDialog)
from PyQt6.QtCore import Qt, pyqtSignal, QDate, QTimer
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

class TaskStep:
    """Represents a step within a task"""

    def __init__(self, description: str, is_subtask: bool = True, completed: bool = False, current_count: int = 0, target_count: int = 1):
        self.description = description
        self.is_subtask = is_subtask  # True = checkbox subtask, False = counter
        self.completed = completed if is_subtask else False
        self.current_count = current_count if not is_subtask else 0
        self.target_count = target_count if not is_subtask else 1

    def to_dict(self) -> dict:
        return {
            'description': self.description,
            'is_subtask': self.is_subtask,
            'completed': self.completed,
            'current_count': self.current_count,
            'target_count': self.target_count
        }

    @staticmethod
    def from_dict(data: dict) -> 'TaskStep':
        return TaskStep(
            description=data.get('description', ''),
            is_subtask=data.get('is_subtask', True),
            completed=data.get('completed', False),
            current_count=data.get('current_count', 0),
            target_count=data.get('target_count', 1)
        )


class Task:
    """Represents a single task"""

    def __init__(self, title: str, description: str = "", category: str = "General",
                 priority: str = "Medium", status: str = "Pending",
                 due_date: Optional[str] = None, assignees: List[str] = None,
                 steps: List[TaskStep] = None, linked_files: List[str] = None):
        self.title = title
        self.description = description
        self.category = category
        self.priority = priority
        self.status = status
        self.due_date = due_date  # ISO format: YYYY-MM-DD
        self.assignees = assignees or []
        self.steps = steps or []
        self.linked_files = linked_files or []
        self.created_date = datetime.now().isoformat()
        self.modified_date = datetime.now().isoformat()

    def to_dict(self) -> dict:
        return {
            'title': self.title,
            'description': self.description,
            'category': self.category,
            'priority': self.priority,
            'status': self.status,
            'due_date': self.due_date,
            'assignees': self.assignees,
            'steps': [step.to_dict() for step in self.steps],
            'linked_files': self.linked_files,
            'created_date': self.created_date,
            'modified_date': self.modified_date
        }

    @staticmethod
    def from_dict(data: dict) -> 'Task':
        task = Task(
            title=data.get('title', 'Untitled'),
            description=data.get('description', ''),
            category=data.get('category', 'General'),
            priority=data.get('priority', 'Medium'),
            status=data.get('status', 'Pending'),
            due_date=data.get('due_date'),
            assignees=data.get('assignees', []),
            steps=[TaskStep.from_dict(s) for s in data.get('steps', [])],
            linked_files=data.get('linked_files', [])
        )
        task.created_date = data.get('created_date', datetime.now().isoformat())
        task.modified_date = data.get('modified_date', datetime.now().isoformat())
        return task


class TodoList:
    """Represents a todo list containing multiple tasks"""

    def __init__(self, name: str, file_path: Optional[str] = None):
        self.name = name
        self.file_path = file_path
        self.tasks: List[Task] = []
        self.categories: List[str] = ["General"]

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

        self.statuses: List[str] = ["Pending", "In Progress", "Awaiting Testing", "Awaiting Confirmation", "Completed"]
        self.modified = False

    def add_task(self, task: Task):
        """Add a task to the list"""
        self.tasks.append(task)
        if task.category not in self.categories:
            self.categories.append(task.category)
        self.modified = True

    def remove_task(self, task: Task):
        """Remove a task from the list"""
        if task in self.tasks:
            self.tasks.remove(task)
            self.modified = True

    def add_category(self, category: str):
        """Add a new category"""
        if category and category not in self.categories:
            self.categories.append(category)
            self.modified = True

    def remove_category(self, category: str):
        """Remove a category"""
        if category in self.categories and category != "General":
            self.categories.remove(category)
            # Update tasks with this category
            for task in self.tasks:
                if task.category == category:
                    task.category = "General"
            self.modified = True

    def add_priority(self, priority: str, color: str):
        """Add a new priority level"""
        if priority and priority not in self.priorities:
            self.priorities[priority] = color
            self.modified = True

    def remove_priority(self, priority: str):
        """Remove a priority level"""
        if priority in self.priorities and priority not in ["Low", "Medium", "High"]:
            del self.priorities[priority]
            # Update tasks with this priority
            for task in self.tasks:
                if task.priority == priority:
                    task.priority = "Medium"
            self.modified = True

    def add_status(self, status: str):
        """Add a new status"""
        if status and status not in self.statuses:
            self.statuses.append(status)
            self.modified = True

    def remove_status(self, status: str):
        """Remove a status"""
        default_statuses = ["Pending", "In Progress", "Awaiting Testing", "Awaiting Confirmation", "Completed"]
        if status in self.statuses and status not in default_statuses:
            self.statuses.remove(status)
            # Update tasks with this status
            for task in self.tasks:
                if task.status == status:
                    task.status = "Pending"
            self.modified = True

    def to_dict(self) -> dict:
        return {
            'name': self.name,
            'tasks': [task.to_dict() for task in self.tasks],
            'categories': self.categories,
            'priorities': self.priorities,
            'statuses': self.statuses
        }

    @staticmethod
    def from_dict(data: dict, file_path: Optional[str] = None) -> 'TodoList':
        todo_list = TodoList(data.get('name', 'Untitled'), file_path)
        todo_list.tasks = [Task.from_dict(t) for t in data.get('tasks', [])]
        todo_list.categories = data.get('categories', ["General"])

        # Use saved priorities if available, otherwise use theme defaults
        if 'priorities' in data:
            todo_list.priorities = data['priorities']
        # priorities already set by __init__ using theme colors

        todo_list.statuses = data.get('statuses', ["Pending", "In Progress", "Awaiting Testing", "Awaiting Confirmation", "Completed"])
        todo_list.modified = False
        return todo_list

    def save(self) -> bool:
        """Save the list to file"""
        if not self.file_path:
            return False

        try:
            with open(self.file_path, 'w', encoding='utf-8') as f:
                json.dump(self.to_dict(), f, indent=2)
            self.modified = False
            return True
        except Exception as e:
            print(f"Failed to save todo list: {e}")
            return False

    @staticmethod
    def load(file_path: str) -> Optional['TodoList']:
        """Load a list from file"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
            return TodoList.from_dict(data, file_path)
        except Exception as e:
            print(f"Failed to load todo list: {e}")
            return None


# ===== DIALOGS =====

class TaskEditDialog(QDialog):
    """Dialog for creating/editing tasks"""

    def __init__(self, task: Optional[Task] = None, todo_list: Optional[TodoList] = None,
                 project_root: Optional[str] = None, parent=None):
        super().__init__(parent)
        self.task = task
        self.todo_list = todo_list
        self.project_root = project_root
        self.setWindowTitle("Edit Task" if task else "New Task")
        self.setModal(True)
        self.setMinimumWidth(700)
        self.setMinimumHeight(600)

        self.setup_ui()

        if task:
            self.load_task_data()

    def setup_ui(self):
        """Setup dialog UI"""
        layout = QVBoxLayout()

        # Scroll area for form
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)

        form_widget = QWidget()
        form_layout = QFormLayout()
        form_layout.setSpacing(10)

        # Title
        self.title_edit = QLineEdit()
        self.title_edit.setPlaceholderText("Task title")
        form_layout.addRow("Title:", self.title_edit)

        # Description
        self.description_edit = QTextEdit()
        self.description_edit.setPlaceholderText("Task description (optional)")
        self.description_edit.setMaximumHeight(100)
        form_layout.addRow("Description:", self.description_edit)

        # Category
        category_layout = QHBoxLayout()
        self.category_combo = QComboBox()
        if self.todo_list:
            self.category_combo.addItems(self.todo_list.categories)
        category_layout.addWidget(self.category_combo)

        add_category_btn = QPushButton("+")
        add_category_btn.setFixedWidth(30)
        add_category_btn.setToolTip("Add new category")
        add_category_btn.clicked.connect(self._add_category)
        category_layout.addWidget(add_category_btn)

        form_layout.addRow("Category:", category_layout)

        # Priority
        self.priority_combo = QComboBox()
        if self.todo_list:
            self.priority_combo.addItems(list(self.todo_list.priorities.keys()))
        form_layout.addRow("Priority:", self.priority_combo)

        # Status
        self.status_combo = QComboBox()
        if self.todo_list:
            self.status_combo.addItems(self.todo_list.statuses)
        form_layout.addRow("Status:", self.status_combo)

        # Due Date
        due_date_layout = QHBoxLayout()
        self.due_date_check = QCheckBox("Set due date")
        self.due_date_check.toggled.connect(self._toggle_due_date)
        due_date_layout.addWidget(self.due_date_check)

        self.due_date_edit = QDateEdit()
        self.due_date_edit.setCalendarPopup(True)
        self.due_date_edit.setDate(QDate.currentDate())
        self.due_date_edit.setEnabled(False)
        due_date_layout.addWidget(self.due_date_edit)

        form_layout.addRow("Due Date:", due_date_layout)

        # Assignees
        assignee_layout = QVBoxLayout()

        assignee_controls = QHBoxLayout()
        self.assignee_input = QLineEdit()
        self.assignee_input.setPlaceholderText("Enter assignee name")
        assignee_controls.addWidget(self.assignee_input)

        add_assignee_btn = QPushButton("Add")
        add_assignee_btn.clicked.connect(self._add_assignee)
        assignee_controls.addWidget(add_assignee_btn)

        assignee_layout.addLayout(assignee_controls)

        self.assignee_list = QListWidget()
        self.assignee_list.setMaximumHeight(80)
        assignee_layout.addWidget(self.assignee_list)

        remove_assignee_btn = QPushButton("Remove Selected")
        remove_assignee_btn.clicked.connect(self._remove_assignee)
        assignee_layout.addWidget(remove_assignee_btn)

        form_layout.addRow("Assignees:", assignee_layout)

        # Steps
        steps_group = QGroupBox("Steps")
        steps_layout = QVBoxLayout()

        steps_controls = QHBoxLayout()
        self.step_type_combo = QComboBox()
        self.step_type_combo.addItems(["Subtask (checkbox)", "Counter"])
        steps_controls.addWidget(self.step_type_combo)

        self.step_input = QLineEdit()
        self.step_input.setPlaceholderText("Step description")
        steps_controls.addWidget(self.step_input)

        add_step_btn = QPushButton("Add")
        add_step_btn.clicked.connect(self._add_step)
        steps_controls.addWidget(add_step_btn)

        steps_layout.addLayout(steps_controls)

        self.steps_tree = QTreeWidget()
        self.steps_tree.setHeaderLabels(["Description", "Type", "Progress"])
        self.steps_tree.setMaximumHeight(150)
        self.steps_tree.itemClicked.connect(self._on_step_clicked)

        # Style checkboxes with theme colors
        theme = get_theme_manager().get_current_theme()
        if theme:
            c = theme.colors
            accent = c.accent_color
            surface = c.surface
            border = c.border
        else:
            accent = "#7b5fb8"
            surface = "#1e1a28"
            border = "#3d3650"

        self.steps_tree.setStyleSheet(f"""
            QTreeWidget::indicator {{
                width: 16px;
                height: 16px;
                border: 2px solid {border};
                border-radius: 3px;
                background-color: {surface};
            }}
            QTreeWidget::indicator:hover {{
                border-color: {accent};
            }}
            QTreeWidget::indicator:checked {{
                background-color: {accent};
                border-color: {accent};
                image: none;
            }}
            QTreeWidget::indicator:checked:hover {{
                background-color: {accent};
                border-color: {accent};
            }}
        """)

        steps_layout.addWidget(self.steps_tree)

        steps_btn_layout = QHBoxLayout()
        edit_step_btn = QPushButton("Edit Selected")
        edit_step_btn.clicked.connect(self._edit_step)
        steps_btn_layout.addWidget(edit_step_btn)

        remove_step_btn = QPushButton("Remove Selected")
        remove_step_btn.clicked.connect(self._remove_step)
        steps_btn_layout.addWidget(remove_step_btn)

        steps_layout.addLayout(steps_btn_layout)

        steps_group.setLayout(steps_layout)
        form_layout.addRow(steps_group)

        # Linked Files
        files_group = QGroupBox("Linked Files")
        files_layout = QVBoxLayout()

        files_controls = QHBoxLayout()
        browse_file_btn = QPushButton("Browse...")
        browse_file_btn.clicked.connect(self._browse_file)
        files_controls.addWidget(browse_file_btn)

        files_layout.addLayout(files_controls)

        self.files_list = QListWidget()
        self.files_list.setMaximumHeight(100)
        files_layout.addWidget(self.files_list)

        remove_file_btn = QPushButton("Remove Selected")
        remove_file_btn.clicked.connect(self._remove_file)
        files_layout.addWidget(remove_file_btn)

        files_group.setLayout(files_layout)
        form_layout.addRow(files_group)

        form_widget.setLayout(form_layout)
        scroll.setWidget(form_widget)
        layout.addWidget(scroll)

        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self.setLayout(layout)

    def _toggle_due_date(self, checked):
        """Toggle due date field"""
        self.due_date_edit.setEnabled(checked)

    def _add_category(self):
        """Add a new category"""
        category, ok = QInputDialog.getText(self, "New Category", "Category name:")
        if ok and category:
            if self.todo_list:
                self.todo_list.add_category(category)
                self.category_combo.addItem(category)
                self.category_combo.setCurrentText(category)

    def _add_assignee(self):
        """Add an assignee"""
        assignee = self.assignee_input.text().strip()
        if assignee:
            self.assignee_list.addItem(assignee)
            self.assignee_input.clear()

    def _remove_assignee(self):
        """Remove selected assignee"""
        current = self.assignee_list.currentItem()
        if current:
            self.assignee_list.takeItem(self.assignee_list.row(current))

    def _add_step(self):
        """Add a step"""
        description = self.step_input.text().strip()
        if not description:
            return

        is_subtask = self.step_type_combo.currentIndex() == 0
        step = TaskStep(description, is_subtask)

        item = QTreeWidgetItem()
        item.setText(0, description)
        item.setText(1, "Subtask" if is_subtask else "Counter")

        if is_subtask:
            # Enable checkbox and set it
            item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)
            item.setCheckState(2, Qt.CheckState.Unchecked)
        else:
            # Counter display
            item.setText(2, "0/1")

        item.setData(0, Qt.ItemDataRole.UserRole, step)

        self.steps_tree.addTopLevelItem(item)
        self.step_input.clear()

    def _on_step_clicked(self, item: QTreeWidgetItem, column: int):
        """Handle click on step in edit dialog"""
        step = item.data(0, Qt.ItemDataRole.UserRole)
        if not step:
            return

        # If clicking on progress column (2) and it's a subtask
        if column == 2 and step.is_subtask:
            # The checkbox state has already changed, so read the new state
            new_check_state = item.checkState(2)
            step.completed = (new_check_state == Qt.CheckState.Checked)

    def _edit_step(self):
        """Edit selected step"""
        current = self.steps_tree.currentItem()
        if not current:
            return

        step = current.data(0, Qt.ItemDataRole.UserRole)
        if not step:
            return

        if step.is_subtask:
            # Toggle checkbox
            step.completed = not step.completed
            current.setText(2, "☑" if step.completed else "☐")
        else:
            # Edit counter
            count, ok = QInputDialog.getInt(
                self, "Edit Counter",
                f"Current: {step.current_count}/{step.target_count}\n\nEnter current count:",
                step.current_count, 0, 99999
            )
            if ok:
                step.current_count = count
                current.setText(2, f"{step.current_count}/{step.target_count}")

    def _remove_step(self):
        """Remove selected step"""
        current = self.steps_tree.currentItem()
        if current:
            index = self.steps_tree.indexOfTopLevelItem(current)
            self.steps_tree.takeTopLevelItem(index)

    def _browse_file(self):
        """Browse for file to link"""
        start_dir = str(Path(self.project_root)) if self.project_root else str(Path.home())

        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select File to Link",
            start_dir,
            "All Files (*.*)"
        )

        if file_path:
            # Convert to relative path if within project
            if self.project_root:
                try:
                    rel_path = Path(file_path).relative_to(Path(self.project_root))
                    file_path = str(rel_path).replace('\\', '/')
                except ValueError:
                    pass

            self.files_list.addItem(file_path)

    def _remove_file(self):
        """Remove selected file"""
        current = self.files_list.currentItem()
        if current:
            self.files_list.takeItem(self.files_list.row(current))

    def load_task_data(self):
        """Load task data into fields"""
        if not self.task:
            return

        self.title_edit.setText(self.task.title)
        self.description_edit.setPlainText(self.task.description)

        # Set category
        index = self.category_combo.findText(self.task.category)
        if index >= 0:
            self.category_combo.setCurrentIndex(index)

        # Set priority
        index = self.priority_combo.findText(self.task.priority)
        if index >= 0:
            self.priority_combo.setCurrentIndex(index)

        # Set status
        index = self.status_combo.findText(self.task.status)
        if index >= 0:
            self.status_combo.setCurrentIndex(index)

        # Set due date
        if self.task.due_date:
            self.due_date_check.setChecked(True)
            date = QDate.fromString(self.task.due_date, "yyyy-MM-dd")
            self.due_date_edit.setDate(date)

        # Set assignees
        for assignee in self.task.assignees:
            self.assignee_list.addItem(assignee)

        # Set steps
        for step in self.task.steps:
            item = QTreeWidgetItem()
            item.setText(0, step.description)
            item.setText(1, "Subtask" if step.is_subtask else "Counter")
            if step.is_subtask:
                # Enable checkbox and set its state
                item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)
                item.setCheckState(2, Qt.CheckState.Checked if step.completed else Qt.CheckState.Unchecked)
            else:
                item.setText(2, f"{step.current_count}/{step.target_count}")
            item.setData(0, Qt.ItemDataRole.UserRole, step)
            self.steps_tree.addTopLevelItem(item)

        # Set linked files
        for file_path in self.task.linked_files:
            self.files_list.addItem(file_path)

    def get_task(self) -> Task:
        """Get task from dialog fields"""
        title = self.title_edit.text().strip() or "Untitled"
        description = self.description_edit.toPlainText().strip()
        category = self.category_combo.currentText()
        priority = self.priority_combo.currentText()
        status = self.status_combo.currentText()

        due_date = None
        if self.due_date_check.isChecked():
            due_date = self.due_date_edit.date().toString("yyyy-MM-dd")

        assignees = []
        for i in range(self.assignee_list.count()):
            assignees.append(self.assignee_list.item(i).text())

        steps = []
        for i in range(self.steps_tree.topLevelItemCount()):
            item = self.steps_tree.topLevelItem(i)
            step = item.data(0, Qt.ItemDataRole.UserRole)
            if step:
                steps.append(step)

        linked_files = []
        for i in range(self.files_list.count()):
            linked_files.append(self.files_list.item(i).text())

        if self.task:
            # Update existing task
            self.task.title = title
            self.task.description = description
            self.task.category = category
            self.task.priority = priority
            self.task.status = status
            self.task.due_date = due_date
            self.task.assignees = assignees
            self.task.steps = steps
            self.task.linked_files = linked_files
            self.task.modified_date = datetime.now().isoformat()
            return self.task
        else:
            # Create new task
            return Task(
                title=title,
                description=description,
                category=category,
                priority=priority,
                status=status,
                due_date=due_date,
                assignees=assignees,
                steps=steps,
                linked_files=linked_files
            )


class ManageCategoriesDialog(QDialog):
    """Dialog for managing categories"""

    def __init__(self, todo_list: TodoList, parent=None):
        super().__init__(parent)
        self.todo_list = todo_list
        self.setWindowTitle("Manage Categories")
        self.setModal(True)
        self.setMinimumWidth(400)

        self.setup_ui()
        self.load_categories()

    def setup_ui(self):
        """Setup dialog UI"""
        layout = QVBoxLayout()

        # Add category
        add_layout = QHBoxLayout()
        self.category_input = QLineEdit()
        self.category_input.setPlaceholderText("New category name")
        add_layout.addWidget(self.category_input)

        add_btn = QPushButton("Add")
        add_btn.clicked.connect(self._add_category)
        add_layout.addWidget(add_btn)

        layout.addLayout(add_layout)

        # Category list
        self.category_list = QListWidget()
        layout.addWidget(self.category_list)

        # Remove button
        remove_btn = QPushButton("Remove Selected")
        remove_btn.clicked.connect(self._remove_category)
        layout.addWidget(remove_btn)

        # Close button
        close_btn = QPushButton("Close")
        close_btn.clicked.connect(self.accept)
        layout.addWidget(close_btn)

        self.setLayout(layout)

    def load_categories(self):
        """Load categories into list"""
        self.category_list.clear()
        for category in self.todo_list.categories:
            self.category_list.addItem(category)

    def _add_category(self):
        """Add a new category"""
        category = self.category_input.text().strip()
        if category:
            self.todo_list.add_category(category)
            self.load_categories()
            self.category_input.clear()

    def _remove_category(self):
        """Remove selected category"""
        current = self.category_list.currentItem()
        if current:
            category = current.text()
            if category == "General":
                QMessageBox.warning(self, "Cannot Remove", "Cannot remove the General category.")
                return

            reply = QMessageBox.question(
                self,
                "Confirm Removal",
                f"Remove category '{category}'?\nTasks with this category will be moved to 'General'.",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )

            if reply == QMessageBox.StandardButton.Yes:
                self.todo_list.remove_category(category)
                self.load_categories()


class ManagePrioritiesDialog(QDialog):
    """Dialog for managing priorities"""

    def __init__(self, todo_list: TodoList, parent=None):
        super().__init__(parent)
        self.todo_list = todo_list
        self.setWindowTitle("Manage Priorities")
        self.setModal(True)
        self.setMinimumWidth(400)

        self.setup_ui()
        self.load_priorities()

    def setup_ui(self):
        """Setup dialog UI"""
        layout = QVBoxLayout()

        # Add priority
        add_layout = QHBoxLayout()
        self.priority_input = QLineEdit()
        self.priority_input.setPlaceholderText("Priority name")
        add_layout.addWidget(self.priority_input)

        self.color_input = QLineEdit()
        self.color_input.setPlaceholderText("#RRGGBB")
        self.color_input.setMaximumWidth(100)
        add_layout.addWidget(self.color_input)

        add_btn = QPushButton("Add")
        add_btn.clicked.connect(self._add_priority)
        add_layout.addWidget(add_btn)

        layout.addLayout(add_layout)

        # Priority list
        self.priority_list = QListWidget()
        layout.addWidget(self.priority_list)

        # Remove button
        remove_btn = QPushButton("Remove Selected")
        remove_btn.clicked.connect(self._remove_priority)
        layout.addWidget(remove_btn)

        # Close button
        close_btn = QPushButton("Close")
        close_btn.clicked.connect(self.accept)
        layout.addWidget(close_btn)

        self.setLayout(layout)

    def load_priorities(self):
        """Load priorities into list"""
        self.priority_list.clear()
        for priority, color in self.todo_list.priorities.items():
            item = QListWidgetItem(f"{priority} ({color})")
            item.setData(Qt.ItemDataRole.UserRole, priority)
            item.setForeground(QBrush(QColor(color)))
            self.priority_list.addItem(item)

    def _add_priority(self):
        """Add a new priority"""
        priority = self.priority_input.text().strip()
        color = self.color_input.text().strip()

        if not priority or not color:
            QMessageBox.warning(self, "Invalid Input", "Please enter both priority name and color.")
            return

        # Validate color
        if not color.startswith('#') or len(color) != 7:
            QMessageBox.warning(self, "Invalid Color", "Color must be in #RRGGBB format.")
            return

        self.todo_list.add_priority(priority, color)
        self.load_priorities()
        self.priority_input.clear()
        self.color_input.clear()

    def _remove_priority(self):
        """Remove selected priority"""
        current = self.priority_list.currentItem()
        if current:
            priority = current.data(Qt.ItemDataRole.UserRole)
            if priority in ["Low", "Medium", "High"]:
                QMessageBox.warning(self, "Cannot Remove", "Cannot remove default priorities.")
                return

            reply = QMessageBox.question(
                self,
                "Confirm Removal",
                f"Remove priority '{priority}'?\nTasks with this priority will be set to 'Medium'.",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )

            if reply == QMessageBox.StandardButton.Yes:
                self.todo_list.remove_priority(priority)
                self.load_priorities()


class ManageStatusesDialog(QDialog):
    """Dialog for managing statuses"""

    def __init__(self, todo_list: TodoList, parent=None):
        super().__init__(parent)
        self.todo_list = todo_list
        self.setWindowTitle("Manage Statuses")
        self.setModal(True)
        self.setMinimumWidth(400)

        self.setup_ui()
        self.load_statuses()

    def setup_ui(self):
        """Setup dialog UI"""
        layout = QVBoxLayout()

        # Add status
        add_layout = QHBoxLayout()
        self.status_input = QLineEdit()
        self.status_input.setPlaceholderText("New status name")
        add_layout.addWidget(self.status_input)

        add_btn = QPushButton("Add")
        add_btn.clicked.connect(self._add_status)
        add_layout.addWidget(add_btn)

        layout.addLayout(add_layout)

        # Status list
        self.status_list = QListWidget()
        layout.addWidget(self.status_list)

        # Remove button
        remove_btn = QPushButton("Remove Selected")
        remove_btn.clicked.connect(self._remove_status)
        layout.addWidget(remove_btn)

        # Close button
        close_btn = QPushButton("Close")
        close_btn.clicked.connect(self.accept)
        layout.addWidget(close_btn)

        self.setLayout(layout)

    def load_statuses(self):
        """Load statuses into list"""
        self.status_list.clear()
        for status in self.todo_list.statuses:
            self.status_list.addItem(status)

    def _add_status(self):
        """Add a new status"""
        status = self.status_input.text().strip()
        if status:
            self.todo_list.add_status(status)
            self.load_statuses()
            self.status_input.clear()

    def _remove_status(self):
        """Remove selected status"""
        current = self.status_list.currentItem()
        if current:
            status = current.text()
            default_statuses = ["Pending", "In Progress", "Awaiting Testing", "Awaiting Confirmation", "Completed"]
            if status in default_statuses:
                QMessageBox.warning(self, "Cannot Remove", "Cannot remove default statuses.")
                return

            reply = QMessageBox.question(
                self,
                "Confirm Removal",
                f"Remove status '{status}'?\nTasks with this status will be set to 'Pending'.",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )

            if reply == QMessageBox.StandardButton.Yes:
                self.todo_list.remove_status(status)
                self.load_statuses()


# ===== MAIN PANEL =====

class TodoListTool(EditorPanel):
    """To Do List tool for production task management"""

    def __init__(self, parent=None, project_root=None):
        self.project_root = project_root
        self.open_lists: Dict[str, TodoList] = {}  # file_path -> TodoList
        self.current_list: Optional[TodoList] = None
        self.search_text = ""
        self.filter_category = "All"
        self.filter_priority = "All"
        self.filter_status = "All"
        self.sort_by = "Created"

        super().__init__("To Do Lists", parent)
        self.setObjectName("TodoListTool")

        # Load session
        if self.project_root:
            self._load_session()

    def _setup_panel(self):
        """Setup todo list panel UI"""
        # Main widget
        main_widget = QWidget()
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 0, 0, 0)

        # Toolbar
        toolbar = QHBoxLayout()
        toolbar.setContentsMargins(5, 5, 5, 5)

        new_list_btn = QPushButton("New List")
        new_list_btn.clicked.connect(self._new_list)
        toolbar.addWidget(new_list_btn)

        open_list_btn = QPushButton("Open List")
        open_list_btn.clicked.connect(self._open_list)
        toolbar.addWidget(open_list_btn)

        save_btn = QPushButton("Save")
        save_btn.clicked.connect(self._save_current_list)
        toolbar.addWidget(save_btn)

        save_all_btn = QPushButton("Save All")
        save_all_btn.clicked.connect(self._save_all_lists)
        toolbar.addWidget(save_all_btn)

        close_list_btn = QPushButton("Close List")
        close_list_btn.clicked.connect(self._close_current_list)
        toolbar.addWidget(close_list_btn)

        toolbar.addStretch()

        # Manage buttons
        manage_btn = QPushButton("Manage")
        manage_menu = QMenu()
        manage_categories_action = QAction("Categories", self)
        manage_categories_action.triggered.connect(self._manage_categories)
        manage_menu.addAction(manage_categories_action)

        manage_priorities_action = QAction("Priorities", self)
        manage_priorities_action.triggered.connect(self._manage_priorities)
        manage_menu.addAction(manage_priorities_action)

        manage_statuses_action = QAction("Statuses", self)
        manage_statuses_action.triggered.connect(self._manage_statuses)
        manage_menu.addAction(manage_statuses_action)

        manage_btn.setMenu(manage_menu)
        toolbar.addWidget(manage_btn)

        main_layout.addLayout(toolbar)

        # Tab widget for lists
        self.list_tabs = QTabWidget()
        self.list_tabs.setTabsClosable(True)
        self.list_tabs.setMovable(True)
        self.list_tabs.tabCloseRequested.connect(self._close_list_tab)
        self.list_tabs.currentChanged.connect(self._on_tab_changed)
        main_layout.addWidget(self.list_tabs)

        main_widget.setLayout(main_layout)
        self.content_layout.addWidget(main_widget)

    def _create_list_widget(self, todo_list: TodoList) -> QWidget:
        """Create widget for a todo list"""
        widget = QWidget()
        layout = QVBoxLayout()
        layout.setContentsMargins(5, 5, 5, 5)

        # Task controls toolbar
        controls = QHBoxLayout()

        add_task_btn = QPushButton("Add Task")
        add_task_btn.clicked.connect(lambda: self._add_task(todo_list))
        controls.addWidget(add_task_btn)

        edit_task_btn = QPushButton("Edit Task")
        edit_task_btn.clicked.connect(lambda: self._edit_task(todo_list))
        controls.addWidget(edit_task_btn)

        delete_task_btn = QPushButton("Delete Task")
        delete_task_btn.clicked.connect(lambda: self._delete_task(todo_list))
        controls.addWidget(delete_task_btn)

        controls.addStretch()

        layout.addLayout(controls)

        # Filter and search
        filter_layout = QHBoxLayout()

        # Search
        search_label = QLabel("Search:")
        filter_layout.addWidget(search_label)

        search_input = QLineEdit()
        search_input.setPlaceholderText("Search tasks...")
        search_input.textChanged.connect(lambda text: self._on_search_changed(text, todo_list))
        filter_layout.addWidget(search_input)

        # Category filter
        filter_layout.addWidget(QLabel("Category:"))
        category_filter = QComboBox()
        category_filter.addItem("All")
        category_filter.addItems(todo_list.categories)
        category_filter.currentTextChanged.connect(lambda text: self._on_category_filter_changed(text, todo_list))
        filter_layout.addWidget(category_filter)

        # Priority filter
        filter_layout.addWidget(QLabel("Priority:"))
        priority_filter = QComboBox()
        priority_filter.addItem("All")
        priority_filter.addItems(list(todo_list.priorities.keys()))
        priority_filter.currentTextChanged.connect(lambda text: self._on_priority_filter_changed(text, todo_list))
        filter_layout.addWidget(priority_filter)

        # Status filter
        filter_layout.addWidget(QLabel("Status:"))
        status_filter = QComboBox()
        status_filter.addItem("All")
        status_filter.addItems(todo_list.statuses)
        status_filter.currentTextChanged.connect(lambda text: self._on_status_filter_changed(text, todo_list))
        filter_layout.addWidget(status_filter)

        # Sort
        filter_layout.addWidget(QLabel("Sort:"))
        sort_combo = QComboBox()
        sort_combo.addItems(["Created", "Modified", "Title", "Priority", "Status", "Due Date"])
        sort_combo.currentTextChanged.connect(lambda text: self._on_sort_changed(text, todo_list))
        filter_layout.addWidget(sort_combo)

        layout.addLayout(filter_layout)

        # Task tree
        task_tree = QTreeWidget()
        task_tree.setHeaderLabels(["✓", "Title", "Category", "Priority", "Status", "Due Date", "Assignees"])
        task_tree.setColumnWidth(0, 70)  # Checkbox column (wider for indented subtasks)
        task_tree.setColumnWidth(1, 250)
        task_tree.setColumnWidth(2, 100)
        task_tree.setColumnWidth(3, 80)
        task_tree.setColumnWidth(4, 120)
        task_tree.setColumnWidth(5, 100)
        task_tree.itemDoubleClicked.connect(lambda: self._edit_task(todo_list))
        task_tree.itemClicked.connect(lambda item, col: self._on_task_item_clicked(item, col, todo_list))

        # Enable alternating row colors and selection
        task_tree.setAlternatingRowColors(True)
        task_tree.setSelectionMode(QTreeWidget.SelectionMode.ExtendedSelection)

        # Get theme colors
        theme = get_theme_manager().get_current_theme()
        if theme:
            c = theme.colors
            hover_bg = c.surface_hover
            select_bg = c.selection
            select_text = c.text_primary
            select_hover_bg = c.accent_color
            accent = c.accent_color
            surface = c.surface
            border = c.border
        else:
            hover_bg = "#e5f3ff"
            select_bg = "#0078d7"
            select_text = "white"
            select_hover_bg = "#005fa3"
            accent = "#7b5fb8"
            surface = "#1e1a28"
            border = "#3d3650"

        # Style the tree for better hover and selection visibility, plus checkbox styling
        task_tree.setStyleSheet(f"""
            QTreeWidget {{
                outline: none;
                selection-background-color: {select_bg};
                selection-color: {select_text};
            }}
            QTreeWidget::item {{
                padding: 4px;
                border: none;
            }}
            QTreeWidget::item:hover {{
                background-color: {hover_bg};
            }}
            QTreeWidget::item:selected {{
                background-color: {select_bg};
                color: {select_text};
            }}
            QTreeWidget::item:selected:hover {{
                background-color: {select_hover_bg};
                color: {select_text};
            }}
            QTreeWidget::indicator {{
                width: 16px;
                height: 16px;
                border: 2px solid {border};
                border-radius: 3px;
                background-color: {surface};
            }}
            QTreeWidget::indicator:hover {{
                border-color: {accent};
            }}
            QTreeWidget::indicator:checked {{
                background-color: {accent};
                border-color: {accent};
                image: none;
            }}
            QTreeWidget::indicator:checked:hover {{
                background-color: {accent};
                border-color: {accent};
            }}
        """)

        # Context menu
        task_tree.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        task_tree.customContextMenuRequested.connect(lambda pos: self._show_task_context_menu(pos, task_tree, todo_list))

        layout.addWidget(task_tree)

        # Store references
        widget.task_tree = task_tree
        widget.todo_list = todo_list
        widget.search_input = search_input
        widget.category_filter = category_filter
        widget.priority_filter = priority_filter
        widget.status_filter = status_filter
        widget.sort_combo = sort_combo

        widget.setLayout(layout)

        # Initial population
        self._refresh_task_tree(widget)

        return widget

    def _refresh_task_tree(self, list_widget: QWidget):
        """Refresh the task tree for a list widget"""
        task_tree = list_widget.task_tree
        todo_list = list_widget.todo_list

        # Save expanded state before clearing
        expanded_tasks = set()
        for i in range(task_tree.topLevelItemCount()):
            item = task_tree.topLevelItem(i)
            if item and item.isExpanded():
                task = item.data(0, Qt.ItemDataRole.UserRole)
                if task:
                    expanded_tasks.add(id(task))

        task_tree.clear()

        # Get filtered and sorted tasks
        tasks = self._filter_and_sort_tasks(
            todo_list,
            self.search_text,
            self.filter_category,
            self.filter_priority,
            self.filter_status,
            self.sort_by
        )

        for task in tasks:
            item = self._create_task_item(task, todo_list)
            task_tree.addTopLevelItem(item)

            # If task has steps, add them as child items
            if task.steps:
                for step in task.steps:
                    step_item = self._create_step_item(step, task)
                    item.addChild(step_item)

                # Restore expanded state, or expand by default if this is first time
                if id(task) in expanded_tasks or not hasattr(list_widget, '_has_been_refreshed'):
                    item.setExpanded(True)
                else:
                    item.setExpanded(False)

        # Mark that we've refreshed at least once
        list_widget._has_been_refreshed = True

    def _create_task_item(self, task: Task, todo_list: TodoList) -> QTreeWidgetItem:
        """Create a tree widget item for a task"""
        item = QTreeWidgetItem()

        # Get theme colors
        theme = get_theme_manager().get_current_theme()
        if theme:
            disabled_color = theme.colors.text_disabled
        else:
            disabled_color = "#888888"

        # Enable checkbox in column 0
        item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)

        # Checkbox - use Qt checkbox instead of text
        is_completed = task.status == "Completed"
        item.setCheckState(0, Qt.CheckState.Checked if is_completed else Qt.CheckState.Unchecked)

        # Task details
        item.setText(1, task.title)
        item.setText(2, task.category)
        item.setText(3, task.priority)
        item.setText(4, task.status)
        item.setText(5, task.due_date or "")
        item.setText(6, ", ".join(task.assignees))

        # Set priority color
        if task.priority in todo_list.priorities:
            color = QColor(todo_list.priorities[task.priority])
            item.setForeground(3, QBrush(color))

        # Set completed style
        if is_completed:
            font = item.font(1)
            font.setStrikeOut(True)
            for col in range(7):
                item.setFont(col, font)
                if col != 3:  # Don't override priority color
                    item.setForeground(col, QBrush(QColor(disabled_color)))

        # Store task reference - store in ALL columns for proper click detection
        for col in range(7):
            item.setData(col, Qt.ItemDataRole.UserRole, task)
            item.setData(col, Qt.ItemDataRole.UserRole + 1, "task")

        return item

    def _create_step_item(self, step: TaskStep, parent_task: Task) -> QTreeWidgetItem:
        """Create a tree widget item for a step"""
        item = QTreeWidgetItem()

        # Get theme colors
        theme = get_theme_manager().get_current_theme()
        if theme:
            disabled_color = theme.colors.text_disabled
        else:
            disabled_color = "#888888"

        # Checkbox for subtasks, counter for other steps
        if step.is_subtask:
            # Enable checkbox in column 0
            item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)
            # Use Qt checkbox for clickability
            item.setCheckState(0, Qt.CheckState.Checked if step.completed else Qt.CheckState.Unchecked)
        else:
            # Counter display
            item.setText(0, f"{step.current_count}/{step.target_count}")
            item.setTextAlignment(0, Qt.AlignmentFlag.AlignCenter)

        # Step description (indented)
        item.setText(1, f"  └─ {step.description}")

        # Leave other columns empty for steps
        for col in range(2, 7):
            item.setText(col, "")

        # Set completed style for completed subtasks
        if step.is_subtask and step.completed:
            font = item.font(1)
            font.setStrikeOut(True)
            for col in range(7):
                item.setFont(col, font)
                item.setForeground(col, QBrush(QColor(disabled_color)))

        # Store step reference - CRITICAL: Store in ALL columns for proper click detection
        for col in range(7):
            item.setData(col, Qt.ItemDataRole.UserRole, step)
            item.setData(col, Qt.ItemDataRole.UserRole + 1, "step")
            item.setData(col, Qt.ItemDataRole.UserRole + 2, parent_task)

        return item

    def _on_task_item_clicked(self, item: QTreeWidgetItem, column: int, todo_list: TodoList):
        """Handle click on task/step item"""
        # Only handle clicks on checkbox column
        if column != 0:
            return

        item_type = item.data(0, Qt.ItemDataRole.UserRole + 1)

        if item_type == "task":
            # Toggle task completion based on checkbox state
            task = item.data(0, Qt.ItemDataRole.UserRole)
            if task:
                # The checkbox state has already changed, so read the new state
                new_check_state = item.checkState(0)
                if new_check_state == Qt.CheckState.Checked:
                    task.status = "Completed"
                else:
                    task.status = "In Progress"
                task.modified_date = datetime.now().isoformat()
                todo_list.modified = True
                self._refresh_current_list()
                # Auto-save
                todo_list.save()

        elif item_type == "step":
            # Toggle step completion or edit counter
            step = item.data(0, Qt.ItemDataRole.UserRole)
            parent_task = item.data(0, Qt.ItemDataRole.UserRole + 2)

            if step and parent_task:
                if step.is_subtask:
                    # Toggle checkbox - read the new state
                    new_check_state = item.checkState(0)
                    step.completed = (new_check_state == Qt.CheckState.Checked)
                else:
                    # Increment counter
                    step.current_count += 1
                    if step.current_count > step.target_count:
                        step.current_count = 0

                parent_task.modified_date = datetime.now().isoformat()
                todo_list.modified = True
                self._refresh_current_list()
                # Auto-save
                todo_list.save()

    def _filter_and_sort_tasks(self, todo_list: TodoList, search: str,
                               category: str, priority: str, status: str, sort_by: str) -> List[Task]:
        """Filter and sort tasks"""
        tasks = todo_list.tasks.copy()

        # Filter by search
        if search:
            search_lower = search.lower()
            tasks = [t for t in tasks if
                    search_lower in t.title.lower() or
                    search_lower in t.description.lower()]

        # Filter by category
        if category != "All":
            tasks = [t for t in tasks if t.category == category]

        # Filter by priority
        if priority != "All":
            tasks = [t for t in tasks if t.priority == priority]

        # Filter by status
        if status != "All":
            tasks = [t for t in tasks if t.status == status]

        # Sort
        if sort_by == "Created":
            tasks.sort(key=lambda t: t.created_date, reverse=True)
        elif sort_by == "Modified":
            tasks.sort(key=lambda t: t.modified_date, reverse=True)
        elif sort_by == "Title":
            tasks.sort(key=lambda t: t.title.lower())
        elif sort_by == "Priority":
            priority_order = list(todo_list.priorities.keys())
            tasks.sort(key=lambda t: priority_order.index(t.priority) if t.priority in priority_order else 999)
        elif sort_by == "Status":
            tasks.sort(key=lambda t: todo_list.statuses.index(t.status) if t.status in todo_list.statuses else 999)
        elif sort_by == "Due Date":
            tasks.sort(key=lambda t: t.due_date or "9999-99-99")

        return tasks

    def _on_search_changed(self, text: str, todo_list: TodoList):
        """Handle search text change"""
        self.search_text = text
        self._refresh_current_list()

    def _on_category_filter_changed(self, category: str, todo_list: TodoList):
        """Handle category filter change"""
        self.filter_category = category
        self._refresh_current_list()

    def _on_priority_filter_changed(self, priority: str, todo_list: TodoList):
        """Handle priority filter change"""
        self.filter_priority = priority
        self._refresh_current_list()

    def _on_status_filter_changed(self, status: str, todo_list: TodoList):
        """Handle status filter change"""
        self.filter_status = status
        self._refresh_current_list()

    def _on_sort_changed(self, sort_by: str, todo_list: TodoList):
        """Handle sort change"""
        self.sort_by = sort_by
        self._refresh_current_list()

    def _refresh_current_list(self):
        """Refresh the currently active list"""
        current_widget = self.list_tabs.currentWidget()
        if current_widget and hasattr(current_widget, 'task_tree'):
            self._refresh_task_tree(current_widget)

    def _show_task_context_menu(self, pos, tree: QTreeWidget, todo_list: TodoList):
        """Show context menu for tasks"""
        item = tree.itemAt(pos)
        if not item:
            return

        item_type = item.data(0, Qt.ItemDataRole.UserRole + 1)

        menu = QMenu(self)

        edit_action = QAction("Edit Task", self)
        edit_action.triggered.connect(lambda: self._edit_task(todo_list))
        menu.addAction(edit_action)

        delete_action = QAction("Delete Task", self)
        delete_action.triggered.connect(lambda: self._delete_task(todo_list))
        menu.addAction(delete_action)

        menu.addSeparator()

        # Quick status change (only for tasks, not steps)
        if item_type == "task":
            status_menu = menu.addMenu("Set Status")
            for status in todo_list.statuses:
                status_action = QAction(status, self)
                status_action.triggered.connect(lambda checked, s=status: self._quick_set_status(item, s, todo_list))
                status_menu.addAction(status_action)

        menu.exec(tree.viewport().mapToGlobal(pos))

    def _quick_set_status(self, item: QTreeWidgetItem, status: str, todo_list: TodoList):
        """Quickly set task status"""
        item_type = item.data(0, Qt.ItemDataRole.UserRole + 1)

        # Get the task
        if item_type == "step":
            task = item.data(0, Qt.ItemDataRole.UserRole + 2)
        else:
            task = item.data(0, Qt.ItemDataRole.UserRole)

        if task:
            task.status = status
            task.modified_date = datetime.now().isoformat()
            todo_list.modified = True
            self._refresh_current_list()
            # Auto-save
            todo_list.save()

    def _new_list(self):
        """Create a new todo list"""
        name, ok = QInputDialog.getText(self, "New List", "List name:")
        if not ok or not name:
            return

        # Determine file path
        if self.project_root:
            todo_dir = Path(self.project_root) / "production" / "todo"
            todo_dir.mkdir(parents=True, exist_ok=True)
            file_path = todo_dir / f"{name}.json"
        else:
            file_path, _ = QFileDialog.getSaveFileName(
                self,
                "Save List",
                f"{name}.json",
                "JSON Files (*.json)"
            )
            if not file_path:
                return

        # Create list
        todo_list = TodoList(name, str(file_path))

        # Save immediately
        if not todo_list.save():
            QMessageBox.warning(self, "Error", "Failed to create list.")
            return

        # Open the list
        self._open_todo_list(todo_list)

        # Save session
        self._save_session()

    def _open_list(self):
        """Open an existing todo list"""
        start_dir = str(Path(self.project_root) / "production" / "todo") if self.project_root else ""

        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Open List",
            start_dir,
            "JSON Files (*.json)"
        )

        if not file_path:
            return

        # Check if already open
        if file_path in self.open_lists:
            # Switch to tab
            for i in range(self.list_tabs.count()):
                widget = self.list_tabs.widget(i)
                if hasattr(widget, 'todo_list') and widget.todo_list.file_path == file_path:
                    self.list_tabs.setCurrentIndex(i)
                    return

        # Load list
        todo_list = TodoList.load(file_path)
        if not todo_list:
            QMessageBox.warning(self, "Error", "Failed to load list.")
            return

        self._open_todo_list(todo_list)

        # Save session
        self._save_session()

    def _open_todo_list(self, todo_list: TodoList):
        """Open a todo list in a tab"""
        # Create widget
        list_widget = self._create_list_widget(todo_list)

        # Add tab
        tab_index = self.list_tabs.addTab(list_widget, todo_list.name)
        self.list_tabs.setCurrentIndex(tab_index)

        # Track open list
        self.open_lists[todo_list.file_path] = todo_list
        self.current_list = todo_list

    def _save_current_list(self):
        """Save the current list"""
        current_widget = self.list_tabs.currentWidget()
        if not current_widget or not hasattr(current_widget, 'todo_list'):
            return

        todo_list = current_widget.todo_list
        if todo_list.save():
            QMessageBox.information(self, "Saved", f"List '{todo_list.name}' saved successfully.")
        else:
            QMessageBox.warning(self, "Error", "Failed to save list.")

    def _save_all_lists(self):
        """Save all open lists"""
        for todo_list in self.open_lists.values():
            todo_list.save()

        QMessageBox.information(self, "Saved", "All lists saved successfully.")

    def _close_current_list(self):
        """Close the current list"""
        index = self.list_tabs.currentIndex()
        if index >= 0:
            self._close_list_tab(index)

    def _close_list_tab(self, index: int):
        """Close a list tab"""
        widget = self.list_tabs.widget(index)
        if not widget or not hasattr(widget, 'todo_list'):
            return

        todo_list = widget.todo_list

        # Check if modified
        if todo_list.modified:
            reply = QMessageBox.question(
                self,
                "Unsaved Changes",
                f"List '{todo_list.name}' has unsaved changes. Save before closing?",
                QMessageBox.StandardButton.Save | QMessageBox.StandardButton.Discard | QMessageBox.StandardButton.Cancel
            )

            if reply == QMessageBox.StandardButton.Save:
                if not todo_list.save():
                    QMessageBox.warning(self, "Error", "Failed to save list.")
                    return
            elif reply == QMessageBox.StandardButton.Cancel:
                return

        # Remove from tracking
        if todo_list.file_path in self.open_lists:
            del self.open_lists[todo_list.file_path]

        # Remove tab
        self.list_tabs.removeTab(index)

        # Update current list
        current_widget = self.list_tabs.currentWidget()
        if current_widget and hasattr(current_widget, 'todo_list'):
            self.current_list = current_widget.todo_list
        else:
            self.current_list = None

        # Save session
        self._save_session()

    def _on_tab_changed(self, index: int):
        """Handle tab change"""
        if index < 0:
            self.current_list = None
            return

        widget = self.list_tabs.widget(index)
        if widget and hasattr(widget, 'todo_list'):
            self.current_list = widget.todo_list

    def _add_task(self, todo_list: TodoList):
        """Add a new task"""
        dialog = TaskEditDialog(None, todo_list, self.project_root, self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            task = dialog.get_task()
            todo_list.add_task(task)
            self._refresh_current_list()
            # Auto-save
            todo_list.save()

    def _edit_task(self, todo_list: TodoList):
        """Edit selected task"""
        current_widget = self.list_tabs.currentWidget()
        if not current_widget or not hasattr(current_widget, 'task_tree'):
            return

        task_tree = current_widget.task_tree
        current_item = task_tree.currentItem()
        if not current_item:
            return

        # Get the task - if it's a step, get the parent task
        item_type = current_item.data(0, Qt.ItemDataRole.UserRole + 1)
        if item_type == "step":
            task = current_item.data(0, Qt.ItemDataRole.UserRole + 2)
        else:
            task = current_item.data(0, Qt.ItemDataRole.UserRole)

        if not task:
            return

        dialog = TaskEditDialog(task, todo_list, self.project_root, self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            todo_list.modified = True
            self._refresh_current_list()
            # Auto-save
            todo_list.save()

    def _delete_task(self, todo_list: TodoList):
        """Delete selected task"""
        current_widget = self.list_tabs.currentWidget()
        if not current_widget or not hasattr(current_widget, 'task_tree'):
            return

        task_tree = current_widget.task_tree
        current_item = task_tree.currentItem()
        if not current_item:
            return

        # Get the task - if it's a step, get the parent task
        item_type = current_item.data(0, Qt.ItemDataRole.UserRole + 1)
        if item_type == "step":
            task = current_item.data(0, Qt.ItemDataRole.UserRole + 2)
        else:
            task = current_item.data(0, Qt.ItemDataRole.UserRole)

        if not task:
            return

        reply = QMessageBox.question(
            self,
            "Confirm Deletion",
            f"Delete task '{task.title}'?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )

        if reply == QMessageBox.StandardButton.Yes:
            todo_list.remove_task(task)
            self._refresh_current_list()
            # Auto-save
            todo_list.save()

    def _manage_categories(self):
        """Manage categories"""
        if not self.current_list:
            QMessageBox.warning(self, "No List", "No list is currently open.")
            return

        dialog = ManageCategoriesDialog(self.current_list, self)
        dialog.exec()

        # Auto-save
        if self.current_list.modified:
            self.current_list.save()

        # Refresh filters
        self._refresh_current_list()
        current_widget = self.list_tabs.currentWidget()
        if current_widget and hasattr(current_widget, 'category_filter'):
            current_text = current_widget.category_filter.currentText()
            current_widget.category_filter.clear()
            current_widget.category_filter.addItem("All")
            current_widget.category_filter.addItems(self.current_list.categories)
            index = current_widget.category_filter.findText(current_text)
            if index >= 0:
                current_widget.category_filter.setCurrentIndex(index)

    def _manage_priorities(self):
        """Manage priorities"""
        if not self.current_list:
            QMessageBox.warning(self, "No List", "No list is currently open.")
            return

        dialog = ManagePrioritiesDialog(self.current_list, self)
        dialog.exec()

        # Auto-save
        if self.current_list.modified:
            self.current_list.save()

        # Refresh
        self._refresh_current_list()
        current_widget = self.list_tabs.currentWidget()
        if current_widget and hasattr(current_widget, 'priority_filter'):
            current_text = current_widget.priority_filter.currentText()
            current_widget.priority_filter.clear()
            current_widget.priority_filter.addItem("All")
            current_widget.priority_filter.addItems(list(self.current_list.priorities.keys()))
            index = current_widget.priority_filter.findText(current_text)
            if index >= 0:
                current_widget.priority_filter.setCurrentIndex(index)

    def _manage_statuses(self):
        """Manage statuses"""
        if not self.current_list:
            QMessageBox.warning(self, "No List", "No list is currently open.")
            return

        dialog = ManageStatusesDialog(self.current_list, self)
        dialog.exec()

        # Auto-save
        if self.current_list.modified:
            self.current_list.save()

        # Refresh
        self._refresh_current_list()
        current_widget = self.list_tabs.currentWidget()
        if current_widget and hasattr(current_widget, 'status_filter'):
            current_text = current_widget.status_filter.currentText()
            current_widget.status_filter.clear()
            current_widget.status_filter.addItem("All")
            current_widget.status_filter.addItems(self.current_list.statuses)
            index = current_widget.status_filter.findText(current_text)
            if index >= 0:
                current_widget.status_filter.setCurrentIndex(index)

    def _get_todo_directory(self):
        """Get the todo directory path"""
        if self.project_root:
            todo_dir = Path(self.project_root) / "production" / "todo"
            todo_dir.mkdir(parents=True, exist_ok=True)
            return todo_dir
        return Path.home()

    def _save_session(self):
        """Save the current session"""
        if not self.project_root:
            return

        session_data = {
            'open_lists': [path for path in self.open_lists.keys()]
        }

        try:
            session_file = Path(self.project_root) / "production" / "todo_session.json"
            session_file.parent.mkdir(parents=True, exist_ok=True)

            with open(session_file, 'w', encoding='utf-8') as f:
                json.dump(session_data, f, indent=2)
        except Exception as e:
            print(f"Failed to save todo session: {e}")

    def _load_session(self):
        """Load the previous session"""
        if not self.project_root:
            return

        try:
            session_file = Path(self.project_root) / "production" / "todo_session.json"

            if session_file.exists():
                with open(session_file, 'r', encoding='utf-8') as f:
                    session_data = json.load(f)

                # Reopen lists from previous session
                for list_path in session_data.get('open_lists', []):
                    if Path(list_path).exists():
                        todo_list = TodoList.load(list_path)
                        if todo_list:
                            self._open_todo_list(todo_list)
        except Exception as e:
            print(f"Failed to load todo session: {e}")

    def get_state(self) -> dict:
        """Get panel state for serialization"""
        state = super().get_state()
        state['open_lists'] = list(self.open_lists.keys())
        return state

    def set_state(self, state: dict):
        """Restore panel state"""
        super().set_state(state)

        # Reopen lists
        for list_path in state.get('open_lists', []):
            if Path(list_path).exists():
                todo_list = TodoList.load(list_path)
                if todo_list:
                    self._open_todo_list(todo_list)
