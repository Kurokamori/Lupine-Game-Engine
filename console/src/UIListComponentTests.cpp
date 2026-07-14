/**
 * @file UIListComponentTests.cpp
 * @brief Headless console tests for the list / menu / text UI components.
 *
 * These components (ItemList, Dropdown, PopupMenu, RichTextLabel, Tree,
 * TabContainer) are all UIControl/Container-derived. Their collection and
 * selection state lives entirely in the component property registry and a few
 * runtime members, so it can be exercised without a GPU, window or input
 * pipeline. Each component's properties must be declared via DefineProperties()
 * before its accessors return meaningful values (GetPropertyValue returns a
 * default-constructed value for undeclared properties), so every test calls
 * DefineProperties() (and DefineSignals() where present) right after
 * construction, exactly as the engine does at registration time.
 *
 * Rendering, font/atlas access, popup focus grabbing and input dispatch are
 * intentionally NOT exercised here.
 */

#include "lupine/engine/Engine.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/components/ItemList.hpp"
#include "lupine/components/Dropdown.hpp"
#include "lupine/components/PopupMenu.hpp"
#include "lupine/components/RichTextLabel.hpp"
#include "lupine/components/Tree.hpp"
#include "lupine/components/TabContainer.hpp"
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

bool FloatEq(float a, float b, float tol = 1e-4f) {
    return std::fabs(a - b) < tol;
}

bool ColorEq(const math::Color& c, float r, float g, float b, float a, float tol = 1e-4f) {
    return FloatEq(c.r, r, tol) && FloatEq(c.g, g, tol) &&
           FloatEq(c.b, b, tol) && FloatEq(c.a, a, tol);
}

bool TestItemListItems() {
    TEST_SECTION("ItemList: Items Collection");

    std::shared_ptr<ItemList> list = std::make_shared<ItemList>();
    list->DefineProperties();
    list->DefineSignals();

    TEST_ASSERT(list->GetItemCount() == 0, "New ItemList is empty");
    TEST_ASSERT(list->GetItems().empty(), "GetItems is empty on a new list");

    list->AddItem("Alpha");
    list->AddItem("Beta");
    list->AddItem("Gamma");
    TEST_ASSERT(list->GetItemCount() == 3, "Three items added are counted");
    TEST_ASSERT(list->GetItem(0) == "Alpha", "First item text round-trips");
    TEST_ASSERT(list->GetItem(1) == "Beta", "Second item text round-trips");
    TEST_ASSERT(list->GetItem(2) == "Gamma", "Third item text round-trips");

    TEST_ASSERT(list->GetItem(-1).empty(), "Negative index returns empty string");
    TEST_ASSERT(list->GetItem(99).empty(), "Out-of-range index returns empty string");

    std::vector<std::string> items = list->GetItems();
    TEST_ASSERT(items.size() == 3, "GetItems returns all items");
    TEST_ASSERT(items[2] == "Gamma", "GetItems preserves order");

    list->RemoveItem(1);
    TEST_ASSERT(list->GetItemCount() == 2, "RemoveItem reduces the count");
    TEST_ASSERT(list->GetItem(0) == "Alpha", "Item before removed index unchanged");
    TEST_ASSERT(list->GetItem(1) == "Gamma", "Item after removed index shifts down");

    list->RemoveItem(-1);
    list->RemoveItem(50);
    TEST_ASSERT(list->GetItemCount() == 2, "Out-of-range RemoveItem is a no-op");

    list->ClearItems();
    TEST_ASSERT(list->GetItemCount() == 0, "ClearItems empties the list");

    std::vector<std::string> bulk = {"One", "Two", "Three", "Four"};
    list->SetItems(bulk);
    TEST_ASSERT(list->GetItemCount() == 4, "SetItems replaces all items");
    TEST_ASSERT(list->GetItem(3) == "Four", "SetItems preserves the last item");

    return true;
}

bool TestItemListSelection() {
    TEST_SECTION("ItemList: Selection");

    std::shared_ptr<ItemList> list = std::make_shared<ItemList>();
    list->DefineProperties();

    TEST_ASSERT(list->GetSelectedIndex() == -1, "New ItemList has no selection");

    list->SetItems({"A", "B", "C"});
    list->SetSelectedIndex(1);
    TEST_ASSERT(list->GetSelectedIndex() == 1, "Selection index round-trips");

    list->SetSelectedIndex(99);
    TEST_ASSERT(list->GetSelectedIndex() == 2, "Selection clamps to the last item");

    list->SetSelectedIndex(-5);
    TEST_ASSERT(list->GetSelectedIndex() == -1, "Negative selection collapses to -1");

    list->SetSelectedIndex(2);
    list->RemoveItem(2);
    TEST_ASSERT(list->GetSelectedIndex() == -1, "Removing the selected item clears selection");

    list->SetItems({"X", "Y", "Z"});
    list->SetSelectedIndex(2);
    list->RemoveItem(0);
    TEST_ASSERT(list->GetSelectedIndex() == 1, "Removing before selection shifts it down");

    list->SetSelectedIndex(0);
    list->ClearItems();
    TEST_ASSERT(list->GetSelectedIndex() == -1, "ClearItems clears the selection");

    return true;
}

bool TestItemListAppearance() {
    TEST_SECTION("ItemList: Appearance State");

    std::shared_ptr<ItemList> list = std::make_shared<ItemList>();
    list->DefineProperties();

    list->SetItemHeight(40.0f);
    TEST_ASSERT(FloatEq(list->GetItemHeight(), 40.0f), "Item height round-trips");

    list->SetFontPath("res://fonts/test.ttf");
    TEST_ASSERT(list->GetFontPath() == "res://fonts/test.ttf", "Font path round-trips");

    list->SetFontSize(22.0f);
    TEST_ASSERT(FloatEq(list->GetFontSize(), 22.0f), "Font size round-trips");

    list->SetFontColor(math::Color(0.1f, 0.2f, 0.3f, 0.4f));
    TEST_ASSERT(ColorEq(list->GetFontColor(), 0.1f, 0.2f, 0.3f, 0.4f), "Font color round-trips");

    list->SetBackgroundColor(math::Color(0.5f, 0.5f, 0.5f, 1.0f));
    TEST_ASSERT(ColorEq(list->GetBackgroundColor(), 0.5f, 0.5f, 0.5f, 1.0f), "Background color round-trips");

    list->SetSelectionColor(math::Color(0.9f, 0.1f, 0.1f, 0.5f));
    TEST_ASSERT(ColorEq(list->GetSelectionColor(), 0.9f, 0.1f, 0.1f, 0.5f), "Selection color round-trips");

    list->SetHoverColor(math::Color(0.2f, 0.2f, 0.8f, 0.3f));
    TEST_ASSERT(ColorEq(list->GetHoverColor(), 0.2f, 0.2f, 0.8f, 0.3f), "Hover color round-trips");

    return true;
}

bool TestDropdownItems() {
    TEST_SECTION("Dropdown: Items Collection");

    std::shared_ptr<Dropdown> dropdown = std::make_shared<Dropdown>();
    dropdown->DefineProperties();
    dropdown->DefineSignals();

    TEST_ASSERT(dropdown->GetItemCount() == 0, "New Dropdown is empty");
    TEST_ASSERT(!dropdown->IsOpen(), "New Dropdown is closed");

    dropdown->AddItem("Red");
    dropdown->AddItem("Green");
    dropdown->AddItem("Blue");
    TEST_ASSERT(dropdown->GetItemCount() == 3, "Items added are counted");
    TEST_ASSERT(dropdown->GetItem(1) == "Green", "Item text round-trips");
    TEST_ASSERT(dropdown->GetItem(-1).empty(), "Out-of-range item returns empty string");

    dropdown->RemoveItem(0);
    TEST_ASSERT(dropdown->GetItemCount() == 2, "RemoveItem reduces count");
    TEST_ASSERT(dropdown->GetItem(0) == "Green", "Remaining items shift down");

    dropdown->ClearItems();
    TEST_ASSERT(dropdown->GetItemCount() == 0, "ClearItems empties the dropdown");

    dropdown->SetItems({"Mon", "Tue", "Wed"});
    TEST_ASSERT(dropdown->GetItemCount() == 3, "SetItems replaces all items");

    return true;
}

bool TestDropdownSelection() {
    TEST_SECTION("Dropdown: Selection And Text");

    std::shared_ptr<Dropdown> dropdown = std::make_shared<Dropdown>();
    dropdown->DefineProperties();

    dropdown->SetPlaceholder("Choose...");
    TEST_ASSERT(dropdown->GetPlaceholder() == "Choose...", "Placeholder round-trips");
    TEST_ASSERT(dropdown->GetSelectedIndex() == -1, "No selection by default");
    TEST_ASSERT(dropdown->GetSelectedText() == "Choose...",
                "Selected text falls back to placeholder with no selection");

    dropdown->SetItems({"One", "Two", "Three"});
    dropdown->SetSelectedIndex(2);
    TEST_ASSERT(dropdown->GetSelectedIndex() == 2, "Selection index round-trips");
    TEST_ASSERT(dropdown->GetSelectedText() == "Three", "Selected text matches the chosen item");

    dropdown->SetSelectedIndex(100);
    TEST_ASSERT(dropdown->GetSelectedIndex() == 2, "Selection clamps to the last item");

    dropdown->SetSelectedIndex(-3);
    TEST_ASSERT(dropdown->GetSelectedIndex() == -1, "Negative selection collapses to -1");
    TEST_ASSERT(dropdown->GetSelectedText() == "Choose...",
                "Selected text returns to placeholder when cleared");

    dropdown->SetSelectedIndex(1);
    dropdown->RemoveItem(1);
    TEST_ASSERT(dropdown->GetSelectedIndex() == -1, "Removing selected item clears selection");

    return true;
}

bool TestDropdownAppearance() {
    TEST_SECTION("Dropdown: Appearance State");

    std::shared_ptr<Dropdown> dropdown = std::make_shared<Dropdown>();
    dropdown->DefineProperties();

    dropdown->SetItemHeight(30.0f);
    TEST_ASSERT(FloatEq(dropdown->GetItemHeight(), 30.0f), "Item height round-trips");

    dropdown->SetFontPath("res://fonts/ui.otf");
    TEST_ASSERT(dropdown->GetFontPath() == "res://fonts/ui.otf", "Font path round-trips");

    dropdown->SetFontSize(18.0f);
    TEST_ASSERT(FloatEq(dropdown->GetFontSize(), 18.0f), "Font size round-trips");

    dropdown->SetFontColor(math::Color(0.0f, 0.0f, 0.0f, 1.0f));
    TEST_ASSERT(ColorEq(dropdown->GetFontColor(), 0.0f, 0.0f, 0.0f, 1.0f), "Font color round-trips");

    dropdown->SetBackgroundColor(math::Color(0.3f, 0.3f, 0.3f, 1.0f));
    TEST_ASSERT(ColorEq(dropdown->GetBackgroundColor(), 0.3f, 0.3f, 0.3f, 1.0f), "Background color round-trips");

    dropdown->SetListColor(math::Color(0.15f, 0.15f, 0.15f, 1.0f));
    TEST_ASSERT(ColorEq(dropdown->GetListColor(), 0.15f, 0.15f, 0.15f, 1.0f), "List color round-trips");

    dropdown->SetHoverColor(math::Color(0.4f, 0.4f, 0.7f, 0.5f));
    TEST_ASSERT(ColorEq(dropdown->GetHoverColor(), 0.4f, 0.4f, 0.7f, 0.5f), "Hover color round-trips");

    return true;
}

bool TestPopupMenuItems() {
    TEST_SECTION("PopupMenu: Items Collection");

    std::shared_ptr<PopupMenu> menu = std::make_shared<PopupMenu>();
    menu->DefineProperties();
    menu->DefineSignals();

    TEST_ASSERT(menu->GetItemCount() == 0, "New PopupMenu is empty");
    TEST_ASSERT(!menu->IsOpen(), "New PopupMenu is closed");

    menu->AddItem("Cut");
    menu->AddItem("Copy");
    menu->AddItem("Paste");
    TEST_ASSERT(menu->GetItemCount() == 3, "Entries added are counted");
    TEST_ASSERT(menu->GetItem(0) == "Cut", "First entry round-trips");
    TEST_ASSERT(menu->GetItem(2) == "Paste", "Last entry round-trips");
    TEST_ASSERT(menu->GetItem(7).empty(), "Out-of-range entry returns empty string");

    menu->RemoveItem(1);
    TEST_ASSERT(menu->GetItemCount() == 2, "RemoveItem reduces count");
    TEST_ASSERT(menu->GetItem(1) == "Paste", "Entries shift down after removal");

    menu->RemoveItem(99);
    TEST_ASSERT(menu->GetItemCount() == 2, "Out-of-range RemoveItem is a no-op");

    menu->ClearItems();
    TEST_ASSERT(menu->GetItemCount() == 0, "ClearItems empties the menu");

    menu->SetItems({"New", "Open", "Save", "Quit"});
    TEST_ASSERT(menu->GetItemCount() == 4, "SetItems replaces all entries");
    TEST_ASSERT(menu->GetItem(3) == "Quit", "SetItems preserves the last entry");

    return true;
}

bool TestPopupMenuAppearance() {
    TEST_SECTION("PopupMenu: Appearance State");

    std::shared_ptr<PopupMenu> menu = std::make_shared<PopupMenu>();
    menu->DefineProperties();

    menu->SetItemHeight(26.0f);
    TEST_ASSERT(FloatEq(menu->GetItemHeight(), 26.0f), "Item height round-trips");

    menu->SetFontPath("res://fonts/menu.ttf");
    TEST_ASSERT(menu->GetFontPath() == "res://fonts/menu.ttf", "Font path round-trips");

    menu->SetFontSize(15.0f);
    TEST_ASSERT(FloatEq(menu->GetFontSize(), 15.0f), "Font size round-trips");

    menu->SetFontColor(math::Color(1.0f, 1.0f, 1.0f, 1.0f));
    TEST_ASSERT(ColorEq(menu->GetFontColor(), 1.0f, 1.0f, 1.0f, 1.0f), "Font color round-trips");

    menu->SetBackgroundColor(math::Color(0.08f, 0.08f, 0.08f, 0.95f));
    TEST_ASSERT(ColorEq(menu->GetBackgroundColor(), 0.08f, 0.08f, 0.08f, 0.95f), "Background color round-trips");

    menu->SetHoverColor(math::Color(0.25f, 0.45f, 0.85f, 0.6f));
    TEST_ASSERT(ColorEq(menu->GetHoverColor(), 0.25f, 0.45f, 0.85f, 0.6f), "Hover color round-trips");

    return true;
}

bool TestRichTextLabelContent() {
    TEST_SECTION("RichTextLabel: Content And BBCode");

    std::shared_ptr<RichTextLabel> label = std::make_shared<RichTextLabel>();
    label->DefineProperties();

    TEST_ASSERT(label->GetText().empty(), "New RichTextLabel has empty text");

    label->SetText("Plain text");
    TEST_ASSERT(label->GetText() == "Plain text", "Plain text round-trips");

    const std::string bbcode = "[b]Bold[/b] and [color=#ff0000]red[/color] and [size=24]big[/size]";
    label->SetText(bbcode);
    TEST_ASSERT(label->GetText() == bbcode, "BBCode markup is preserved verbatim");

    label->SetText("");
    TEST_ASSERT(label->GetText().empty(), "Text can be cleared");

    return true;
}

bool TestRichTextLabelFormatting() {
    TEST_SECTION("RichTextLabel: Formatting State");

    std::shared_ptr<RichTextLabel> label = std::make_shared<RichTextLabel>();
    label->DefineProperties();

    label->SetFontPath("res://fonts/body.ttf");
    TEST_ASSERT(label->GetFontPath() == "res://fonts/body.ttf", "Font path round-trips");

    label->SetFontSize(28.0f);
    TEST_ASSERT(FloatEq(label->GetFontSize(), 28.0f), "Font size round-trips");

    label->SetColor(math::Color(0.2f, 0.6f, 0.9f, 1.0f));
    TEST_ASSERT(ColorEq(label->GetColor(), 0.2f, 0.6f, 0.9f, 1.0f), "Default text color round-trips");

    label->SetWordWrap(true);
    TEST_ASSERT(label->GetWordWrap(), "Word wrap can be enabled");
    label->SetWordWrap(false);
    TEST_ASSERT(!label->GetWordWrap(), "Word wrap can be disabled");

    label->SetHorizontalAlign(TextHAlign::Center);
    TEST_ASSERT(label->GetHorizontalAlign() == TextHAlign::Center, "Center alignment round-trips");
    label->SetHorizontalAlign(TextHAlign::Right);
    TEST_ASSERT(label->GetHorizontalAlign() == TextHAlign::Right, "Right alignment round-trips");
    label->SetHorizontalAlign(TextHAlign::Left);
    TEST_ASSERT(label->GetHorizontalAlign() == TextHAlign::Left, "Left alignment round-trips");

    label->SetLineSpacing(6.0f);
    TEST_ASSERT(FloatEq(label->GetLineSpacing(), 6.0f), "Line spacing round-trips");

    return true;
}

bool TestTreeHierarchy() {
    TEST_SECTION("Tree: Hierarchy And Items");

    std::shared_ptr<Tree> tree = std::make_shared<Tree>();
    tree->DefineProperties();
    tree->DefineSignals();

    TEST_ASSERT(tree->GetItemCount() == 0, "New Tree is empty");

    tree->AddItem("Root", 0);
    tree->AddItem("Child A", 1);
    tree->AddItem("Grandchild", 2);
    tree->AddItem("Child B", 1);
    TEST_ASSERT(tree->GetItemCount() == 4, "Items added are counted");

    TEST_ASSERT(tree->GetItemText(0) == "Root", "Root item text round-trips");
    TEST_ASSERT(tree->GetItemText(2) == "Grandchild", "Nested item text round-trips");
    TEST_ASSERT(tree->GetItemText(99).empty(), "Out-of-range item text returns empty string");

    TEST_ASSERT(tree->GetItemDepth(0) == 0, "Root depth is zero");
    TEST_ASSERT(tree->GetItemDepth(1) == 1, "First child depth is one");
    TEST_ASSERT(tree->GetItemDepth(2) == 2, "Grandchild depth is two");
    TEST_ASSERT(tree->GetItemDepth(3) == 1, "Second child depth is one");
    TEST_ASSERT(tree->GetItemDepth(-1) == 0, "Out-of-range depth defaults to zero");

    tree->AddItem("Negative", -5);
    TEST_ASSERT(tree->GetItemDepth(4) == 0, "Negative depth is clamped to zero");

    tree->ClearItems();
    TEST_ASSERT(tree->GetItemCount() == 0, "ClearItems empties the tree");

    return true;
}

bool TestTreeExpansion() {
    TEST_SECTION("Tree: Expand / Collapse");

    std::shared_ptr<Tree> tree = std::make_shared<Tree>();
    tree->DefineProperties();

    tree->AddItem("Branch", 0);
    tree->AddItem("Leaf 1", 1);
    tree->AddItem("Leaf 2", 1);

    TEST_ASSERT(tree->IsExpanded(0), "Items are expanded by default");

    tree->SetExpanded(0, false);
    TEST_ASSERT(!tree->IsExpanded(0), "SetExpanded false collapses an item");

    tree->SetExpanded(0, true);
    TEST_ASSERT(tree->IsExpanded(0), "SetExpanded true expands an item");

    tree->ToggleExpand(0);
    TEST_ASSERT(!tree->IsExpanded(0), "ToggleExpand collapses an expanded item");
    tree->ToggleExpand(0);
    TEST_ASSERT(tree->IsExpanded(0), "ToggleExpand re-expands a collapsed item");

    TEST_ASSERT(tree->IsExpanded(-1), "Out-of-range index reports expanded (default true)");

    return true;
}

bool TestTreeSelection() {
    TEST_SECTION("Tree: Selection");

    std::shared_ptr<Tree> tree = std::make_shared<Tree>();
    tree->DefineProperties();

    TEST_ASSERT(tree->GetSelectedIndex() == -1, "New Tree has no selection");

    tree->AddItem("A", 0);
    tree->AddItem("B", 1);
    tree->AddItem("C", 0);

    tree->SetSelectedIndex(1);
    TEST_ASSERT(tree->GetSelectedIndex() == 1, "Selection index round-trips");

    tree->SetSelectedIndex(50);
    TEST_ASSERT(tree->GetSelectedIndex() == 2, "Selection clamps to the last item");

    tree->SetSelectedIndex(-10);
    TEST_ASSERT(tree->GetSelectedIndex() == -1, "Negative selection collapses to -1");

    tree->SetSelectedIndex(0);
    tree->ClearItems();
    TEST_ASSERT(tree->GetSelectedIndex() == -1, "ClearItems clears the selection");

    return true;
}

bool TestTreeAppearance() {
    TEST_SECTION("Tree: Appearance State");

    std::shared_ptr<Tree> tree = std::make_shared<Tree>();
    tree->DefineProperties();

    tree->SetItemHeight(28.0f);
    TEST_ASSERT(FloatEq(tree->GetItemHeight(), 28.0f), "Item height round-trips");

    tree->SetIndentWidth(20.0f);
    TEST_ASSERT(FloatEq(tree->GetIndentWidth(), 20.0f), "Indent width round-trips");

    tree->SetFontPath("res://fonts/tree.ttf");
    TEST_ASSERT(tree->GetFontPath() == "res://fonts/tree.ttf", "Font path round-trips");

    tree->SetFontSize(14.0f);
    TEST_ASSERT(FloatEq(tree->GetFontSize(), 14.0f), "Font size round-trips");

    tree->SetFontColor(math::Color(0.9f, 0.9f, 0.9f, 1.0f));
    TEST_ASSERT(ColorEq(tree->GetFontColor(), 0.9f, 0.9f, 0.9f, 1.0f), "Font color round-trips");

    tree->SetBackgroundColor(math::Color(0.1f, 0.1f, 0.12f, 1.0f));
    TEST_ASSERT(ColorEq(tree->GetBackgroundColor(), 0.1f, 0.1f, 0.12f, 1.0f), "Background color round-trips");

    tree->SetSelectionColor(math::Color(0.2f, 0.5f, 0.9f, 0.7f));
    TEST_ASSERT(ColorEq(tree->GetSelectionColor(), 0.2f, 0.5f, 0.9f, 0.7f), "Selection color round-trips");

    return true;
}

bool TestTabContainerTabs() {
    TEST_SECTION("TabContainer: Tabs From Child Nodes");

    std::shared_ptr<Node> host = std::make_shared<Node>("TabHost");
    std::shared_ptr<TabContainer> tabs = std::make_shared<TabContainer>();
    host->AddComponent(tabs);
    tabs->DefineProperties();
    tabs->DefineSignals();

    TEST_ASSERT(tabs->GetTabCount() == 0, "TabContainer with no child pages has no tabs");
    TEST_ASSERT(tabs->GetTabTitle(0).empty(), "Tab title out of range returns empty string");

    host->AddChild(std::make_shared<Node>("General"));
    host->AddChild(std::make_shared<Node>("Audio"));
    host->AddChild(std::make_shared<Node>("Video"));

    TEST_ASSERT(tabs->GetTabCount() == 3, "Each child node is a tab");
    TEST_ASSERT(tabs->GetTabTitle(0) == "General", "First tab title is the child node name");
    TEST_ASSERT(tabs->GetTabTitle(1) == "Audio", "Second tab title is the child node name");
    TEST_ASSERT(tabs->GetTabTitle(2) == "Video", "Third tab title is the child node name");
    TEST_ASSERT(tabs->GetTabTitle(-1).empty(), "Negative tab title returns empty string");
    TEST_ASSERT(tabs->GetTabTitle(5).empty(), "Out-of-range tab title returns empty string");

    return true;
}

bool TestTabContainerSelection() {
    TEST_SECTION("TabContainer: Current Tab");

    std::shared_ptr<Node> host = std::make_shared<Node>("TabHost2");
    std::shared_ptr<TabContainer> tabs = std::make_shared<TabContainer>();
    host->AddComponent(tabs);
    tabs->DefineProperties();
    tabs->DefineSignals();

    TEST_ASSERT(tabs->GetCurrentTab() == 0, "Default current tab is zero");

    host->AddChild(std::make_shared<Node>("First"));
    host->AddChild(std::make_shared<Node>("Second"));
    host->AddChild(std::make_shared<Node>("Third"));

    tabs->SetCurrentTab(2);
    TEST_ASSERT(tabs->GetCurrentTab() == 2, "Current tab index round-trips");

    tabs->SetCurrentTab(99);
    TEST_ASSERT(tabs->GetCurrentTab() == 2, "Current tab clamps to the last tab");

    tabs->SetCurrentTab(0);
    TEST_ASSERT(tabs->GetCurrentTab() == 0, "Current tab can return to the first tab");

    return true;
}

bool TestTabContainerAppearance() {
    TEST_SECTION("TabContainer: Appearance State");

    std::shared_ptr<TabContainer> tabs = std::make_shared<TabContainer>();
    tabs->DefineProperties();

    tabs->SetTabHeight(36.0f);
    TEST_ASSERT(FloatEq(tabs->GetTabHeight(), 36.0f), "Tab height round-trips");

    tabs->SetFontPath("res://fonts/tabs.ttf");
    TEST_ASSERT(tabs->GetFontPath() == "res://fonts/tabs.ttf", "Font path round-trips");

    tabs->SetFontSize(17.0f);
    TEST_ASSERT(FloatEq(tabs->GetFontSize(), 17.0f), "Font size round-trips");

    tabs->SetFontColor(math::Color(0.95f, 0.95f, 0.95f, 1.0f));
    TEST_ASSERT(ColorEq(tabs->GetFontColor(), 0.95f, 0.95f, 0.95f, 1.0f), "Font color round-trips");

    tabs->SetTabColor(math::Color(0.2f, 0.2f, 0.2f, 1.0f));
    TEST_ASSERT(ColorEq(tabs->GetTabColor(), 0.2f, 0.2f, 0.2f, 1.0f), "Tab color round-trips");

    tabs->SetActiveTabColor(math::Color(0.3f, 0.5f, 0.8f, 1.0f));
    TEST_ASSERT(ColorEq(tabs->GetActiveTabColor(), 0.3f, 0.5f, 0.8f, 1.0f), "Active tab color round-trips");

    tabs->SetTabBarColor(math::Color(0.12f, 0.12f, 0.12f, 1.0f));
    TEST_ASSERT(ColorEq(tabs->GetTabBarColor(), 0.12f, 0.12f, 0.12f, 1.0f), "Tab bar color round-trips");

    return true;
}

} // namespace

void RunUIListComponentTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "UI LIST COMPONENT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("UIListComponents");

    engine::InitializeEngine();

    bool allPassed = true;
    allPassed &= TestItemListItems();
    allPassed &= TestItemListSelection();
    allPassed &= TestItemListAppearance();
    allPassed &= TestDropdownItems();
    allPassed &= TestDropdownSelection();
    allPassed &= TestDropdownAppearance();
    allPassed &= TestPopupMenuItems();
    allPassed &= TestPopupMenuAppearance();
    allPassed &= TestRichTextLabelContent();
    allPassed &= TestRichTextLabelFormatting();
    allPassed &= TestTreeHierarchy();
    allPassed &= TestTreeExpansion();
    allPassed &= TestTreeSelection();
    allPassed &= TestTreeAppearance();
    allPassed &= TestTabContainerTabs();
    allPassed &= TestTabContainerSelection();
    allPassed &= TestTabContainerAppearance();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL UI LIST COMPONENT TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
