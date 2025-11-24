"""
Input Mapping Dialog
Godot-style input mapping editor for defining actions and keybindings
Supports keyboard, mouse, and gamepad inputs with multiple bindings per action
"""

from PyQt6.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QPushButton, 
                             QTreeWidget, QTreeWidgetItem, QLabel, QWidget,
                             QLineEdit, QMessageBox, QMenu, QComboBox, QSpinBox,
                             QGroupBox, QFormLayout, QDoubleSpinBox, QSplitter)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QKeyEvent, QIcon
import json
from pathlib import Path
from typing import Dict, List, Optional, Any


class InputBindingEditor(QDialog):
    """Dialog for editing a single input binding"""
    
    def __init__(self, binding_data: Optional[Dict] = None, parent=None):
        super().__init__(parent)
        self.binding_data = binding_data or {}
        self.is_capturing = False
        self.captured_device_type = None
        self.captured_key = None
        
        self.setWindowTitle("Edit Input Binding")
        self.setModal(True)
        self.setMinimumSize(400, 250)
        
        self._setup_ui()
        self._load_binding()
    
    def _setup_ui(self):
        """Setup the dialog UI"""
        layout = QVBoxLayout()
        
        # Title
        title = QLabel("Configure Input Binding")
        title.setStyleSheet("font-size: 12px; font-weight: bold; padding: 5px 0;")
        layout.addWidget(title)
        
        # Device type selection
        device_group = QGroupBox("Input Device")
        device_layout = QVBoxLayout()
        
        self.device_combo = QComboBox()
        self.device_combo.addItems(["Keyboard", "Mouse", "Gamepad Button", "Gamepad Axis"])
        self.device_combo.currentIndexChanged.connect(self._on_device_changed)
        device_layout.addWidget(self.device_combo)
        
        device_group.setLayout(device_layout)
        layout.addWidget(device_group)
        
        # Input capture/selection
        capture_group = QGroupBox("Input Configuration")
        capture_layout = QVBoxLayout()
        
        # Keyboard capture
        self.keyboard_capture_widget = QWidget()
        keyboard_layout = QVBoxLayout()
        keyboard_layout.setContentsMargins(0, 0, 0, 0)
        
        self.capture_btn = QPushButton("Click and Press Key")
        self.capture_btn.setMinimumHeight(50)
        self.capture_btn.setProperty("accent", True)
        self.capture_btn.clicked.connect(self._start_capture)
        keyboard_layout.addWidget(self.capture_btn)
        
        self.binding_label = QLabel("No input assigned")
        self.binding_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.binding_label.setStyleSheet("padding: 10px; background: rgba(255,255,255,0.05); border-radius: 4px;")
        keyboard_layout.addWidget(self.binding_label)
        
        self.keyboard_capture_widget.setLayout(keyboard_layout)
        capture_layout.addWidget(self.keyboard_capture_widget)
        
        # Mouse dropdown
        self.mouse_dropdown_widget = QWidget()
        mouse_layout = QFormLayout()
        mouse_layout.setContentsMargins(0, 0, 0, 0)
        
        self.mouse_button_combo = QComboBox()
        self.mouse_button_combo.addItems(["Left", "Right", "Middle", "Button 4", "Button 5", "Scroll Up", "Scroll Down"])
        self.mouse_button_combo.currentTextChanged.connect(self._on_mouse_button_changed)
        mouse_layout.addRow("Mouse Button:", self.mouse_button_combo)
        
        self.mouse_dropdown_widget.setLayout(mouse_layout)
        self.mouse_dropdown_widget.hide()
        capture_layout.addWidget(self.mouse_dropdown_widget)
        
        # Gamepad selection
        self.gamepad_selection_widget = QWidget()
        gamepad_selection_layout = QVBoxLayout()
        gamepad_selection_layout.setContentsMargins(0, 0, 0, 0)
        
        # Gamepad button dropdown
        self.gamepad_button_combo = QComboBox()
        self.gamepad_button_combo.addItems([
            "A (Cross)", "B (Circle)", "X (Square)", "Y (Triangle)",
            "Left Bumper", "Right Bumper", "Left Trigger", "Right Trigger",
            "Back (Select)", "Start", "Left Stick", "Right Stick",
            "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right"
        ])
        self.gamepad_button_combo.currentTextChanged.connect(self._on_gamepad_button_changed)
        gamepad_selection_layout.addWidget(QLabel("Button:"))
        gamepad_selection_layout.addWidget(self.gamepad_button_combo)
        
        # Gamepad axis dropdown
        self.gamepad_axis_combo = QComboBox()
        self.gamepad_axis_combo.addItems([
            "Left Stick X", "Left Stick Y", "Right Stick X", "Right Stick Y",
            "Left Trigger", "Right Trigger"
        ])
        self.gamepad_axis_combo.currentTextChanged.connect(self._on_gamepad_axis_changed)
        gamepad_selection_layout.addWidget(QLabel("Axis:"))
        gamepad_selection_layout.addWidget(self.gamepad_axis_combo)
        
        self.gamepad_selection_widget.setLayout(gamepad_selection_layout)
        self.gamepad_selection_widget.hide()
        capture_layout.addWidget(self.gamepad_selection_widget)
        
        # Gamepad-specific controls
        self.gamepad_controls = QWidget()
        gamepad_layout = QFormLayout()
        
        self.gamepad_id_spin = QSpinBox()
        self.gamepad_id_spin.setMinimum(0)
        self.gamepad_id_spin.setMaximum(7)
        self.gamepad_id_spin.setToolTip("0 = any gamepad, 1-7 = specific gamepad")
        gamepad_layout.addRow("Gamepad ID:", self.gamepad_id_spin)
        
        self.axis_scale_spin = QDoubleSpinBox()
        self.axis_scale_spin.setMinimum(-10.0)
        self.axis_scale_spin.setMaximum(10.0)
        self.axis_scale_spin.setValue(1.0)
        self.axis_scale_spin.setSingleStep(0.1)
        self.axis_scale_spin.setToolTip("Scale factor for axis (negative to invert)")
        gamepad_layout.addRow("Axis Scale:", self.axis_scale_spin)
        
        self.gamepad_controls.setLayout(gamepad_layout)
        self.gamepad_controls.hide()
        capture_layout.addWidget(self.gamepad_controls)
        
        capture_group.setLayout(capture_layout)
        layout.addWidget(capture_group)
        
        # Buttons
        button_layout = QHBoxLayout()
        button_layout.addStretch()
        
        cancel_btn = QPushButton("Cancel")
        cancel_btn.setProperty("secondary", True)
        cancel_btn.clicked.connect(self.reject)
        button_layout.addWidget(cancel_btn)
        
        ok_btn = QPushButton("OK")
        ok_btn.setProperty("success", True)
        ok_btn.clicked.connect(self._on_accept)
        button_layout.addWidget(ok_btn)
        
        layout.addLayout(button_layout)
        
        self.setLayout(layout)
    
    def _load_binding(self):
        """Load existing binding data"""
        if not self.binding_data:
            # For new bindings, set initial dropdown values
            self._on_mouse_button_changed(self.mouse_button_combo.currentText())
            self._on_gamepad_button_changed(self.gamepad_button_combo.currentText())
            return
        
        device_type = self.binding_data.get("deviceType", 0)
        self.device_combo.setCurrentIndex(device_type)
        
        if device_type == 0:  # Keyboard
            key_name = self.binding_data.get("keyName", "")
            self.binding_label.setText(f"Key: {key_name}")
        elif device_type == 1:  # Mouse
            button_name = self.binding_data.get("buttonName", "")
            self.mouse_button_combo.setCurrentText(button_name)
            self.binding_label.setText(f"Mouse: {button_name}")
        elif device_type == 2:  # Gamepad
            if "buttonName" in self.binding_data:
                button_name = self.binding_data.get("buttonName", "")
                self.gamepad_button_combo.setCurrentText(button_name)
                self.binding_label.setText(f"Gamepad: {button_name}")
            elif "axisName" in self.binding_data:
                axis_name = self.binding_data.get("axisName", "")
                scale = self.binding_data.get("scale", 1.0)
                self.gamepad_axis_combo.setCurrentText(axis_name)
                self.axis_scale_spin.setValue(scale)
                self.binding_label.setText(f"Gamepad Axis: {axis_name} (scale: {scale})")
            
            gamepad_id = self.binding_data.get("gamepadID", 0)
            self.gamepad_id_spin.setValue(gamepad_id)
    
    def _on_device_changed(self, index):
        """Handle device type change"""
        # Show/hide appropriate input widgets
        self.keyboard_capture_widget.setVisible(index == 0)
        self.mouse_dropdown_widget.setVisible(index == 1)
        self.gamepad_selection_widget.setVisible(index >= 2)
        self.gamepad_controls.setVisible(index >= 2)
        
        # For gamepad, show button or axis dropdown based on selection
        if index == 2:  # Gamepad Button
            self.gamepad_button_combo.show()
            self.gamepad_axis_combo.hide()
            self.axis_scale_spin.setEnabled(False)
            # Initialize with current button
            self._on_gamepad_button_changed(self.gamepad_button_combo.currentText())
        elif index == 3:  # Gamepad Axis
            self.gamepad_button_combo.hide()
            self.gamepad_axis_combo.show()
            self.axis_scale_spin.setEnabled(True)
            # Initialize with current axis
            self._on_gamepad_axis_changed(self.gamepad_axis_combo.currentText())
        elif index == 1:  # Mouse
            # Initialize with current mouse button
            self._on_mouse_button_changed(self.mouse_button_combo.currentText())
        elif index == 0:  # Keyboard
            # Clear current binding
            self.binding_label.setText("No input assigned")
            self.captured_key = None
    
    def _start_capture(self):
        """Start capturing input"""
        self.is_capturing = True
        self.capture_btn.setText("Press any input...")
        self.capture_btn.setProperty("warning", True)
        self.capture_btn.style().unpolish(self.capture_btn)
        self.capture_btn.style().polish(self.capture_btn)
        self.setFocus()
    
    def _on_mouse_button_changed(self, button_name):
        """Handle mouse button selection"""
        if button_name:
            self.binding_label.setText(f"Mouse: {button_name}")
    
    def _on_gamepad_button_changed(self, button_name):
        """Handle gamepad button selection"""
        if button_name:
            self.binding_label.setText(f"Gamepad: {button_name}")
    
    def _on_gamepad_axis_changed(self, axis_name):
        """Handle gamepad axis selection"""
        if axis_name:
            scale = self.axis_scale_spin.value()
            self.binding_label.setText(f"Gamepad Axis: {axis_name} (scale: {scale})")
    
    def keyPressEvent(self, event: QKeyEvent):
        """Capture keyboard input"""
        if not self.is_capturing:
            super().keyPressEvent(event)
            return
        
        device_index = self.device_combo.currentIndex()
        if device_index != 0:  # Not keyboard
            super().keyPressEvent(event)
            return
        
        # Get key name
        key = event.key()
        key_text = event.text() or str(key)
        
        # Convert Qt key to readable name
        key_name = self._qt_key_to_name(key)
        
        self.captured_key = key
        self.binding_label.setText(f"Key: {key_name}")
        self.captured_device_type = 0
        
        self._stop_capture()
    
    def _stop_capture(self):
        """Stop capturing input"""
        self.is_capturing = False
        self.capture_btn.setText("Click and Press Key/Button")
        self.capture_btn.setProperty("warning", False)
        self.capture_btn.setProperty("accent", True)
        self.capture_btn.style().unpolish(self.capture_btn)
        self.capture_btn.style().polish(self.capture_btn)
    
    def _qt_key_to_name(self, key: int) -> str:
        """Convert Qt key code to readable name"""
        # Common mappings
        key_map = {
            Qt.Key.Key_Space: "Space",
            Qt.Key.Key_Return: "Enter",
            Qt.Key.Key_Enter: "Enter",
            Qt.Key.Key_Escape: "Escape",
            Qt.Key.Key_Tab: "Tab",
            Qt.Key.Key_Backspace: "Backspace",
            Qt.Key.Key_Delete: "Delete",
            Qt.Key.Key_Insert: "Insert",
            Qt.Key.Key_Home: "Home",
            Qt.Key.Key_End: "End",
            Qt.Key.Key_PageUp: "PageUp",
            Qt.Key.Key_PageDown: "PageDown",
            Qt.Key.Key_Left: "Left",
            Qt.Key.Key_Right: "Right",
            Qt.Key.Key_Up: "Up",
            Qt.Key.Key_Down: "Down",
            Qt.Key.Key_Shift: "Shift",
            Qt.Key.Key_Control: "Ctrl",
            Qt.Key.Key_Alt: "Alt",
            Qt.Key.Key_CapsLock: "CapsLock",
            Qt.Key.Key_F1: "F1",
            Qt.Key.Key_F2: "F2",
            Qt.Key.Key_F3: "F3",
            Qt.Key.Key_F4: "F4",
            Qt.Key.Key_F5: "F5",
            Qt.Key.Key_F6: "F6",
            Qt.Key.Key_F7: "F7",
            Qt.Key.Key_F8: "F8",
            Qt.Key.Key_F9: "F9",
            Qt.Key.Key_F10: "F10",
            Qt.Key.Key_F11: "F11",
            Qt.Key.Key_F12: "F12",
        }
        
        if key in key_map:
            return key_map[key]
        
        # For letter keys
        if Qt.Key.Key_A <= key <= Qt.Key.Key_Z:
            return chr(key)
        
        # For number keys
        if Qt.Key.Key_0 <= key <= Qt.Key.Key_9:
            return chr(key)
        
        return f"Key_{key}"
    
    def _qt_key_to_code(self, key: int) -> int:
        """Convert Qt key to engine key code (simplified mapping)"""
        # This is a simplified mapping - in production you'd want a complete mapping
        # For now, we'll just use the Qt key code directly
        # The engine should have its own key code enum
        return key
    
    def _on_accept(self):
        """Accept the dialog and build binding data"""
        device_index = self.device_combo.currentIndex()
        
        if device_index == 0:  # Keyboard
            if not self.captured_key:
                QMessageBox.warning(self, "No Input", "Please capture a key before saving.")
                return
            
            # Build keyboard binding
            key_name = self.binding_label.text().replace("Key: ", "")
            self.binding_data = {
                "deviceType": 0,
                "keyCode": self._qt_key_to_code(self.captured_key),
                "keyName": key_name
            }
        
        elif device_index == 1:  # Mouse
            button_name = self.mouse_button_combo.currentText()
            mouse_buttons = {"Left": 0, "Right": 1, "Middle": 2, "Button 4": 3, "Button 5": 4, "Scroll Up": 5, "Scroll Down": 6}
            self.binding_data = {
                "deviceType": 1,
                "mouseButton": mouse_buttons.get(button_name, 0),
                "buttonName": button_name
            }
        
        elif device_index == 2:  # Gamepad Button
            button_name = self.gamepad_button_combo.currentText()
            # Map button names to indices
            button_map = {
                "A (Cross)": 0, "B (Circle)": 1, "X (Square)": 2, "Y (Triangle)": 3,
                "Left Bumper": 4, "Right Bumper": 5, "Left Trigger": 6, "Right Trigger": 7,
                "Back (Select)": 8, "Start": 9, "Left Stick": 10, "Right Stick": 11,
                "D-Pad Up": 12, "D-Pad Down": 13, "D-Pad Left": 14, "D-Pad Right": 15
            }
            self.binding_data = {
                "deviceType": 2,
                "gamepadButton": button_map.get(button_name, 0),
                "buttonName": button_name,
                "gamepadID": self.gamepad_id_spin.value()
            }
        
        elif device_index == 3:  # Gamepad Axis
            axis_name = self.gamepad_axis_combo.currentText()
            # Map axis names to indices
            axis_map = {
                "Left Stick X": 0, "Left Stick Y": 1,
                "Right Stick X": 2, "Right Stick Y": 3,
                "Left Trigger": 4, "Right Trigger": 5
            }
            self.binding_data = {
                "deviceType": 2,
                "gamepadAxis": axis_map.get(axis_name, 0),
                "axisName": axis_name,
                "scale": self.axis_scale_spin.value(),
                "gamepadID": self.gamepad_id_spin.value()
            }
        
        self.accept()
    
    def get_binding_data(self) -> Dict:
        """Get the configured binding data"""
        return self.binding_data


class InputMappingDialog(QDialog):
    """Dialog for managing input actions and their bindings"""
    
    input_map_changed = pyqtSignal(dict)  # Emits the complete input map
    
    def __init__(self, input_map_data: Optional[Dict] = None, parent=None):
        super().__init__(parent)
        self.input_map = input_map_data or {"actions": [], "axes": []}
        self.has_unsaved_changes = False
        self.current_action_index = None  # Track by index instead of reference
        
        self.setWindowTitle("Input Map")
        self.setModal(True)
        self.setMinimumSize(900, 700)
        
        self._setup_ui()
        self._load_input_map()
    
    def _setup_ui(self):
        """Setup the dialog UI"""
        layout = QVBoxLayout()
        layout.setSpacing(5)
        
        # Title
        title_layout = QHBoxLayout()
        title_layout.setSpacing(10)
        title = QLabel("Input Map")
        title.setStyleSheet("font-size: 14px; font-weight: bold;")
        title.setMaximumHeight(30)
        title_layout.addWidget(title)
        title_layout.addStretch()
        
        # Help text
        help_label = QLabel("Define input actions that can be used in your game")
        help_label.setStyleSheet("color: rgba(255,255,255,0.6); font-size: 11px;")
        help_label.setMaximumHeight(30)
        title_layout.addWidget(help_label)
        
        # Wrap in a widget to control height
        title_widget = QWidget()
        title_widget.setLayout(title_layout)
        title_widget.setMaximumHeight(50)
        layout.addWidget(title_widget)
        
        # Main content splitter
        splitter = QSplitter(Qt.Orientation.Horizontal)
        
        # Left side - Actions list
        actions_widget = QWidget()
        actions_layout = QVBoxLayout()
        actions_layout.setContentsMargins(0, 0, 0, 0)
        
        actions_header = QHBoxLayout()
        actions_label = QLabel("Actions")
        actions_label.setStyleSheet("font-size: 14px; font-weight: bold;")
        actions_header.addWidget(actions_label)
        actions_header.addStretch()
        
        add_action_btn = QPushButton("+ Action")
        add_action_btn.setProperty("success", True)
        add_action_btn.clicked.connect(self._add_action)
        actions_header.addWidget(add_action_btn)
        
        actions_layout.addLayout(actions_header)
        
        # Actions tree
        self.actions_tree = QTreeWidget()
        self.actions_tree.setHeaderLabels(["Action / Binding", "Device", "Input"])
        self.actions_tree.setColumnWidth(0, 250)
        self.actions_tree.setColumnWidth(1, 100)
        self.actions_tree.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.actions_tree.customContextMenuRequested.connect(self._show_context_menu)
        self.actions_tree.itemDoubleClicked.connect(self._edit_item)
        self.actions_tree.currentItemChanged.connect(self._on_tree_selection_changed)

        # Custom styling for selected items
        self.actions_tree.setStyleSheet("""
            QTreeWidget::item:selected {
                background-color: #3d3d3d;
                color: white;
            }
            QTreeWidget::item:hover {
                background-color: #2d2d2d;
            }
        """)

        actions_layout.addWidget(self.actions_tree)
        
        actions_widget.setLayout(actions_layout)
        splitter.addWidget(actions_widget)
        
        # Right side - Action properties
        properties_widget = QWidget()
        properties_layout = QVBoxLayout()
        properties_layout.setContentsMargins(0, 0, 0, 0)
        
        properties_label = QLabel("Action Properties")
        properties_label.setStyleSheet("font-size: 14px; font-weight: bold;")
        properties_layout.addWidget(properties_label)
        
        # Properties form
        self.properties_group = QGroupBox("Settings")
        properties_form = QFormLayout()
        
        self.action_name_edit = QLineEdit()
        self.action_name_edit.setPlaceholderText("e.g. jump, attack, move_forward")
        self.action_name_edit.textChanged.connect(self._on_property_changed)
        properties_form.addRow("Action Name:", self.action_name_edit)
        
        self.deadzone_spin = QDoubleSpinBox()
        self.deadzone_spin.setMinimum(0.0)
        self.deadzone_spin.setMaximum(1.0)
        self.deadzone_spin.setValue(0.5)
        self.deadzone_spin.setSingleStep(0.05)
        self.deadzone_spin.setToolTip("Minimum input value to register as pressed")
        self.deadzone_spin.valueChanged.connect(self._on_property_changed)
        properties_form.addRow("Deadzone:", self.deadzone_spin)
        
        self.properties_group.setLayout(properties_form)
        self.properties_group.setEnabled(False)
        properties_layout.addWidget(self.properties_group)
        
        # Bindings management
        bindings_group = QGroupBox("Bindings")
        bindings_layout = QVBoxLayout()
        
        # Bindings list to show current action's bindings
        self.bindings_list = QTreeWidget()
        self.bindings_list.setHeaderLabels(["Device", "Input"])
        self.bindings_list.setMaximumHeight(200)
        self.bindings_list.setRootIsDecorated(False)
        self.bindings_list.setSelectionMode(QTreeWidget.SelectionMode.SingleSelection)
        self.bindings_list.itemDoubleClicked.connect(lambda item, col: self._edit_binding())

        # Custom styling for selected items
        self.bindings_list.setStyleSheet("""
            QTreeWidget::item:selected {
                background-color: #3d3d3d;
                color: white;
            }
            QTreeWidget::item:hover {
                background-color: #2d2d2d;
            }
        """)

        bindings_layout.addWidget(self.bindings_list)
        
        bindings_buttons = QHBoxLayout()
        
        add_binding_btn = QPushButton("+ Add Binding")
        add_binding_btn.setProperty("success", True)
        add_binding_btn.clicked.connect(self._add_binding)
        bindings_buttons.addWidget(add_binding_btn)
        
        edit_binding_btn = QPushButton("Edit")
        edit_binding_btn.clicked.connect(self._edit_binding)
        bindings_buttons.addWidget(edit_binding_btn)
        
        remove_binding_btn = QPushButton("Remove")
        remove_binding_btn.setProperty("error", True)
        remove_binding_btn.clicked.connect(self._remove_binding)
        bindings_buttons.addWidget(remove_binding_btn)
        
        bindings_layout.addLayout(bindings_buttons)
        bindings_group.setLayout(bindings_layout)
        properties_layout.addWidget(bindings_group)
        
        properties_layout.addStretch()
        
        properties_widget.setLayout(properties_layout)
        splitter.addWidget(properties_widget)
        
        splitter.setStretchFactor(0, 2)
        splitter.setStretchFactor(1, 1)
        
        layout.addWidget(splitter)
        
        # Bottom buttons
        button_layout = QHBoxLayout()
        
        # Left side buttons
        reset_btn = QPushButton("Reset to Default")
        reset_btn.setProperty("warning", True)
        reset_btn.clicked.connect(self._reset_to_default)
        button_layout.addWidget(reset_btn)
        
        button_layout.addStretch()
        
        # Right side buttons
        close_btn = QPushButton("Close")
        close_btn.setProperty("secondary", True)
        close_btn.clicked.connect(self._on_close)
        button_layout.addWidget(close_btn)
        
        save_btn = QPushButton("Save")
        save_btn.clicked.connect(self._on_save)
        button_layout.addWidget(save_btn)
        
        layout.addLayout(button_layout)
        
        self.setLayout(layout)
    
    def _load_input_map(self):
        """Load input map into the tree"""
        # Store current selection by index (more reliable than object reference)
        selected_action_index = self.current_action_index

        self.actions_tree.clear()

        # Load actions
        for i, action in enumerate(self.input_map.get("actions", [])):
            action_item = QTreeWidgetItem(self.actions_tree)
            action_item.setText(0, action.get("name", "Unnamed"))
            action_item.setData(0, Qt.ItemDataRole.UserRole, {"type": "action", "data": action, "index": i})
            action_item.setExpanded(True)

            # Add bindings as children
            for binding in action.get("bindings", []):
                binding_item = QTreeWidgetItem(action_item)
                device_name, input_name = self._get_binding_display_names(binding)
                binding_item.setText(0, "  → Binding")
                binding_item.setText(1, device_name)
                binding_item.setText(2, input_name)
                binding_item.setData(0, Qt.ItemDataRole.UserRole, {"type": "binding", "data": binding, "parent_action": action, "parent_index": i})

        # Restore selection by index
        if selected_action_index is not None and selected_action_index < self.actions_tree.topLevelItemCount():
            item_to_select = self.actions_tree.topLevelItem(selected_action_index)
            if item_to_select:
                self.actions_tree.setCurrentItem(item_to_select)
                self._load_action_properties(item_to_select)
    
    def _on_tree_selection_changed(self, current, previous):
        """Handle tree selection change"""
        if not current:
            self.properties_group.setEnabled(False)
            self.current_action_index = None
            return

        item_data = current.data(0, Qt.ItemDataRole.UserRole)
        if not item_data:
            self.properties_group.setEnabled(False)
            self.current_action_index = None
            return

        if item_data.get("type") == "action":
            self.current_action_index = item_data.get("index")
            self._load_action_properties(current)
        elif item_data.get("type") == "binding":
            # If a binding is selected, load the parent action's properties
            self.current_action_index = item_data.get("parent_index")
            parent = current.parent()
            if parent:
                parent_data = parent.data(0, Qt.ItemDataRole.UserRole)
                if parent_data and parent_data.get("type") == "action":
                    self._load_action_properties(parent)
            else:
                self.properties_group.setEnabled(False)
                self.current_action_index = None
    
    def _add_action(self):
        """Add a new action"""
        # Generate a unique action name
        base_name = "new_action"
        action_names = {action.get("name", "") for action in self.input_map.get("actions", [])}
        
        counter = 1
        new_name = base_name
        while new_name in action_names:
            new_name = f"{base_name}_{counter}"
            counter += 1
        
        # Create a new action with default values
        new_action = {
            "name": new_name,
            "deadzone": 0.5,
            "bindings": []
        }
        
        self.input_map.setdefault("actions", []).append(new_action)
        self._mark_unsaved()
        self._load_input_map()
        
        # Select the new action
        root = self.actions_tree.invisibleRootItem()
        if root.childCount() > 0:
            last_item = root.child(root.childCount() - 1)
            self.actions_tree.setCurrentItem(last_item)
            self._load_action_properties(last_item)
    
    def _add_binding(self):
        """Add a new binding to the selected action"""
        if self.current_action_index is None:
            QMessageBox.information(self, "No Selection", "Please select an action to add a binding.")
            return

        # Get the action data directly from input_map using current_action_index
        action_data = self.input_map["actions"][self.current_action_index]

        # Open binding editor
        dialog = InputBindingEditor(parent=self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            binding_data = dialog.get_binding_data()

            # Add binding to action
            if "bindings" not in action_data:
                action_data["bindings"] = []
            action_data["bindings"].append(binding_data)

            self._mark_unsaved()

            # Reload the tree to show the new binding (selection will be restored automatically)
            self._load_input_map()
    
    def _edit_binding(self):
        """Edit the selected binding"""
        binding_data = None

        # Try to get binding from the bindings_list first
        binding_item = self.bindings_list.currentItem()
        if binding_item:
            binding_data = binding_item.data(0, Qt.ItemDataRole.UserRole)
        else:
            # Fall back to tree selection
            current = self.actions_tree.currentItem()
            if current:
                item_data = current.data(0, Qt.ItemDataRole.UserRole)
                if item_data and item_data.get("type") == "binding":
                    binding_data = item_data["data"]

        if not binding_data:
            QMessageBox.information(self, "No Selection", "Please select a binding to edit.")
            return

        # Open binding editor with existing data
        dialog = InputBindingEditor(binding_data.copy(), parent=self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            updated_binding = dialog.get_binding_data()
            # Update the binding in place
            binding_data.clear()
            binding_data.update(updated_binding)
            self._mark_unsaved()
            self._load_input_map()
    
    def _remove_binding(self):
        """Remove the selected binding"""
        if self.current_action_index is None:
            QMessageBox.information(self, "No Selection", "Please select an action first.")
            return

        binding_data = None

        # Try to get binding from the bindings_list first
        binding_item = self.bindings_list.currentItem()
        if binding_item:
            binding_data = binding_item.data(0, Qt.ItemDataRole.UserRole)
        else:
            # Fall back to tree selection
            current = self.actions_tree.currentItem()
            if current:
                item_data = current.data(0, Qt.ItemDataRole.UserRole)
                if item_data and item_data.get("type") == "binding":
                    binding_data = item_data["data"]

        if not binding_data:
            QMessageBox.information(self, "No Selection", "Please select a binding to remove.")
            return

        reply = QMessageBox.question(self, "Remove Binding",
                                     "Are you sure you want to remove this binding?",
                                     QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)

        if reply == QMessageBox.StandardButton.Yes:
            action_data = self.input_map["actions"][self.current_action_index]
            if binding_data in action_data.get("bindings", []):
                action_data["bindings"].remove(binding_data)
                self._mark_unsaved()
                self._load_input_map()
    
    def _edit_item(self, item, column):
        """Handle double-click on item"""
        item_data = item.data(0, Qt.ItemDataRole.UserRole)
        if item_data.get("type") == "action":
            self._load_action_properties(item)
        elif item_data.get("type") == "binding":
            self._edit_binding()
    
    def _load_action_properties(self, item):
        """Load action properties into the properties panel"""
        if not item:
            self.bindings_list.clear()
            self.properties_group.setEnabled(False)
            return

        item_data = item.data(0, Qt.ItemDataRole.UserRole)
        if not item_data or item_data.get("type") != "action":
            self.properties_group.setEnabled(False)
            self.bindings_list.clear()
            return

        # Get the action index from item data (already set by _on_tree_selection_changed)
        action_index = item_data.get("index")
        if action_index is None or action_index >= len(self.input_map.get("actions", [])):
            self.properties_group.setEnabled(False)
            self.bindings_list.clear()
            return

        action_data = self.input_map["actions"][action_index]
        self.properties_group.setEnabled(True)

        # Disconnect signal temporarily to prevent recursive updates
        self.action_name_edit.textChanged.disconnect(self._on_property_changed)
        self.deadzone_spin.valueChanged.disconnect(self._on_property_changed)

        self.action_name_edit.setText(action_data.get("name", ""))
        self.deadzone_spin.setValue(action_data.get("deadzone", 0.5))

        # Reconnect signals
        self.action_name_edit.textChanged.connect(self._on_property_changed)
        self.deadzone_spin.valueChanged.connect(self._on_property_changed)

        # Populate bindings list
        self.bindings_list.clear()
        for binding in action_data.get("bindings", []):
            device_name, input_name = self._get_binding_display_names(binding)
            binding_item = QTreeWidgetItem(self.bindings_list)
            binding_item.setText(0, device_name)
            binding_item.setText(1, input_name)
            binding_item.setData(0, Qt.ItemDataRole.UserRole, binding)
    
    def _on_property_changed(self):
        """Handle property change"""
        if self.current_action_index is None:
            return

        # Update the action data directly in the input_map
        new_name = self.action_name_edit.text()
        new_deadzone = self.deadzone_spin.value()

        action = self.input_map["actions"][self.current_action_index]
        action["name"] = new_name
        action["deadzone"] = new_deadzone

        # Update tree item text directly without full reload
        current = self.actions_tree.currentItem()
        if current:
            item_data = current.data(0, Qt.ItemDataRole.UserRole)
            if item_data and item_data.get("type") == "action":
                current.setText(0, new_name)

        self._mark_unsaved()
    
    def _show_context_menu(self, position):
        """Show context menu for tree items"""
        item = self.actions_tree.itemAt(position)
        if not item:
            return
        
        menu = QMenu()
        
        item_data = item.data(0, Qt.ItemDataRole.UserRole)
        if item_data.get("type") == "action":
            add_binding_action = menu.addAction("Add Binding")
            add_binding_action.triggered.connect(lambda: self._context_add_binding(item))
            
            menu.addSeparator()
            
            duplicate_action = menu.addAction("Duplicate Action")
            duplicate_action.triggered.connect(lambda: self._duplicate_action(item))
            
            remove_action = menu.addAction("Remove Action")
            remove_action.triggered.connect(lambda: self._remove_action(item))
        elif item_data.get("type") == "binding":
            edit_action = menu.addAction("Edit Binding")
            edit_action.triggered.connect(self._edit_binding)
            
            remove_action = menu.addAction("Remove Binding")
            remove_action.triggered.connect(self._remove_binding)
        
        menu.exec(self.actions_tree.mapToGlobal(position))
    
    def _context_add_binding(self, item):
        """Add binding from context menu"""
        self.actions_tree.setCurrentItem(item)
        self._add_binding()
    
    def _duplicate_action(self, item):
        """Duplicate an action"""
        item_data = item.data(0, Qt.ItemDataRole.UserRole)
        if item_data.get("type") != "action":
            return
        
        import copy
        action_data = copy.deepcopy(item_data["data"])
        action_data["name"] = action_data["name"] + "_copy"
        
        self.input_map.setdefault("actions", []).append(action_data)
        self._mark_unsaved()
        self._load_input_map()
    
    def _remove_action(self, item):
        """Remove an action"""
        item_data = item.data(0, Qt.ItemDataRole.UserRole)
        if item_data.get("type") != "action":
            return
        
        action_data = item_data["data"]
        action_name = action_data.get("name", "Unknown")
        
        reply = QMessageBox.question(self, "Remove Action",
                                     f"Are you sure you want to remove the action '{action_name}'?",
                                     QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
        
        if reply == QMessageBox.StandardButton.Yes:
            self.input_map["actions"].remove(action_data)
            self._mark_unsaved()
            self._load_input_map()
    
    def _reset_to_default(self):
        """Reset input map to default"""
        reply = QMessageBox.question(self, "Reset to Default",
                                     "This will reset all input actions to default values. Continue?",
                                     QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
        
        if reply == QMessageBox.StandardButton.Yes:
            self.input_map = self._get_default_input_map()
            self._load_input_map()
            self._mark_unsaved()
    
    def _get_default_input_map(self) -> Dict:
        """Get default input map"""
        return {
            "actions": [
                {
                    "name": "ui_accept",
                    "deadzone": 0.5,
                    "bindings": [
                        {"deviceType": 0, "keyCode": 257, "keyName": "Enter"},
                        {"deviceType": 0, "keyCode": 32, "keyName": "Space"}
                    ]
                },
                {
                    "name": "ui_cancel",
                    "deadzone": 0.5,
                    "bindings": [
                        {"deviceType": 0, "keyCode": 256, "keyName": "Escape"}
                    ]
                },
                {
                    "name": "jump",
                    "deadzone": 0.5,
                    "bindings": [
                        {"deviceType": 0, "keyCode": 32, "keyName": "Space"}
                    ]
                }
            ],
            "axes": []
        }
    
    def _mark_unsaved(self):
        """Mark that there are unsaved changes"""
        self.has_unsaved_changes = True
        self.setWindowTitle("Input Map *")
    
    def _get_binding_display_names(self, binding: Dict) -> tuple:
        """Get display names for a binding"""
        device_type = binding.get("deviceType", 0)
        
        if device_type == 0:  # Keyboard
            device_name = "Keyboard"
            input_name = binding.get("keyName", "Unknown")
        elif device_type == 1:  # Mouse
            device_name = "Mouse"
            mouse_button = binding.get("mouseButton", 0)
            mouse_map = {0: "Left", 1: "Right", 2: "Middle", 3: "Scroll Up", 4: "Scroll Down"}
            input_name = mouse_map.get(mouse_button, "Unknown")
        elif device_type == 2:  # Gamepad (Button or Axis)
            # Check if it's an axis binding (has gamepadAxis field) or button binding
            if "gamepadAxis" in binding:
                device_name = "Gamepad Axis"
                axis = binding.get("gamepadAxis", 0)
                axis_map = {
                    0: "Left X", 1: "Left Y", 2: "Right X", 3: "Right Y",
                    4: "Left Trigger", 5: "Right Trigger"
                }
                input_name = axis_map.get(axis, f"Axis {axis}")
            else:
                device_name = "Gamepad"
                button = binding.get("gamepadButton", 0)
                button_map = {
                    0: "A/Cross", 1: "B/Circle", 2: "X/Square", 3: "Y/Triangle",
                    4: "Left Bumper", 5: "Right Bumper", 6: "Back/Select", 7: "Start",
                    8: "Left Stick", 9: "Right Stick", 10: "D-Pad Up", 11: "D-Pad Down",
                    12: "D-Pad Left", 13: "D-Pad Right"
                }
                input_name = button_map.get(button, f"Button {button}")
        elif device_type == 3:  # Gamepad Axis (legacy, should use deviceType 2)
            device_name = "Gamepad Axis"
            axis = binding.get("gamepadAxis", 0)
            axis_map = {
                0: "Left X", 1: "Left Y", 2: "Right X", 3: "Right Y",
                4: "Left Trigger", 5: "Right Trigger"
            }
            input_name = axis_map.get(axis, f"Axis {axis}")
        else:
            device_name = "Unknown"
            input_name = "Unknown"
        
        return device_name, input_name
    
    def _on_save(self):
        """Save the input map"""
        self.input_map_changed.emit(self.input_map)
        self.has_unsaved_changes = False
        self.setWindowTitle("Input Map")
        QMessageBox.information(self, "Saved", "Input map saved successfully!")
    
    def _on_close(self):
        """Handle close button"""
        if self.has_unsaved_changes:
            reply = QMessageBox.question(self, "Unsaved Changes",
                                        "You have unsaved changes. Close without saving?",
                                        QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
            if reply == QMessageBox.StandardButton.No:
                return
        
        self.accept()
    
    def get_input_map(self) -> Dict:
        """Get the current input map"""
        return self.input_map
