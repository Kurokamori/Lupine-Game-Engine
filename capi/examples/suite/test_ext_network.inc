/* Networking / multiplayer C API coverage (lc_network / lc_rpc / lc_replication).
 *
 * Runs headless over the in-process loopback transport. Validates session
 * lifecycle, peer/authority queries, RPC + spawn entry points, and - crucially -
 * that everything degrades cleanly when no session is active. */

static int g_extNetEventHits = 0;

static void ext_net_event_cb(const char* args_json, void* user_data) {
    (void)args_json;
    (void)user_data;
    g_extNetEventHits++;
}

static void test_ext_network(void) {
    SECTION_START("Ext: Networking session");

    LCResult result = lc_net_initialize();
    TEST_RESULT(result, "lc_net_initialize()");

    /* Offline baseline: nothing is active and queries are safe. */
    TEST_ASSERT(lc_net_is_active() == false, "offline: not active");
    TEST_ASSERT(lc_net_is_server() == false, "offline: not server");
    TEST_ASSERT(lc_net_get_mode() == LC_NET_MODE_OFFLINE, "offline: mode is OFFLINE");

    /* Start a loopback host. */
    LCNetConfig config;
    result = lc_net_config_default(&config);
    TEST_RESULT(result, "lc_net_config_default()");
    config.transport = LC_NET_TRANSPORT_LOOPBACK;
    config.port = 41234;

    bool ok = false;
    result = lc_net_start_host(&config, &ok);
    TEST_RESULT(result, "lc_net_start_host(loopback)");
    TEST_ASSERT(ok == true, "host started");
    TEST_ASSERT(lc_net_is_active() == true, "host: active");
    TEST_ASSERT(lc_net_is_server() == true, "host: is server");
    TEST_ASSERT(lc_net_get_mode() == LC_NET_MODE_HOST, "host: mode is HOST");
    TEST_ASSERT(lc_net_local_peer_id() == 1, "host: local peer id is 1");
    TEST_ASSERT(lc_net_peer_count() == 0, "host: no remote peers yet");

    char* peersJson = NULL;
    result = lc_net_get_peers(&peersJson);
    TEST_RESULT(result, "lc_net_get_peers()");
    if (peersJson != NULL) {
        TEST_ASSERT(peersJson[0] == '[', "peers serialized as JSON array");
        lc_free(peersJson);
    }

    /* Session-signal subscription (no peer to fire it here, just the API path). */
    uint64_t subId = 0;
    result = lc_net_on_event("peer_connected", ext_net_event_cb, NULL, &subId);
    TEST_RESULT(result, "lc_net_on_event(peer_connected)");
    result = lc_net_off_event("peer_connected", subId);
    TEST_RESULT(result, "lc_net_off_event()");

    /* Networked node + NetworkObject. */
    LCNodeHandle node = NULL;
    result = lc_type_create_node("Node2D", &node);
    TEST_RESULT(result, "create Node2D");

    if (node != NULL) {
        LCComponentHandle netObj = NULL;
        result = lc_node_add_component_by_type(node, "NetworkObject", &netObj);
        TEST_RESULT(result, "add NetworkObject component");

        /* Authority: the host is authority for server-owned objects. */
        TEST_ASSERT(lc_net_is_authority(node) == true, "host is authority for the node");

        result = lc_net_set_authority(node, 2);
        TEST_RESULT(result, "lc_net_set_authority(peer 2)");
        TEST_ASSERT(lc_net_get_authority(node) == 2, "authority round-trips to peer 2");
        /* The host is still authority over everything regardless of owner. */
        TEST_ASSERT(lc_net_is_authority(node) == true, "host stays authority over all objects");

        uint32_t objId = 123;
        result = lc_net_object_id(node, &objId);
        TEST_RESULT(result, "lc_net_object_id()");

        /* RPC entry points (no peers => no delivery, must not error). */
        result = lc_net_register_component_rpc("NetworkObject", "ping",
                                               LC_RPC_MODE_ANY_PEER, LC_RPC_TRANSFER_RELIABLE, true);
        TEST_RESULT(result, "lc_net_register_component_rpc()");
        result = lc_net_rpc(node, "on_ping", "[1, 2, 3]");
        TEST_RESULT(result, "lc_net_rpc()");
        result = lc_net_rpc_id(node, 1, "on_ping", "[]");
        TEST_RESULT(result, "lc_net_rpc_id()");
        result = lc_net_rpc_unreliable(node, "on_move", "[1.5, 2.5]");
        TEST_RESULT(result, "lc_net_rpc_unreliable()");

        lc_node_destroy(node);
    }

    /* Pump once so any queued work drains without a SceneManager present. */
    result = lc_net_poll(0.016f);
    TEST_RESULT(result, "lc_net_poll()");
    result = lc_net_flush();
    TEST_RESULT(result, "lc_net_flush()");

    /* Tear the session down and confirm we are inert again. */
    result = lc_net_disconnect();
    TEST_RESULT(result, "lc_net_disconnect()");
    TEST_ASSERT(lc_net_is_active() == false, "after disconnect: not active");

    /* Offline RPC must still be a safe no-op / local call. */
    LCNodeHandle offlineNode = NULL;
    if (lc_type_create_node("Node2D", &offlineNode) == LC_SUCCESS && offlineNode != NULL) {
        LCComponentHandle c = NULL;
        lc_node_add_component_by_type(offlineNode, "NetworkObject", &c);
        TEST_ASSERT(lc_net_is_authority(offlineNode) == true, "offline: node is locally authoritative");
        result = lc_net_rpc(offlineNode, "on_ping", "[]");
        TEST_RESULT(result, "offline lc_net_rpc() degrades safely");
        lc_node_destroy(offlineNode);
    }

    /* New config fields carry sane defaults. */
    LCNetConfig cfg2;
    result = lc_net_config_default(&cfg2);
    TEST_RESULT(result, "lc_net_config_default() (prediction/reconnect fields)");
    TEST_ASSERT(cfg2.enable_prediction == true, "default: prediction enabled");
    TEST_ASSERT(cfg2.input_redundancy == 3, "default: input redundancy 3");
    TEST_ASSERT(cfg2.auto_reconnect == false, "default: auto-reconnect off");
    TEST_ASSERT(cfg2.resume_timeout_seconds == 0.0f, "default: resume off");

    /* Diagnostics queries are safe offline. */
    LCNetStats stats;
    result = lc_net_get_stats(&stats);
    TEST_RESULT(result, "lc_net_get_stats()");
    TEST_ASSERT(stats.peer_count == 0, "offline: zero peers in stats");
    TEST_ASSERT(lc_net_peer_loss(2) == 0.0f, "offline: peer loss is 0");
    TEST_ASSERT(lc_net_peer_jitter(2) == 0.0f, "offline: peer jitter is 0");

    /* The four new networked components are creatable via the registry. */
    LCNodeHandle compNode = NULL;
    if (lc_type_create_node("Node2D", &compNode) == LC_SUCCESS && compNode != NULL) {
        LCComponentHandle nc = NULL;
        TEST_ASSERT(lc_node_add_component_by_type(compNode, "NetworkController", &nc) == LC_SUCCESS,
                    "create NetworkController");
        LCComponentHandle na = NULL;
        TEST_ASSERT(lc_node_add_component_by_type(compNode, "NetworkAnimator", &na) == LC_SUCCESS,
                    "create NetworkAnimator");
        LCComponentHandle nr = NULL;
        TEST_ASSERT(lc_node_add_component_by_type(compNode, "NetworkRigidBody2D", &nr) == LC_SUCCESS,
                    "create NetworkRigidBody2D");
        lc_node_destroy(compNode);
    }

    SECTION_END("Ext: Networking session");
}
