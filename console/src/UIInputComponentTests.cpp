/**
 * @file UIInputComponentTests.cpp
 * @brief Headless console tests for the interactive UI input components.
 *
 * These suites drive the pure state layer of the engine's editable / selectable UI
 * components (LineEdit, TextEdit, SpinBox, Slider, Checkbox, CheckList, RadioButton,
 * RadioList) without any GPU, window, font atlas, or live input-event dispatch. Each
 * component stores its configuration in the reflected property system, so the tests
 * call RegisterProperties() (which lazily runs DefineProperties()) before exercising
 * the public getters/setters and verifying value clamping, text/selection bookkeeping,
 * checked/selected state transitions, and the group/list container semantics — the
 * behavior a project relies on independently of rendering.
 */

#include "lupine/engine/Engine.hpp"
#include "lupine/components/LineEdit.hpp"
#include "lupine/components/TextEdit.hpp"
#include "lupine/components/SpinBox.hpp"
#include "lupine/components/Slider.hpp"
#include "lupine/components/Checkbox.hpp"
#include "lupine/components/CheckList.hpp"
#include "lupine/components/RadioButton.hpp"
#include "lupine/components/RadioList.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cmath>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

bool FloatEq(float a, float b, float tol = 1e-3f) {
    return std::fabs(a - b) < tol;
}

bool ColorEq(const math::Color& c, float r, float g, float b, float a, float tol = 1e-3f) {
    return FloatEq(c.r, r, tol) && FloatEq(c.g, g, tol) &&
           FloatEq(c.b, b, tol) && FloatEq(c.a, a, tol);
}

bool TestLineEdit() {
    TEST_SECTION("LineEdit: Text / Caret / Selection State");

    std::shared_ptr<LineEdit> edit = std::make_shared<LineEdit>();
    edit->RegisterProperties();

    TEST_ASSERT(edit->GetTypeName() == "LineEdit", "Type name is LineEdit");
    TEST_ASSERT(edit->GetText().empty(), "New LineEdit starts with empty text");

    edit->SetText("Hello");
    TEST_ASSERT(edit->GetText() == "Hello", "SetText/GetText round-trip");
    TEST_ASSERT(edit->GetCaretPosition() == 5, "Caret moves to end of text after SetText");

    edit->SetText("Hi");
    TEST_ASSERT(edit->GetText() == "Hi", "SetText replaces previous content");
    TEST_ASSERT(edit->GetCaretPosition() == 2, "Caret follows shorter replacement text");

    edit->SetCaretPosition(1);
    TEST_ASSERT(edit->GetCaretPosition() == 1, "SetCaretPosition sets a valid caret index");
    edit->SetCaretPosition(100);
    TEST_ASSERT(edit->GetCaretPosition() == 2, "Caret clamps to text length when too large");
    edit->SetCaretPosition(-10);
    TEST_ASSERT(edit->GetCaretPosition() == 0, "Caret clamps to zero when negative");

    TEST_ASSERT(!edit->HasSelection(), "No selection by default");
    TEST_ASSERT(edit->GetSelectedText().empty(), "Selected text empty without a selection");
    edit->SelectAll();
    TEST_ASSERT(edit->HasSelection(), "SelectAll creates a selection");
    TEST_ASSERT(edit->GetSelectedText() == "Hi", "SelectAll selects the whole string");
    TEST_ASSERT(edit->GetCaretPosition() == 2, "SelectAll leaves caret at the end");
    edit->Deselect();
    TEST_ASSERT(!edit->HasSelection(), "Deselect clears the selection");
    TEST_ASSERT(edit->GetSelectedText().empty(), "No selected text after Deselect");

    edit->SetText("");
    edit->SelectAll();
    TEST_ASSERT(!edit->HasSelection(), "SelectAll on empty text yields no selection");

    TEST_SECTION("LineEdit: Placeholder / Flags / Appearance");

    edit->SetPlaceholder("type here");
    TEST_ASSERT(edit->GetPlaceholder() == "type here", "Placeholder round-trips");

    TEST_ASSERT(edit->GetEditable(), "LineEdit is editable by default");
    edit->SetEditable(false);
    TEST_ASSERT(!edit->GetEditable(), "SetEditable(false) round-trips");
    edit->SetEditable(true);
    TEST_ASSERT(edit->GetEditable(), "SetEditable(true) round-trips");

    TEST_ASSERT(!edit->GetSecret(), "LineEdit is not secret by default");
    edit->SetSecret(true);
    TEST_ASSERT(edit->GetSecret(), "SetSecret round-trips");
    edit->SetSecret(false);

    edit->SetMaxLength(8);
    TEST_ASSERT(edit->GetMaxLength() == 8, "MaxLength round-trips");
    edit->SetMaxLength(0);
    TEST_ASSERT(edit->GetMaxLength() == 0, "MaxLength of zero (unlimited) round-trips");

    edit->SetFontSize(22.0f);
    TEST_ASSERT(FloatEq(edit->GetFontSize(), 22.0f), "Font size round-trips");
    edit->SetFontPath("res://fonts/test.ttf");
    TEST_ASSERT(edit->GetFontPath() == "res://fonts/test.ttf", "Font path round-trips");

    edit->SetFontColor(math::Color(0.1f, 0.2f, 0.3f, 0.4f));
    TEST_ASSERT(ColorEq(edit->GetFontColor(), 0.1f, 0.2f, 0.3f, 0.4f), "Font color round-trips");
    edit->SetBackgroundColor(math::Color(0.5f, 0.5f, 0.5f, 1.0f));
    TEST_ASSERT(ColorEq(edit->GetBackgroundColor(), 0.5f, 0.5f, 0.5f, 1.0f), "Background color round-trips");
    edit->SetSelectionColor(math::Color(0.2f, 0.4f, 0.8f, 0.6f));
    TEST_ASSERT(ColorEq(edit->GetSelectionColor(), 0.2f, 0.4f, 0.8f, 0.6f), "Selection color round-trips");
    edit->SetCaretColor(math::Color(1.0f, 0.0f, 0.0f, 1.0f));
    TEST_ASSERT(ColorEq(edit->GetCaretColor(), 1.0f, 0.0f, 0.0f, 1.0f), "Caret color round-trips");

    return true;
}

bool TestTextEdit() {
    TEST_SECTION("TextEdit: Multi-line Text / Caret / Selection");

    std::shared_ptr<TextEdit> edit = std::make_shared<TextEdit>();
    edit->RegisterProperties();

    TEST_ASSERT(edit->GetTypeName() == "TextEdit", "Type name is TextEdit");
    TEST_ASSERT(edit->GetText().empty(), "New TextEdit starts empty");

    edit->SetText("line one\nline two\nthree");
    TEST_ASSERT(edit->GetText() == "line one\nline two\nthree", "Multi-line text round-trips");
    TEST_ASSERT(edit->GetCaretPosition() == 23, "Caret moves to end after SetText");

    edit->SetCaretPosition(5);
    TEST_ASSERT(edit->GetCaretPosition() == 5, "SetCaretPosition within range");
    edit->SetCaretPosition(9999);
    TEST_ASSERT(edit->GetCaretPosition() == 23, "Caret clamps to length");
    edit->SetCaretPosition(-1);
    TEST_ASSERT(edit->GetCaretPosition() == 0, "Caret clamps to zero");

    edit->SelectAll();
    TEST_ASSERT(edit->HasSelection(), "SelectAll creates a multi-line selection");
    TEST_ASSERT(edit->GetSelectedText() == "line one\nline two\nthree",
                "SelectAll captures all lines including newlines");
    edit->Deselect();
    TEST_ASSERT(!edit->HasSelection(), "Deselect clears selection");

    TEST_SECTION("TextEdit: Flags / Spacing / Appearance");

    TEST_ASSERT(edit->GetEditable(), "TextEdit editable by default");
    edit->SetEditable(false);
    TEST_ASSERT(!edit->GetEditable(), "SetEditable round-trips");
    edit->SetEditable(true);

    edit->SetLineSpacing(1.5f);
    TEST_ASSERT(FloatEq(edit->GetLineSpacing(), 1.5f), "Line spacing round-trips");

    edit->SetFontSize(18.0f);
    TEST_ASSERT(FloatEq(edit->GetFontSize(), 18.0f), "Font size round-trips");
    edit->SetFontPath("res://fonts/mono.ttf");
    TEST_ASSERT(edit->GetFontPath() == "res://fonts/mono.ttf", "Font path round-trips");

    edit->SetFontColor(math::Color(0.9f, 0.8f, 0.7f, 1.0f));
    TEST_ASSERT(ColorEq(edit->GetFontColor(), 0.9f, 0.8f, 0.7f, 1.0f), "Font color round-trips");
    edit->SetBackgroundColor(math::Color(0.05f, 0.05f, 0.05f, 1.0f));
    TEST_ASSERT(ColorEq(edit->GetBackgroundColor(), 0.05f, 0.05f, 0.05f, 1.0f), "Background color round-trips");
    edit->SetSelectionColor(math::Color(0.1f, 0.3f, 0.9f, 0.5f));
    TEST_ASSERT(ColorEq(edit->GetSelectionColor(), 0.1f, 0.3f, 0.9f, 0.5f), "Selection color round-trips");
    edit->SetCaretColor(math::Color(0.0f, 1.0f, 0.0f, 1.0f));
    TEST_ASSERT(ColorEq(edit->GetCaretColor(), 0.0f, 1.0f, 0.0f, 1.0f), "Caret color round-trips");

    return true;
}

bool TestSpinBox() {
    TEST_SECTION("SpinBox: Value Range / Clamping");

    std::shared_ptr<SpinBox> spin = std::make_shared<SpinBox>();
    spin->RegisterProperties();

    TEST_ASSERT(spin->GetTypeName() == "SpinBox", "Type name is SpinBox");
    TEST_ASSERT(FloatEq(spin->GetMinValue(), 0.0f), "Default min value is 0");
    TEST_ASSERT(FloatEq(spin->GetMaxValue(), 100.0f), "Default max value is 100");

    spin->SetValue(42.0f);
    TEST_ASSERT(FloatEq(spin->GetValue(), 42.0f), "Value within range round-trips");

    spin->SetValue(250.0f);
    TEST_ASSERT(FloatEq(spin->GetValue(), 100.0f), "Value above max clamps to max");
    spin->SetValue(-50.0f);
    TEST_ASSERT(FloatEq(spin->GetValue(), 0.0f), "Value below min clamps to min");

    spin->SetMinValue(10.0f);
    spin->SetMaxValue(20.0f);
    TEST_ASSERT(FloatEq(spin->GetMinValue(), 10.0f), "Min value round-trips");
    TEST_ASSERT(FloatEq(spin->GetMaxValue(), 20.0f), "Max value round-trips");
    TEST_ASSERT(FloatEq(spin->GetValue(), 10.0f), "Existing value re-clamps when min raised above it");

    spin->SetValue(15.0f);
    TEST_ASSERT(FloatEq(spin->GetValue(), 15.0f), "Value inside the new range round-trips");
    spin->SetValue(99.0f);
    TEST_ASSERT(FloatEq(spin->GetValue(), 20.0f), "Value re-clamps to the new max");

    TEST_SECTION("SpinBox: Step / Decimals / Flags / Affixes");

    spin->SetStep(2.5f);
    TEST_ASSERT(FloatEq(spin->GetStep(), 2.5f), "Step round-trips");

    spin->SetDecimals(3);
    TEST_ASSERT(spin->GetDecimals() == 3, "Decimals round-trip");
    spin->SetDecimals(20);
    TEST_ASSERT(spin->GetDecimals() == 8, "Decimals clamp to upper bound of 8");
    spin->SetDecimals(-5);
    TEST_ASSERT(spin->GetDecimals() == 0, "Decimals clamp to lower bound of 0");

    TEST_ASSERT(spin->GetEditable(), "SpinBox editable by default");
    spin->SetEditable(false);
    TEST_ASSERT(!spin->GetEditable(), "SetEditable round-trips");
    spin->SetEditable(true);

    spin->SetPrefix("$");
    TEST_ASSERT(spin->GetPrefix() == "$", "Prefix round-trips");
    spin->SetSuffix(" kg");
    TEST_ASSERT(spin->GetSuffix() == " kg", "Suffix round-trips");

    spin->SetFontSize(14.0f);
    TEST_ASSERT(FloatEq(spin->GetFontSize(), 14.0f), "Font size round-trips");
    spin->SetFontPath("res://fonts/num.ttf");
    TEST_ASSERT(spin->GetFontPath() == "res://fonts/num.ttf", "Font path round-trips");

    spin->SetFontColor(math::Color(1.0f, 1.0f, 0.0f, 1.0f));
    TEST_ASSERT(ColorEq(spin->GetFontColor(), 1.0f, 1.0f, 0.0f, 1.0f), "Font color round-trips");
    spin->SetBackgroundColor(math::Color(0.2f, 0.2f, 0.2f, 1.0f));
    TEST_ASSERT(ColorEq(spin->GetBackgroundColor(), 0.2f, 0.2f, 0.2f, 1.0f), "Background color round-trips");
    spin->SetButtonColor(math::Color(0.4f, 0.4f, 0.4f, 1.0f));
    TEST_ASSERT(ColorEq(spin->GetButtonColor(), 0.4f, 0.4f, 0.4f, 1.0f), "Button color round-trips");

    return true;
}

bool TestSlider() {
    TEST_SECTION("Slider: Value Range / Clamping / Ratio");

    std::shared_ptr<Slider> slider = std::make_shared<Slider>();
    slider->RegisterProperties();

    TEST_ASSERT(slider->GetTypeName() == "Slider", "Type name is Slider");
    TEST_ASSERT(FloatEq(slider->GetMinValue(), 0.0f), "Default min value is 0");
    TEST_ASSERT(FloatEq(slider->GetMaxValue(), 100.0f), "Default max value is 100");

    slider->SetValue(25.0f);
    TEST_ASSERT(FloatEq(slider->GetValue(), 25.0f), "Value within range round-trips");
    TEST_ASSERT(FloatEq(slider->GetRatio(), 0.25f), "Ratio reflects value within range");

    slider->SetValue(500.0f);
    TEST_ASSERT(FloatEq(slider->GetValue(), 100.0f), "Value above max clamps to max");
    TEST_ASSERT(FloatEq(slider->GetRatio(), 1.0f), "Ratio is 1.0 at max");
    slider->SetValue(-100.0f);
    TEST_ASSERT(FloatEq(slider->GetValue(), 0.0f), "Value below min clamps to min");
    TEST_ASSERT(FloatEq(slider->GetRatio(), 0.0f), "Ratio is 0.0 at min");

    TEST_SECTION("Slider: Step Snapping");

    slider->SetStep(10.0f);
    TEST_ASSERT(FloatEq(slider->GetStep(), 10.0f), "Step round-trips");
    slider->SetValue(23.0f);
    TEST_ASSERT(FloatEq(slider->GetValue(), 20.0f), "Value snaps down to nearest step");
    slider->SetValue(26.0f);
    TEST_ASSERT(FloatEq(slider->GetValue(), 30.0f), "Value snaps up to nearest step");
    slider->SetStep(0.0f);
    slider->SetValue(37.0f);
    TEST_ASSERT(FloatEq(slider->GetValue(), 37.0f), "Zero step disables snapping");

    TEST_SECTION("Slider: Orientation / Editable / Appearance");

    TEST_ASSERT(slider->GetOrientation() == Slider::Orientation::Horizontal,
                "Default orientation is horizontal");
    slider->SetOrientation(Slider::Orientation::Vertical);
    TEST_ASSERT(slider->GetOrientation() == Slider::Orientation::Vertical,
                "Orientation round-trips to vertical");
    slider->SetOrientation(Slider::Orientation::Horizontal);
    TEST_ASSERT(slider->GetOrientation() == Slider::Orientation::Horizontal,
                "Orientation round-trips back to horizontal");

    TEST_ASSERT(slider->GetEditable(), "Slider editable by default");
    slider->SetEditable(false);
    TEST_ASSERT(!slider->GetEditable(), "SetEditable round-trips");
    slider->SetEditable(true);

    slider->SetTrackColor(math::Color(0.1f, 0.1f, 0.1f, 1.0f));
    TEST_ASSERT(ColorEq(slider->GetTrackColor(), 0.1f, 0.1f, 0.1f, 1.0f), "Track color round-trips");
    slider->SetFillColor(math::Color(0.3f, 0.6f, 1.0f, 1.0f));
    TEST_ASSERT(ColorEq(slider->GetFillColor(), 0.3f, 0.6f, 1.0f, 1.0f), "Fill color round-trips");
    slider->SetHandleColor(math::Color(0.9f, 0.9f, 0.9f, 1.0f));
    TEST_ASSERT(ColorEq(slider->GetHandleColor(), 0.9f, 0.9f, 0.9f, 1.0f), "Handle color round-trips");

    slider->SetTrackThickness(8.0f);
    TEST_ASSERT(FloatEq(slider->GetTrackThickness(), 8.0f), "Track thickness round-trips");
    slider->SetHandleRadius(12.0f);
    TEST_ASSERT(FloatEq(slider->GetHandleRadius(), 12.0f), "Handle radius round-trips");

    return true;
}

bool TestCheckbox() {
    TEST_SECTION("Checkbox: Checked State Transitions");

    std::shared_ptr<Checkbox> box = std::make_shared<Checkbox>();
    box->RegisterProperties();

    TEST_ASSERT(box->GetTypeName() == "Checkbox", "Type name is Checkbox");
    TEST_ASSERT(!box->IsChecked(), "Checkbox unchecked by default");
    TEST_ASSERT(box->GetCurrentState() == CheckboxState::Normal, "Default state is Normal");

    box->SetChecked(true);
    TEST_ASSERT(box->IsChecked(), "SetChecked(true) checks the box");
    TEST_ASSERT(box->GetCurrentState() == CheckboxState::Checked, "State becomes Checked");
    box->SetChecked(false);
    TEST_ASSERT(!box->IsChecked(), "SetChecked(false) unchecks the box");
    TEST_ASSERT(box->GetCurrentState() == CheckboxState::Normal, "State returns to Normal");

    box->Toggle();
    TEST_ASSERT(box->IsChecked(), "Toggle from unchecked checks the box");
    box->Toggle();
    TEST_ASSERT(!box->IsChecked(), "Toggle from checked unchecks the box");

    TEST_SECTION("Checkbox: Enabled / Group / Value");

    TEST_ASSERT(box->IsButtonEnabled(), "Checkbox enabled by default");
    box->SetEnabled(false);
    TEST_ASSERT(!box->IsButtonEnabled(), "SetEnabled(false) disables the checkbox");
    TEST_ASSERT(box->GetCurrentState() == CheckboxState::Disabled, "Disabled state reflected");
    box->SetEnabled(true);
    TEST_ASSERT(box->IsButtonEnabled(), "SetEnabled(true) re-enables the checkbox");

    box->SetGroupName("options");
    TEST_ASSERT(box->GetGroupName() == "options", "Group name round-trips");
    box->SetCheckboxValue(7);
    TEST_ASSERT(box->GetCheckboxValue() == 7, "Checkbox value round-trips");

    TEST_SECTION("Checkbox: Box / Text / Appearance");

    box->SetBoxSize(28.0f);
    TEST_ASSERT(FloatEq(box->GetBoxSize(), 28.0f), "Box size round-trips");
    box->SetCornerRadius(5.0f);
    TEST_ASSERT(FloatEq(box->GetCornerRadius(), 5.0f), "Corner radius round-trips");
    box->SetBorderWidth(2.0f);
    TEST_ASSERT(FloatEq(box->GetBorderWidth(), 2.0f), "Border width round-trips");

    box->SetText("Enable feature");
    TEST_ASSERT(box->GetText() == "Enable feature", "Text round-trips");
    box->SetTextOffset(6.0f);
    TEST_ASSERT(FloatEq(box->GetTextOffset(), 6.0f), "Text offset round-trips");
    box->SetFontSize(15.0f);
    TEST_ASSERT(FloatEq(box->GetFontSize(), 15.0f), "Font size round-trips");

    box->SetBorderColor(math::Color(0.7f, 0.7f, 0.7f, 1.0f));
    TEST_ASSERT(ColorEq(box->GetBorderColor(), 0.7f, 0.7f, 0.7f, 1.0f), "Border color round-trips");
    box->SetBackgroundColor(math::Color(0.15f, 0.15f, 0.15f, 1.0f));
    TEST_ASSERT(ColorEq(box->GetBackgroundColor(), 0.15f, 0.15f, 0.15f, 1.0f), "Background color round-trips");
    box->SetCheckmarkColor(math::Color(0.0f, 0.8f, 0.2f, 1.0f));
    TEST_ASSERT(ColorEq(box->GetCheckmarkColor(), 0.0f, 0.8f, 0.2f, 1.0f), "Checkmark color round-trips");
    box->SetFontColor(math::Color(0.95f, 0.95f, 0.95f, 1.0f));
    TEST_ASSERT(ColorEq(box->GetFontColor(), 0.95f, 0.95f, 0.95f, 1.0f), "Font color round-trips");

    box->SetStateModulation(CheckboxState::Hover, math::Color(0.5f, 0.6f, 0.7f, 1.0f));
    TEST_ASSERT(ColorEq(box->GetStateModulation(CheckboxState::Hover), 0.5f, 0.6f, 0.7f, 1.0f),
                "Per-state hover modulation round-trips");

    return true;
}

bool TestCheckListItems() {
    TEST_SECTION("CheckList: Item Management");

    std::shared_ptr<CheckList> list = std::make_shared<CheckList>();
    list->RegisterProperties();

    TEST_ASSERT(list->GetTypeName() == "CheckList", "Type name is CheckList");
    TEST_ASSERT(list->GetItemCount() == 0, "New CheckList has no items");

    list->AddItem("Apple");
    list->AddItem("Banana");
    list->AddItem("Cherry");
    TEST_ASSERT(list->GetItemCount() == 3, "AddItem grows the item list");
    std::vector<std::string> items = list->GetItems();
    TEST_ASSERT(items.size() == 3 && items[0] == "Apple" && items[2] == "Cherry",
                "GetItems returns items in insertion order");

    list->RemoveItem(1);
    TEST_ASSERT(list->GetItemCount() == 2, "RemoveItem shrinks the list");
    items = list->GetItems();
    TEST_ASSERT(items[0] == "Apple" && items[1] == "Cherry", "RemoveItem removes the correct entry");

    list->RemoveItem(99);
    TEST_ASSERT(list->GetItemCount() == 2, "RemoveItem with out-of-range index is a no-op");
    list->RemoveItem(-1);
    TEST_ASSERT(list->GetItemCount() == 2, "RemoveItem with negative index is a no-op");

    std::vector<std::string> replacement = {"One", "Two", "Three", "Four"};
    list->SetItems(replacement);
    TEST_ASSERT(list->GetItemCount() == 4, "SetItems replaces the entire list");
    TEST_ASSERT(list->GetItems()[3] == "Four", "SetItems preserves item order");

    list->ClearItems();
    TEST_ASSERT(list->GetItemCount() == 0, "ClearItems empties the list");

    TEST_SECTION("CheckList: Group / Layout Properties");

    list->SetGroupName("fruits");
    TEST_ASSERT(list->GetGroupName() == "fruits", "Group name round-trips");
    list->SetSpacing(12.0f);
    TEST_ASSERT(FloatEq(list->GetSpacing(), 12.0f), "Spacing round-trips");

    TEST_ASSERT(!list->GetAutoCreateCheckboxes(), "Auto-create defaults to false");
    list->SetAutoCreateCheckboxes(true);
    TEST_ASSERT(list->GetAutoCreateCheckboxes(), "Auto-create round-trips");
    list->SetAutoCreateCheckboxes(false);

    return true;
}

bool TestCheckListGroup() {
    TEST_SECTION("CheckList: Registered Checkbox Group Queries");

    std::shared_ptr<CheckList> list = std::make_shared<CheckList>();
    list->RegisterProperties();

    std::shared_ptr<Checkbox> a = std::make_shared<Checkbox>();
    std::shared_ptr<Checkbox> b = std::make_shared<Checkbox>();
    std::shared_ptr<Checkbox> c = std::make_shared<Checkbox>();
    a->RegisterProperties();
    b->RegisterProperties();
    c->RegisterProperties();
    a->SetCheckboxValue(1);
    b->SetCheckboxValue(2);
    c->SetCheckboxValue(3);

    list->RegisterCheckbox(a.get());
    list->RegisterCheckbox(b.get());
    list->RegisterCheckbox(c.get());
    TEST_ASSERT(list->GetTotalCount() == 3, "All registered checkboxes counted");
    list->RegisterCheckbox(a.get());
    TEST_ASSERT(list->GetTotalCount() == 3, "Re-registering a checkbox does not duplicate it");

    TEST_ASSERT(list->GetCheckedCount() == 0, "No checkboxes checked initially");
    TEST_ASSERT(!list->AreAllChecked(), "AreAllChecked false when none checked");
    TEST_ASSERT(list->AreAllUnchecked(), "AreAllUnchecked true when none checked");
    TEST_ASSERT(!list->IsAnyChecked(), "IsAnyChecked false when none checked");

    a->SetChecked(true);
    TEST_ASSERT(list->GetCheckedCount() == 1, "Checked count reflects a single checked box");
    TEST_ASSERT(list->IsAnyChecked(), "IsAnyChecked true after one is checked");
    TEST_ASSERT(!list->AreAllUnchecked(), "AreAllUnchecked false after one is checked");
    std::vector<int> checkedValues = list->GetCheckedValues();
    TEST_ASSERT(checkedValues.size() == 1 && checkedValues[0] == 1,
                "GetCheckedValues returns the value of the checked box");

    list->CheckAll();
    TEST_ASSERT(list->GetCheckedCount() == 3, "CheckAll checks every box");
    TEST_ASSERT(list->AreAllChecked(), "AreAllChecked true after CheckAll");
    TEST_ASSERT(list->GetCheckedCheckboxes().size() == 3, "GetCheckedCheckboxes returns all boxes");
    TEST_ASSERT(list->GetUncheckedCheckboxes().empty(), "No unchecked boxes after CheckAll");

    list->UncheckAll();
    TEST_ASSERT(list->GetCheckedCount() == 0, "UncheckAll clears every box");
    TEST_ASSERT(list->AreAllUnchecked(), "AreAllUnchecked true after UncheckAll");

    list->ToggleAll();
    TEST_ASSERT(list->GetCheckedCount() == 3, "ToggleAll from all-unchecked checks every box");
    list->ToggleAll();
    TEST_ASSERT(list->GetCheckedCount() == 0, "ToggleAll again unchecks every box");

    list->UnregisterCheckbox(b.get());
    TEST_ASSERT(list->GetTotalCount() == 2, "UnregisterCheckbox removes the box");

    return true;
}

bool TestRadioButton() {
    TEST_SECTION("RadioButton: Selection State");

    std::shared_ptr<RadioButton> radio = std::make_shared<RadioButton>();
    radio->RegisterProperties();

    TEST_ASSERT(radio->GetTypeName() == "RadioButton", "Type name is RadioButton");
    TEST_ASSERT(!radio->IsSelected(), "RadioButton unselected by default");
    TEST_ASSERT(radio->GetCurrentState() == RadioButtonState::Normal, "Default state is Normal");

    radio->SetSelected(true);
    TEST_ASSERT(radio->IsSelected(), "SetSelected(true) selects the button");
    TEST_ASSERT(radio->GetCurrentState() == RadioButtonState::Selected, "State becomes Selected");
    radio->SetSelected(false);
    TEST_ASSERT(!radio->IsSelected(), "SetSelected(false) deselects the button");
    TEST_ASSERT(radio->GetCurrentState() == RadioButtonState::Normal, "State returns to Normal");

    TEST_SECTION("RadioButton: Enabled / Group / Value");

    TEST_ASSERT(radio->IsButtonEnabled(), "RadioButton enabled by default");
    radio->SetEnabled(false);
    TEST_ASSERT(!radio->IsButtonEnabled(), "SetEnabled(false) disables the button");
    TEST_ASSERT(radio->GetCurrentState() == RadioButtonState::Disabled, "Disabled state reflected");
    radio->SetEnabled(true);
    TEST_ASSERT(radio->IsButtonEnabled(), "SetEnabled(true) re-enables the button");

    radio->SetGroupName("difficulty");
    TEST_ASSERT(radio->GetGroupName() == "difficulty", "Group name round-trips");
    radio->SetRadioValue(2);
    TEST_ASSERT(radio->GetRadioValue() == 2, "Radio value round-trips");

    TEST_SECTION("RadioButton: Indicator / Text / Appearance");

    radio->SetIndicatorSize(24.0f);
    TEST_ASSERT(FloatEq(radio->GetIndicatorSize(), 24.0f), "Indicator size round-trips");
    radio->SetInnerCircleScale(0.5f);
    TEST_ASSERT(FloatEq(radio->GetInnerCircleScale(), 0.5f), "Inner circle scale round-trips");
    radio->SetBorderWidth(2.0f);
    TEST_ASSERT(FloatEq(radio->GetBorderWidth(), 2.0f), "Border width round-trips");

    radio->SetText("Hard");
    TEST_ASSERT(radio->GetText() == "Hard", "Text round-trips");
    radio->SetTextOffset(5.0f);
    TEST_ASSERT(FloatEq(radio->GetTextOffset(), 5.0f), "Text offset round-trips");
    radio->SetFontSize(17.0f);
    TEST_ASSERT(FloatEq(radio->GetFontSize(), 17.0f), "Font size round-trips");

    radio->SetOuterCircleColor(math::Color(0.6f, 0.6f, 0.6f, 1.0f));
    TEST_ASSERT(ColorEq(radio->GetOuterCircleColor(), 0.6f, 0.6f, 0.6f, 1.0f), "Outer circle color round-trips");
    radio->SetBackgroundColor(math::Color(0.1f, 0.1f, 0.1f, 1.0f));
    TEST_ASSERT(ColorEq(radio->GetBackgroundColor(), 0.1f, 0.1f, 0.1f, 1.0f), "Background color round-trips");
    radio->SetInnerCircleColor(math::Color(0.2f, 0.8f, 1.0f, 1.0f));
    TEST_ASSERT(ColorEq(radio->GetInnerCircleColor(), 0.2f, 0.8f, 1.0f, 1.0f), "Inner circle color round-trips");
    radio->SetFontColor(math::Color(0.9f, 0.9f, 0.9f, 1.0f));
    TEST_ASSERT(ColorEq(radio->GetFontColor(), 0.9f, 0.9f, 0.9f, 1.0f), "Font color round-trips");

    radio->SetStateModulation(RadioButtonState::Hover, math::Color(0.4f, 0.5f, 0.6f, 1.0f));
    TEST_ASSERT(ColorEq(radio->GetStateModulation(RadioButtonState::Hover), 0.4f, 0.5f, 0.6f, 1.0f),
                "Per-state hover modulation round-trips");

    return true;
}

bool TestRadioListItems() {
    TEST_SECTION("RadioList: Item / Layout Properties");

    std::shared_ptr<RadioList> list = std::make_shared<RadioList>();
    list->RegisterProperties();

    TEST_ASSERT(list->GetTypeName() == "RadioList", "Type name is RadioList");
    TEST_ASSERT(list->GetItemCount() == 0, "New RadioList has no items");

    list->AddItem("Easy");
    list->AddItem("Normal");
    list->AddItem("Hard");
    TEST_ASSERT(list->GetItemCount() == 3, "AddItem grows the item list");
    std::vector<std::string> items = list->GetItems();
    TEST_ASSERT(items.size() == 3 && items[0] == "Easy" && items[2] == "Hard",
                "GetItems returns items in insertion order");

    list->RemoveItem(0);
    TEST_ASSERT(list->GetItemCount() == 2, "RemoveItem shrinks the list");
    TEST_ASSERT(list->GetItems()[0] == "Normal", "RemoveItem removes the correct entry");
    list->RemoveItem(50);
    TEST_ASSERT(list->GetItemCount() == 2, "RemoveItem with out-of-range index is a no-op");

    std::vector<std::string> replacement = {"A", "B"};
    list->SetItems(replacement);
    TEST_ASSERT(list->GetItemCount() == 2 && list->GetItems()[1] == "B",
                "SetItems replaces the list");
    list->ClearItems();
    TEST_ASSERT(list->GetItemCount() == 0, "ClearItems empties the list");

    list->SetGroupName("diff");
    TEST_ASSERT(list->GetGroupName() == "diff", "Group name round-trips");
    list->SetSpacing(18.0f);
    TEST_ASSERT(FloatEq(list->GetSpacing(), 18.0f), "Spacing round-trips");

    TEST_ASSERT(list->GetOrientation() == RadioList::Orientation::Vertical,
                "Default orientation is vertical");
    list->SetOrientation(RadioList::Orientation::Horizontal);
    TEST_ASSERT(list->GetOrientation() == RadioList::Orientation::Horizontal,
                "Orientation round-trips to horizontal");
    list->SetOrientation(RadioList::Orientation::Vertical);

    list->SetAutoCreateButtons(false);
    TEST_ASSERT(!list->GetAutoCreateButtons(), "Auto-create buttons round-trips");
    list->SetManageLayout(false);
    TEST_ASSERT(!list->GetManageLayout(), "Manage layout round-trips");

    return true;
}

bool TestRadioListSelection() {
    TEST_SECTION("RadioList: Mutual-Exclusion Selection");

    std::shared_ptr<RadioList> list = std::make_shared<RadioList>();
    list->RegisterProperties();

    std::shared_ptr<RadioButton> a = std::make_shared<RadioButton>();
    std::shared_ptr<RadioButton> b = std::make_shared<RadioButton>();
    std::shared_ptr<RadioButton> c = std::make_shared<RadioButton>();
    a->RegisterProperties();
    b->RegisterProperties();
    c->RegisterProperties();
    a->SetRadioValue(10);
    b->SetRadioValue(20);
    c->SetRadioValue(30);

    list->RegisterRadioButton(a.get());
    list->RegisterRadioButton(b.get());
    list->RegisterRadioButton(c.get());
    TEST_ASSERT(list->GetSelectedIndex() == -1, "Nothing selected initially");

    b->SetSelected(true);
    TEST_ASSERT(b->IsSelected(), "Selecting button B marks it selected");
    TEST_ASSERT(!a->IsSelected() && !c->IsSelected(), "Other buttons stay deselected");
    TEST_ASSERT(list->GetSelectedIndex() == 1, "List tracks the selected index");
    TEST_ASSERT(list->GetSelectedValue() == 20, "List tracks the selected value");

    c->SetSelected(true);
    TEST_ASSERT(c->IsSelected(), "Selecting button C marks it selected");
    TEST_ASSERT(!b->IsSelected(), "Previously selected button B is deselected (mutual exclusion)");
    TEST_ASSERT(list->GetSelectedIndex() == 2, "List index updates to the new selection");
    TEST_ASSERT(list->GetSelectedValue() == 30, "List value updates to the new selection");

    list->SetSelectedIndex(0);
    TEST_ASSERT(a->IsSelected(), "SetSelectedIndex selects the indexed button");
    TEST_ASSERT(!c->IsSelected(), "SetSelectedIndex deselects the previous button");
    TEST_ASSERT(list->GetSelectedValue() == 10, "SetSelectedIndex updates the selected value");

    list->SetSelectedValue(30);
    TEST_ASSERT(c->IsSelected(), "SetSelectedValue selects the matching button");
    TEST_ASSERT(!a->IsSelected(), "SetSelectedValue deselects the previous button");
    TEST_ASSERT(list->GetSelectedIndex() == 2, "SetSelectedValue updates the selected index");

    list->SetSelectedIndex(99);
    TEST_ASSERT(list->GetSelectedIndex() == 2, "Out-of-range SetSelectedIndex is rejected");

    list->SetSelectedIndex(-1);
    TEST_ASSERT(!a->IsSelected() && !b->IsSelected() && !c->IsSelected(),
                "SetSelectedIndex(-1) deselects all buttons");
    TEST_ASSERT(list->GetSelectedIndex() == -1, "Selected index reset to none");

    return true;
}

} // namespace

void RunUIInputComponentTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "UI INPUT COMPONENT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("UIInputComponents");

    engine::InitializeEngine();

    bool allPassed = true;
    allPassed &= TestLineEdit();
    allPassed &= TestTextEdit();
    allPassed &= TestSpinBox();
    allPassed &= TestSlider();
    allPassed &= TestCheckbox();
    allPassed &= TestCheckListItems();
    allPassed &= TestCheckListGroup();
    allPassed &= TestRadioButton();
    allPassed &= TestRadioListItems();
    allPassed &= TestRadioListSelection();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL UI INPUT COMPONENT TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
