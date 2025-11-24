/**
 * @file node_example.c
 * @brief Example demonstrating the Node system in Lupine Engine C API
 */

#include <lupine_c.h>
#include <stdio.h>

int main(void) {
    printf("==============================================\n");
    printf("  Lupine Engine C API - Node System Example  \n");
    printf("==============================================\n\n");

    // Initialize engine
    printf("Initializing engine...\n");
    if (lc_init() != LC_SUCCESS) {
        printf("ERROR: Failed to initialize: %s\n", lc_get_last_error());
        return 1;
    }
    printf("✓ Engine initialized\n\n");

    // Create a 3D node hierarchy
    printf("=== Testing Node3D Hierarchy ===\n");

    // Create root node
    LCNodeHandle root;
    if (lc_node_create(LC_NODE_3D, "Root", &root) != LC_SUCCESS) {
        printf("ERROR: Failed to create root node\n");
        return 1;
    }
    printf("✓ Created root node: %s\n", lc_node_get_name(root));
    printf("  UUID: %s\n", lc_node_get_uuid(root));

    // Set root transform
    lc_node3d_set_position(root, lc_vec3(0.0f, 0.0f, 0.0f));
    lc_node3d_set_rotation_euler(root, lc_vec3(0.0f, lc_deg_to_rad(45.0f), 0.0f));
    lc_node3d_set_scale(root, lc_vec3_one());

    LCVec3 root_pos;
    lc_node3d_get_position(root, &root_pos);
    printf("  Position: (%.2f, %.2f, %.2f)\n", root_pos.x, root_pos.y, root_pos.z);

    // Create child node
    LCNodeHandle child;
    if (lc_node_create(LC_NODE_3D, "Child", &child) != LC_SUCCESS) {
        printf("ERROR: Failed to create child node\n");
        return 1;
    }
    printf("✓ Created child node: %s\n", lc_node_get_name(child));

    // Set child transform
    lc_node3d_set_position(child, lc_vec3(5.0f, 0.0f, 0.0f));
    lc_node3d_set_scale(child, lc_vec3(2.0f, 2.0f, 2.0f));

    // Add child to root
    if (lc_node_add_child(root, child) != LC_SUCCESS) {
        printf("ERROR: Failed to add child to root\n");
        return 1;
    }
    printf("✓ Added child to root\n");

    // Check hierarchy
    size_t child_count;
    lc_node_get_child_count(root, &child_count);
    printf("  Root has %zu children\n", child_count);

    // Get child's local and global transforms
    LCVec3 local_pos, global_pos;
    lc_node3d_get_position(child, &local_pos);
    lc_node3d_get_global_position(child, &global_pos);

    printf("  Child local position: (%.2f, %.2f, %.2f)\n", local_pos.x, local_pos.y, local_pos.z);
    printf("  Child global position: (%.2f, %.2f, %.2f)\n", global_pos.x, global_pos.y, global_pos.z);

    // Create grandchild
    LCNodeHandle grandchild;
    lc_node_create(LC_NODE_3D, "GrandChild", &grandchild);
    lc_node3d_set_position(grandchild, lc_vec3(0.0f, 3.0f, 0.0f));
    lc_node_add_child(child, grandchild);
    printf("✓ Created grandchild and added to child\n");

    // Get grandchild's global position
    LCVec3 grandchild_global;
    lc_node3d_get_global_position(grandchild, &grandchild_global);
    printf("  GrandChild global position: (%.2f, %.2f, %.2f)\n",
           grandchild_global.x, grandchild_global.y, grandchild_global.z);

    // Test node search
    printf("\n=== Testing Node Search ===\n");
    LCNodeHandle found;
    if (lc_node_find(root, "Child/GrandChild", &found) == LC_SUCCESS) {
        printf("✓ Found node by path: %s\n", lc_node_get_name(found));
        printf("  Path: %s\n", lc_node_get_path(found));
    } else {
        printf("✗ Failed to find node by path\n");
    }

    // Test getting child by name
    LCNodeHandle child_by_name;
    if (lc_node_get_child_by_name(root, "Child", &child_by_name) == LC_SUCCESS) {
        printf("✓ Found child by name: %s\n", lc_node_get_name(child_by_name));
    }

    // Test getting all children
    printf("\n=== Testing Get All Children ===\n");
    size_t total_children;
    lc_node_get_children(child, NULL, 0, &total_children);
    printf("  Child has %zu children\n", total_children);

    if (total_children > 0) {
        LCNodeHandle children[10];
        size_t actual_count;
        lc_node_get_children(child, children, 10, &actual_count);
        printf("  Children:\n");
        for (size_t i = 0; i < actual_count; ++i) {
            printf("    %zu: %s\n", i, lc_node_get_name(children[i]));
        }
    }

    // Test Node2D
    printf("\n=== Testing Node2D ===\n");
    LCNodeHandle node2d_root;
    lc_node_create(LC_NODE_2D, "2D Root", &node2d_root);
    printf("✓ Created Node2D: %s\n", lc_node_get_name(node2d_root));

    // Set 2D transform
    lc_node2d_set_position(node2d_root, lc_vec2(100.0f, 200.0f));
    lc_node2d_set_rotation(node2d_root, lc_deg_to_rad(90.0f));
    lc_node2d_set_scale(node2d_root, lc_vec2(1.5f, 1.5f));
    lc_node2d_set_z_index(node2d_root, 10);

    LCVec2 pos2d;
    float rot2d;
    LCVec2 scale2d;
    int z_index;

    lc_node2d_get_position(node2d_root, &pos2d);
    lc_node2d_get_rotation(node2d_root, &rot2d);
    lc_node2d_get_scale(node2d_root, &scale2d);
    lc_node2d_get_z_index(node2d_root, &z_index);

    printf("  Position: (%.2f, %.2f)\n", pos2d.x, pos2d.y);
    printf("  Rotation: %.2f degrees\n", lc_rad_to_deg(rot2d));
    printf("  Scale: (%.2f, %.2f)\n", scale2d.x, scale2d.y);
    printf("  Z-Index: %d\n", z_index);

    // Create 2D child
    LCNodeHandle node2d_child;
    lc_node_create(LC_NODE_2D, "2D Child", &node2d_child);
    lc_node2d_set_position(node2d_child, lc_vec2(50.0f, 0.0f));
    lc_node_add_child(node2d_root, node2d_child);

    LCVec2 child_local, child_global;
    lc_node2d_get_position(node2d_child, &child_local);
    lc_node2d_get_global_position(node2d_child, &child_global);

    printf("✓ Created 2D child\n");
    printf("  Local position: (%.2f, %.2f)\n", child_local.x, child_local.y);
    printf("  Global position: (%.2f, %.2f)\n", child_global.x, child_global.y);

    // Test look_at for Node3D
    printf("\n=== Testing Look At ===\n");
    LCNodeHandle camera;
    lc_node_create(LC_NODE_3D, "Camera", &camera);
    lc_node3d_set_position(camera, lc_vec3(0.0f, 5.0f, 10.0f));

    LCVec3 target = lc_vec3(0.0f, 0.0f, 0.0f);
    lc_node3d_look_at(camera, target, lc_vec3_up());

    LCVec3 camera_euler;
    lc_node3d_get_rotation_euler(camera, &camera_euler);
    printf("✓ Camera looking at origin\n");
    printf("  Camera rotation (euler): (%.2f, %.2f, %.2f) degrees\n",
           lc_rad_to_deg(camera_euler.x),
           lc_rad_to_deg(camera_euler.y),
           lc_rad_to_deg(camera_euler.z));

    // Test active/visible properties
    printf("\n=== Testing Active/Visible Properties ===\n");
    printf("  Root is active: %s\n", lc_node_is_active(root) ? "yes" : "no");
    printf("  Root is visible: %s\n", lc_node_is_visible(root) ? "yes" : "no");

    lc_node_set_active(root, false);
    printf("  Set root to inactive\n");
    printf("  Root is active: %s\n", lc_node_is_active(root) ? "yes" : "no");
    printf("  Child is active: %s\n", lc_node_is_active(child) ? "yes" : "no");
    printf("  Child is active in hierarchy: %s\n",
           lc_node_is_active_in_hierarchy(child) ? "yes" : "no");

    lc_node_set_active(root, true);
    printf("  Set root back to active\n");
    printf("  Child is active in hierarchy: %s\n",
           lc_node_is_active_in_hierarchy(child) ? "yes" : "no");

    // Test transform matrices
    printf("\n=== Testing Transform Matrices ===\n");
    LCMat4 local_matrix, global_matrix;
    lc_node3d_get_transform_matrix(child, &local_matrix);
    lc_node3d_get_global_transform_matrix(child, &global_matrix);
    printf("✓ Retrieved local and global transform matrices\n");
    printf("  Local matrix [0][0]: %.2f\n", local_matrix.m[0]);
    printf("  Global matrix [0][0]: %.2f\n", global_matrix.m[0]);

    // Cleanup
    printf("\n=== Cleanup ===\n");
    printf("Destroying nodes...\n");
    lc_node_destroy(root);        // Destroys entire 3D hierarchy
    lc_node_destroy(node2d_root); // Destroys entire 2D hierarchy
    lc_node_destroy(camera);
    printf("✓ All nodes destroyed\n\n");

    // Shutdown engine
    printf("Shutting down engine...\n");
    lc_shutdown();
    printf("✓ Engine shutdown complete\n\n");

    printf("==============================================\n");
    printf("  Node system tests completed successfully!  \n");
    printf("==============================================\n");

    return 0;
}
