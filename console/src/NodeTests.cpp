#include "lupine/core/Node.hpp"
#include "lupine/core/Component.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <memory>
#include <string>

using namespace lupine;
using namespace lupine::core;

namespace {

class MarkerComponent : public Component {
public:
    MarkerComponent() : Component("MarkerComponent") {}
    std::string GetTypeName() const override { return "MarkerComponent"; }
    int tag = 0;
};

class OtherComponent : public Component {
public:
    OtherComponent() : Component("OtherComponent") {}
    std::string GetTypeName() const override { return "OtherComponent"; }
};

std::shared_ptr<Node> MakeNode(const std::string& name) {
    return std::make_shared<Node>(name);
}

bool TestConstruction() {
    TEST_SECTION("Node Construction Tests");

    std::shared_ptr<Node> node = MakeNode("Root");
    TEST_ASSERT(node->GetName() == "Root", "Node retains its name");
    TEST_ASSERT(node->GetUUID().IsValid(), "Node has a valid UUID");
    TEST_ASSERT(node->GetParent() == nullptr, "New node has no parent");
    TEST_ASSERT(node->GetChildCount() == 0, "New node has no children");
    TEST_ASSERT(node->IsActive(), "Node is active by default");
    TEST_ASSERT(node->IsVisible(), "Node is visible by default");

    std::shared_ptr<Node> other = MakeNode("Root");
    TEST_ASSERT(node->GetUUID() != other->GetUUID(), "Two nodes have distinct UUIDs");

    node->SetName("Renamed");
    TEST_ASSERT(node->GetName() == "Renamed", "Node name can be changed");

    return true;
}

bool TestHierarchy() {
    TEST_SECTION("Hierarchy Tests");

    std::shared_ptr<Node> parent = MakeNode("Parent");
    std::shared_ptr<Node> a = MakeNode("A");
    std::shared_ptr<Node> b = MakeNode("B");

    parent->AddChild(a);
    parent->AddChild(b);

    TEST_ASSERT(parent->GetChildCount() == 2, "Parent has two children after AddChild");
    TEST_ASSERT(a->GetParent() == parent.get(), "Child A reports correct parent");
    TEST_ASSERT(b->GetParent() == parent.get(), "Child B reports correct parent");
    TEST_ASSERT(parent->GetChild("A") == a, "GetChild by name returns A");
    TEST_ASSERT(parent->GetChild(static_cast<size_t>(1)) == b, "GetChild by index returns B");
    TEST_ASSERT(parent->GetChild("missing") == nullptr, "GetChild for unknown name is null");
    TEST_ASSERT(parent->GetChildren().size() == 2, "GetChildren returns both children");

    std::shared_ptr<Node> c = MakeNode("C");
    parent->InsertChild(c, 1);
    TEST_ASSERT(parent->GetChild(static_cast<size_t>(1)) == c, "InsertChild places C at index 1");
    TEST_ASSERT(parent->GetChildCount() == 3, "InsertChild increases child count");

    parent->RemoveChild(a);
    TEST_ASSERT(parent->GetChildCount() == 2, "RemoveChild by pointer removes A");
    TEST_ASSERT(a->GetParent() == nullptr, "Removed child A has no parent");
    TEST_ASSERT(parent->GetChild("A") == nullptr, "A no longer found by name");

    parent->RemoveChild("B");
    TEST_ASSERT(parent->GetChildCount() == 1, "RemoveChild by name removes B");
    TEST_ASSERT(parent->GetChild("C") == c, "C still present after removals");

    return true;
}

bool TestChildOrdering() {
    TEST_SECTION("Child Ordering Tests");

    std::shared_ptr<Node> parent = MakeNode("Parent");
    std::shared_ptr<Node> a = MakeNode("A");
    std::shared_ptr<Node> b = MakeNode("B");
    std::shared_ptr<Node> c = MakeNode("C");
    parent->AddChild(a);
    parent->AddChild(b);
    parent->AddChild(c);

    TEST_ASSERT(parent->GetChildIndex(b.get()) == 1, "GetChildIndex returns 1 for B");
    TEST_ASSERT(parent->GetChildIndex(a.get()) == 0, "GetChildIndex returns 0 for A");
    TEST_ASSERT(b->GetIndexInParent() == 1, "GetIndexInParent returns 1 for B");

    parent->MoveChild(c, 0);
    TEST_ASSERT(parent->GetChild(static_cast<size_t>(0)) == c, "MoveChild moves C to front");
    TEST_ASSERT(parent->GetChild(static_cast<size_t>(2)) == b, "B shifts to the back after MoveChild");

    a->SetSiblingIndex(0);
    TEST_ASSERT(a->GetIndexInParent() == 0, "SetSiblingIndex moves A to front");

    std::shared_ptr<Node> orphan = MakeNode("Orphan");
    TEST_ASSERT(parent->GetChildIndex(orphan.get()) == -1, "GetChildIndex returns -1 for non-child");
    TEST_ASSERT(orphan->GetIndexInParent() == -1, "GetIndexInParent returns -1 with no parent");

    return true;
}

bool TestTreeStructureVersion() {
    TEST_SECTION("Tree Structure Version Tests");

    std::shared_ptr<Node> parent = MakeNode("Parent");
    std::shared_ptr<Node> a = MakeNode("A");
    std::shared_ptr<Node> b = MakeNode("B");
    parent->AddChild(a);
    parent->AddChild(b);

    uint64_t before = Node::GetTreeStructureVersion();
    parent->MoveChild(b, 0);
    TEST_ASSERT(Node::GetTreeStructureVersion() == before,
        "Pure sibling reorder does not bump the tree structure version");

    std::shared_ptr<Node> c = MakeNode("C");
    parent->AddChild(c);
    TEST_ASSERT(Node::GetTreeStructureVersion() > before,
        "Adding a child bumps the tree structure version");

    return true;
}

bool TestReparent() {
    TEST_SECTION("Reparent Tests");

    std::shared_ptr<Node> root = MakeNode("Root");
    std::shared_ptr<Node> p1 = MakeNode("P1");
    std::shared_ptr<Node> p2 = MakeNode("P2");
    std::shared_ptr<Node> child = MakeNode("Child");
    root->AddChild(p1);
    root->AddChild(p2);
    p1->AddChild(child);

    TEST_ASSERT(child->GetParent() == p1.get(), "Child starts under P1");

    child->ReparentTo(p2.get());
    TEST_ASSERT(child->GetParent() == p2.get(), "Child reparented to P2");
    TEST_ASSERT(p1->GetChildCount() == 0, "P1 no longer owns the child");
    TEST_ASSERT(p2->GetChild("Child") == child, "P2 now owns the child");

    return true;
}

bool TestFindAndPath() {
    TEST_SECTION("Find / Path Tests");

    std::shared_ptr<Node> root = MakeNode("Root");
    std::shared_ptr<Node> mid = MakeNode("Mid");
    std::shared_ptr<Node> leaf = MakeNode("Leaf");
    root->AddChild(mid);
    mid->AddChild(leaf);

    TEST_ASSERT(root->FindNode("Mid") == mid, "FindNode resolves a direct child");
    TEST_ASSERT(root->FindNode("Mid/Leaf") == leaf, "FindNode resolves a nested path");
    TEST_ASSERT(root->FindNode("Mid/Missing") == nullptr, "FindNode returns null for a bad path");

    TEST_ASSERT(root->GetPath() == "/Root", "Root path is /Root");
    TEST_ASSERT(leaf->GetPath() == "/Root/Mid/Leaf", "Leaf path reflects full ancestry");

    return true;
}

bool TestUniqueNames() {
    TEST_SECTION("Unique Name Tests");

    std::shared_ptr<Node> root = MakeNode("Root");
    std::shared_ptr<Node> branch = MakeNode("Branch");
    std::shared_ptr<Node> health = MakeNode("Health");
    root->AddChild(branch);
    branch->AddChild(health);

    TEST_ASSERT(root->ResolveUniqueName("Health") == nullptr,
        "Unmarked node is not resolvable by unique name");

    health->SetUniqueNameInOwner(true);
    TEST_ASSERT(health->IsUniqueNameInOwner(), "Unique-name flag is set");
    TEST_ASSERT(root->ResolveUniqueName("Health") == health.get(),
        "Marked node resolves from the scope root");
    TEST_ASSERT(branch->ResolveUniqueName("Health") == health.get(),
        "Unique name resolves regardless of which node in scope queries");
    TEST_ASSERT(root->ResolveUniqueName("Branch") == nullptr,
        "Unmarked sibling still does not resolve");

    return true;
}

bool TestGroups() {
    TEST_SECTION("Group Tests");

    std::shared_ptr<Node> node = MakeNode("Enemy");
    TEST_ASSERT(!node->IsInGroup("enemies"), "Node starts in no groups");

    node->AddToGroup("enemies");
    node->AddToGroup("damageable");
    TEST_ASSERT(node->IsInGroup("enemies"), "Node is in 'enemies' after AddToGroup");
    TEST_ASSERT(node->IsInGroup("damageable"), "Node is in 'damageable' after AddToGroup");
    TEST_ASSERT(node->GetGroups().size() == 2, "Node reports two groups");

    node->AddToGroup("enemies");
    TEST_ASSERT(node->GetGroups().size() == 2, "Adding a duplicate group is a no-op");

    node->RemoveFromGroup("enemies");
    TEST_ASSERT(!node->IsInGroup("enemies"), "Node removed from 'enemies'");
    TEST_ASSERT(node->IsInGroup("damageable"), "Other group membership is unaffected");

    return true;
}

bool TestComponents() {
    TEST_SECTION("Component Tests");

    std::shared_ptr<Node> node = MakeNode("Owner");
    std::shared_ptr<MarkerComponent> marker = std::make_shared<MarkerComponent>();
    marker->tag = 7;
    node->AddComponent(marker);

    TEST_ASSERT(node->GetComponents().size() == 1, "Node has one component after AddComponent");
    std::shared_ptr<MarkerComponent> fetched = node->GetComponent<MarkerComponent>();
    TEST_ASSERT(fetched == marker, "GetComponent<T> returns the attached component");
    TEST_ASSERT(fetched->tag == 7, "Fetched component retains its state");
    TEST_ASSERT(node->GetComponent<OtherComponent>() == nullptr,
        "GetComponent<T> returns null for an absent type");
    TEST_ASSERT(node->GetComponent("MarkerComponent") == marker,
        "GetComponent by type name returns the component");

    node->RemoveComponent(marker);
    TEST_ASSERT(node->GetComponents().empty(), "RemoveComponent detaches the component");
    TEST_ASSERT(node->GetComponent<MarkerComponent>() == nullptr,
        "GetComponent<T> is null after removal");

    return true;
}

bool TestActiveVisibleHierarchy() {
    TEST_SECTION("Active / Visible Hierarchy Tests");

    std::shared_ptr<Node> parent = MakeNode("Parent");
    std::shared_ptr<Node> child = MakeNode("Child");
    parent->AddChild(child);

    TEST_ASSERT(child->IsActiveInHierarchy(), "Child is active in hierarchy by default");
    parent->SetActive(false);
    TEST_ASSERT(!child->IsActiveInHierarchy(),
        "Child is inactive in hierarchy when an ancestor is inactive");
    TEST_ASSERT(child->IsActive(), "Child's own active flag is unchanged");

    parent->SetActive(true);
    parent->SetVisible(false);
    TEST_ASSERT(!child->IsVisibleInHierarchy(),
        "Child is hidden in hierarchy when an ancestor is hidden");
    TEST_ASSERT(child->IsVisible(), "Child's own visible flag is unchanged");

    return true;
}

bool TestNode2DTransform() {
    TEST_SECTION("Node2D Transform Tests");

    std::shared_ptr<Node2D> parent = std::make_shared<Node2D>("Parent2D");
    std::shared_ptr<Node2D> child = std::make_shared<Node2D>("Child2D");
    parent->AddChild(child);

    parent->SetPosition(math::Vec2(10.0f, 20.0f));
    parent->SetRotation(0.0f);
    parent->SetScale(math::Vec2(1.0f, 1.0f));
    child->SetPosition(math::Vec2(5.0f, 5.0f));

    TEST_ASSERT(math::Equals(child->GetPosition().x, 5.0f), "Child local X is set");

    math::Vec2 global = child->GetGlobalPosition();
    TEST_ASSERT(math::Equals(global.x, 15.0f), "Child global X = parent + local");
    TEST_ASSERT(math::Equals(global.y, 25.0f), "Child global Y = parent + local");

    math::Vec2 rootGlobal = parent->GetGlobalPosition();
    TEST_ASSERT(math::Equals(rootGlobal.x, 10.0f) && math::Equals(rootGlobal.y, 20.0f),
        "Parentless node global equals its local position");

    return true;
}

} // namespace

void RunNodeTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "NODE SYSTEM TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Node System");
    bool allPassed = true;

    allPassed &= TestConstruction();
    allPassed &= TestHierarchy();
    allPassed &= TestChildOrdering();
    allPassed &= TestTreeStructureVersion();
    allPassed &= TestReparent();
    allPassed &= TestFindAndPath();
    allPassed &= TestUniqueNames();
    allPassed &= TestGroups();
    allPassed &= TestComponents();
    allPassed &= TestActiveVisibleHierarchy();
    allPassed &= TestNode2DTransform();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL NODE SYSTEM TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
