// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES FROM THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "network/SpanningTree.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

using namespace network;

namespace
{
    /**
     * @brief Creates a JSON representation of a tree node with the given ID, master ID, and optional children.
     * @param id The ID of the tree node.
     * @param masterId The master ID of the tree node.
     * @param children The optional array of child nodes.
     * @return A JSON value representing the tree node.
     */
    json::value treeNode(uint32_t id, uint32_t masterId, const json::array& children = json::array())
    {
        json::object node = json::object();
        node["id"].set<uint32_t>(id);
        node["masterId"].set<uint32_t>(masterId);
        node["identity"].set<std::string>("test-peer-" + std::to_string(id));
        node["children"].set<json::array>(children);
        return json::value(node);
    }
}

/**
 * @brief Fixture for creating and managing a SpanningTree root node.
 */
class TreeFixture {
public:
    /**
     * @brief Initializes a new instance of the TreeFixture class with the specified root ID.
     * @param rootId The ID of the root node of the spanning tree.
     */
    explicit TreeFixture(uint32_t rootId) :
        root(new SpanningTree(rootId, rootId, nullptr))
    {
        root->identity("test-root");
    }

    /**
     * @brief Finalizes an instance of the TreeFixture class.
     */
    ~TreeFixture()
    {
        if (root != nullptr && SpanningTree::findByPeerID(root->id()) == root)
            SpanningTree::erasePeer(root->id());
    }

    SpanningTree* root;
};

TEST_CASE("SpanningTree recursively removes a multi-level subtree", "[fne][stp]")
{
    TreeFixture fixture(0xF1000001U);
    SpanningTree* branch = new SpanningTree(0xF1000002U, 0xF1100002U, fixture.root);
    SpanningTree* childA = new SpanningTree(0xF1000003U, 0xF1100003U, branch);
    SpanningTree* childB = new SpanningTree(0xF1000004U, 0xF1100004U, branch);
    new SpanningTree(0xF1000005U, 0xF1100005U, childA);
    new SpanningTree(0xF1000006U, 0xF1100006U, childB);

    REQUIRE(SpanningTree::countChildren(fixture.root) == 5U);

    SpanningTree::erasePeer(branch->id());

    CHECK_FALSE(fixture.root->hasChildren());
    CHECK(SpanningTree::countChildren(fixture.root) == 0U);
    for (uint32_t id = 0xF1000002U; id <= 0xF1000006U; id++)
        CHECK(SpanningTree::findByPeerID(id) == nullptr);
}

TEST_CASE("SpanningTree update safely prunes a removed descendant branch", "[fne][stp]")
{
    TreeFixture fixture(0xF2000001U);
    SpanningTree* upstream = new SpanningTree(0xF2000002U, 0xF2100002U, fixture.root);
    SpanningTree* removedBranch = new SpanningTree(0xF2000003U, 0xF2100003U, upstream);
    new SpanningTree(0xF2000004U, 0xF2100004U, removedBranch);
    SpanningTree* retainedBranch = new SpanningTree(0xF2000005U, 0xF2100005U, upstream);
    SpanningTree* unaffected = new SpanningTree(0xF2000006U, 0xF2100006U, fixture.root);

    json::array retainedChildren = json::array();
    retainedChildren.push_back(treeNode(retainedBranch->id(), retainedBranch->masterId()));

    json::array update = json::array();
    update.push_back(treeNode(upstream->id(), upstream->masterId(), retainedChildren));

    std::vector<uint32_t> duplicatePeers;
    SpanningTree::deserializeTree(update, fixture.root, &duplicatePeers);

    CHECK(duplicatePeers.empty());
    CHECK(SpanningTree::findByPeerID(0xF2000003U) == nullptr);
    CHECK(SpanningTree::findByPeerID(0xF2000004U) == nullptr);
    CHECK(SpanningTree::findByPeerID(retainedBranch->id()) == retainedBranch);
    CHECK(retainedBranch->m_parent == upstream);
    CHECK(SpanningTree::findByPeerID(unaffected->id()) == unaffected);
    CHECK(unaffected->m_parent == fixture.root);
    CHECK(SpanningTree::countChildren(fixture.root) == 3U);
}

TEST_CASE("SpanningTree update removes all descendants from an announced leaf", "[fne][stp]")
{
    TreeFixture fixture(0xF3000001U);
    SpanningTree* upstream = new SpanningTree(0xF3000002U, 0xF3100002U, fixture.root);
    SpanningTree* child = new SpanningTree(0xF3000003U, 0xF3100003U, upstream);
    new SpanningTree(0xF3000004U, 0xF3100004U, child);

    json::array update = json::array();
    update.push_back(treeNode(upstream->id(), upstream->masterId()));

    std::vector<uint32_t> duplicatePeers;
    SpanningTree::deserializeTree(update, fixture.root, &duplicatePeers);

    CHECK(duplicatePeers.empty());
    CHECK_FALSE(upstream->hasChildren());
    CHECK(SpanningTree::findByPeerID(0xF3000003U) == nullptr);
    CHECK(SpanningTree::findByPeerID(0xF3000004U) == nullptr);
    CHECK(SpanningTree::countChildren(fixture.root) == 1U);
}

TEST_CASE("SpanningTree deserializes and serializes a complete hierarchy", "[fne][stp]")
{
    TreeFixture fixture(0xF4000001U);

    json::array grandchildren = json::array();
    grandchildren.push_back(treeNode(0xF4000004U, 0xF4100004U));

    json::array children = json::array();
    children.push_back(treeNode(0xF4000003U, 0xF4100003U, grandchildren));

    json::array update = json::array();
    update.push_back(treeNode(0xF4000002U, 0xF4100002U, children));

    std::vector<uint32_t> duplicatePeers;
    SpanningTree::deserializeTree(update, fixture.root, &duplicatePeers);

    SpanningTree* upstream = SpanningTree::findByPeerID(0xF4000002U);
    SpanningTree* child = SpanningTree::findByPeerID(0xF4000003U);
    SpanningTree* grandchild = SpanningTree::findByPeerID(0xF4000004U);
    REQUIRE(upstream != nullptr);
    REQUIRE(child != nullptr);
    REQUIRE(grandchild != nullptr);
    CHECK(duplicatePeers.empty());
    CHECK(upstream->m_parent == fixture.root);
    CHECK(child->m_parent == upstream);
    CHECK(grandchild->m_parent == child);
    CHECK(grandchild->identity() == "test-peer-4093640708");
    CHECK(SpanningTree::countChildren(fixture.root) == 3U);

    json::array serialized = json::array();
    SpanningTree::serializeTree(fixture.root, serialized);
    REQUIRE(serialized.size() == 1U);

    json::object root = serialized[0U].get<json::object>();
    CHECK(root["id"].get<uint32_t>() == fixture.root->id());
    CHECK(root["masterId"].get<uint32_t>() == fixture.root->masterId());
    CHECK(root["identity"].get<std::string>() == "test-root");

    json::array serializedChildren = root["children"].get<json::array>();
    REQUIRE(serializedChildren.size() == 1U);
    json::object serializedUpstream = serializedChildren[0U].get<json::object>();
    CHECK(serializedUpstream["id"].get<uint32_t>() == upstream->id());
    REQUIRE(serializedUpstream["children"].get<json::array>().size() == 1U);
}

TEST_CASE("SpanningTree ignores malformed announcements and continues processing", "[fne][stp]")
{
    TreeFixture fixture(0xF5000001U);

    uint32_t missingMasterId = 0xF5000002U;
    json::object missingMaster = json::object();
    missingMaster["id"].set<uint32_t>(missingMasterId);
    missingMaster["children"].set<json::array>(json::array());

    uint32_t invalidChildrenId = 0xF5000003U;
    uint32_t invalidChildrenMasterId = 0xF5100003U;
    json::object invalidChildren = json::object();
    invalidChildren["id"].set<uint32_t>(invalidChildrenId);
    invalidChildren["masterId"].set<uint32_t>(invalidChildrenMasterId);
    invalidChildren["children"].set<std::string>("not-an-array");

    json::array update = json::array();
    update.push_back(json::value(missingMaster));
    update.push_back(json::value(invalidChildren));
    update.push_back(treeNode(0xF5000004U, 0xF5100004U));

    std::vector<uint32_t> duplicatePeers;
    SpanningTree::deserializeTree(update, fixture.root, &duplicatePeers);

    CHECK(duplicatePeers.empty());
    CHECK(SpanningTree::findByPeerID(0xF5000002U) == nullptr);
    CHECK(SpanningTree::findByPeerID(0xF5000003U) == nullptr);
    CHECK(SpanningTree::findByPeerID(0xF5000004U) != nullptr);
    CHECK(SpanningTree::countChildren(fixture.root) == 1U);
}

TEST_CASE("SpanningTree reports a duplicate master without adding the peer", "[fne][stp]")
{
    TreeFixture fixture(0xF6000001U);
    SpanningTree* existing = new SpanningTree(0xF6000002U, 0xF6100002U, fixture.root);

    json::array update = json::array();
    update.push_back(treeNode(0xF6000003U, existing->masterId()));

    std::vector<uint32_t> duplicatePeers;
    SpanningTree::deserializeTree(update, fixture.root, &duplicatePeers);

    REQUIRE(duplicatePeers.size() == 1U);
    CHECK(duplicatePeers[0U] == 0xF6000003U);
    CHECK(SpanningTree::findByPeerID(0xF6000003U) == nullptr);
    CHECK(SpanningTree::findByPeerID(existing->id()) == existing);
    CHECK(SpanningTree::countChildren(fixture.root) == 1U);
}

TEST_CASE("SpanningTree requires repeated updates before reparenting", "[fne][stp]")
{
    TreeFixture fixture(0xF7000001U);
    SpanningTree* oldParent = new SpanningTree(0xF7000002U, 0xF7100002U, fixture.root);
    SpanningTree* newParent = new SpanningTree(0xF7000003U, 0xF7100003U, fixture.root);
    SpanningTree* moving = new SpanningTree(0xF7000004U, 0xF7100004U, oldParent);

    json::array update = json::array();
    update.push_back(treeNode(moving->id(), moving->masterId()));
    std::vector<uint32_t> duplicatePeers;

    for (uint32_t i = 0U; i < SpanningTree::s_maxUpdatesBeforeReparent; i++) {
        SpanningTree::deserializeTree(update, newParent, &duplicatePeers);
        CHECK(moving->m_parent == oldParent);
    }

    SpanningTree::deserializeTree(update, newParent, &duplicatePeers);

    CHECK(duplicatePeers.empty());
    CHECK(moving->m_parent == newParent);
    CHECK_FALSE(oldParent->hasChildren());
    REQUIRE(newParent->m_children.size() == 1U);
    CHECK(newParent->m_children[0U] == moving);
}

TEST_CASE("SpanningTree explicit reparenting preserves the moved subtree", "[fne][stp]")
{
    TreeFixture fixture(0xF8000001U);
    SpanningTree* oldParent = new SpanningTree(0xF8000002U, 0xF8100002U, fixture.root);
    SpanningTree* newParent = new SpanningTree(0xF8000003U, 0xF8100003U, fixture.root);
    SpanningTree* moving = new SpanningTree(0xF8000004U, 0xF8100004U, oldParent);
    SpanningTree* descendant = new SpanningTree(0xF8000005U, 0xF8100005U, moving);

    SpanningTree::moveParent(moving, newParent);

    CHECK(moving->m_parent == newParent);
    CHECK(descendant->m_parent == moving);
    CHECK_FALSE(oldParent->hasChildren());
    REQUIRE(newParent->m_children.size() == 1U);
    CHECK(newParent->m_children[0U] == moving);
    CHECK(SpanningTree::countChildren(fixture.root) == 4U);

    // Root nodes are never eligible for reparenting.
    SpanningTree::moveParent(fixture.root, newParent);
    CHECK(fixture.root->isRoot());
}
