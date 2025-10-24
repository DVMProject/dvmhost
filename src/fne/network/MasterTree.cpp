// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/Log.h"
#include "network/MasterTree.h"

using namespace network;

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::unordered_map<uint32_t, MasterTree*> MasterTree::m_masterTrees = std::unordered_map<uint32_t, MasterTree*>();
uint8_t MasterTree::m_maxUpdatesBeforeReparent = 5U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MasterTree class. */

MasterTree::MasterTree(uint32_t id, uint32_t masterId, MasterTree* parent) :
    m_parent(parent),
    m_children(),
    m_id(id),
    m_masterId(masterId),
    m_updatesBeforeReparent(0U)
{
    m_masterTrees[id] = this;
    if (m_parent != nullptr)
        m_parent->m_children.push_back(this);
}

/* Finalizes a instance of the MasterTree class. */

MasterTree::~MasterTree()
{
    for (auto child : m_children) {
        if (child != nullptr) {
            delete child;
            child = nullptr;
        }
    }
    m_children.clear();
}

/* Find a peer master tree by peer ID. */

MasterTree* MasterTree::findByPeerID(const uint32_t peerId)
{
    auto it = m_masterTrees.find(peerId);
    if (it != m_masterTrees.end()) {
        return it->second;
    }

    return nullptr;
}

/* Find a peer master tree by master peer ID. */

MasterTree* MasterTree::findByMasterID(const uint32_t masterId)
{
    for (auto it : m_masterTrees) {
        MasterTree* tree = it.second;
        if (tree != nullptr) {
            if (tree->masterId() == masterId) {
                return tree;
            }
        }
    }

    return nullptr;
}

/* Count all children of a master tree node. */

uint32_t MasterTree::countChildren(MasterTree* node)
{
    if (node == nullptr)
        return 0U;
    if (node->m_children.size() == 0)
        return 0U;

    uint32_t count = 0U;
    for (auto child : node->m_children) {
        ++count;
        count += countChildren(child);
    }

    return count;
}

/* Erase a peer from the tree. */

void MasterTree::erasePeer(const uint32_t peerId)
{
    auto it = m_masterTrees.find(peerId);
    if (it != m_masterTrees.end()) {
        MasterTree* tree = it->second;
        if (tree != nullptr) {
            if (tree->m_parent != nullptr) {
                auto& siblings = tree->m_parent->m_children;
                siblings.erase(std::remove(siblings.begin(), siblings.end(), tree), siblings.end());
            }

            if (tree->hasChildren()) {
                eraseChildren(tree);
            }

            delete tree;
        }

        m_masterTrees.erase(it);
    }
}

/* Helper to recursively serialize master tree node to JSON array. */

void MasterTree::serializeTree(MasterTree* node, json::array& jsonArray)
{
    if (node == nullptr)
        return;

    json::object obj;
    uint32_t id = node->id();
    obj["id"].set<uint32_t>(id);
    uint32_t masterId = node->masterId();
    obj["masterId"].set<uint32_t>(masterId);

    json::array childArray;
    for (auto child : node->m_children) {
        serializeTree(child, childArray);
    }
    obj["children"].set<json::array>(childArray);

    jsonArray.push_back(json::value(obj));
}

/* Helper to recursively deserialize master tree node from JSON array. */

void MasterTree::deserializeTree(json::array& jsonArray, MasterTree* parent, std::vector<uint32_t>* duplicatePeers)
{
    for (auto& v : jsonArray) {
        if (!v.is<json::object>())
            continue;

        json::object obj = v.get<json::object>();
        if (!obj["id"].is<uint32_t>())
            continue;
        if (!obj["masterId"].is<uint32_t>())
            continue;
        if (!obj["children"].is<json::array>())
            continue;

        uint32_t id = obj["id"].get<uint32_t>();
        uint32_t masterId = obj["masterId"].get<uint32_t>();

        // check if this peer is already connected via another peer
        MasterTree* tree = MasterTree::findByMasterID(masterId);
        if (tree != nullptr) {
            // is this a fast reconnect? (this happens when a connecting peer
            //  uses the same peer ID and master ID already announced in the tree, but
            //  the tree entry wasn't yet erased)
            if (tree->id() != id) {
                if (duplicatePeers != nullptr)
                    duplicatePeers->push_back(id);
                continue;
            }
        }

        MasterTree* existingNode = findByPeerID(id);
        if (existingNode == nullptr)
            existingNode = new MasterTree(id, masterId, parent);
        else {
            if (existingNode->m_updatesBeforeReparent >= m_maxUpdatesBeforeReparent) {
                existingNode->m_updatesBeforeReparent = 0U;

                // reparent the node if necessary
                moveParent(existingNode, parent);
            } else {
                existingNode->m_updatesBeforeReparent++;
            }
        }

        json::array childArray = obj["children"].get<json::array>();
        //LogDebugEx(LOG_STP, "MasterTree::deserializeTree()", "peerId = %u, masterId = %u, childrenCnt = %u", 
        //    existingNode->id(), existingNode->masterId(), childArray.size());
        if (childArray.size() > 0U)
            deserializeTree(childArray, existingNode, duplicatePeers);
    }
}

/* Helper to move the master tree node to a different parent master tree node. */

void MasterTree::moveParent(MasterTree* node, MasterTree* parent)
{
    if (node == nullptr)
        return;
    if (parent == nullptr)
        return;

    // the root node cannot be moved
    if (node->m_parent == nullptr) {
        LogError(LOG_STP, "PEER %u is a root tree node, can't be moved. BUGBUG.", node->id());
        return;
    }

    if (node->m_parent != parent) {
        // find the node in the parent children and remove it
        MasterTree* nodeParent = node->m_parent;
        uint32_t nodeParentId = 0U;
        bool hasReleasedFromParent = false;

        if (nodeParent != nullptr) {
            nodeParentId = nodeParent->id();
            auto it = std::find_if(nodeParent->m_children.begin(), nodeParent->m_children.end(), [&](MasterTree* childNode) {
                if (childNode == node)
                    return true;
                return false;
            });
            if (it != nodeParent->m_children.end()) {
                nodeParent->m_children.erase(it);
                hasReleasedFromParent = true;
            } else {
                LogError(LOG_STP, "PEER %u failed to release ownership from PEER %u, tree is potentially inconsistent",
                    node->id(), nodeParent->id());
            }
        }

        if (hasReleasedFromParent) {
            // reparent existing node
            node->m_parent = parent;
            if (node->m_parent != nullptr)
                node->m_parent->m_children.push_back(node);

            // reset update counter
            if (node->m_updatesBeforeReparent > 0U)
                node->m_updatesBeforeReparent = 0U;

            LogWarning(LOG_STP, "PEER %u ownership has changed from PEER %u to PEER %u; this normally shouldn't happen",
                    node->id(), nodeParentId, parent->id());
        }
    }
}

/* Debug helper to visualize the tree structure in the log. */

void MasterTree::visualizeTreeToLog(MasterTree* node, uint32_t level)
{
    if (node == nullptr)
        return;
    if (node->m_children.size() == 0) {
        return;
    }

    if (level == 0U)
        LogInfoEx(LOG_STP, "Peer ID: %u, Master Peer ID: %u, Children: %u, IsRoot: %u", 
            node->id(), node->masterId(), node->m_children.size(), node->isRoot());

    std::string indent;
    for (uint32_t i = 0U; i < level; i++) {
        indent += "  ";
    }

    for (auto child : node->m_children) {
        LogInfoEx(LOG_STP, "%s- Peer ID: %u, Master Peer ID: %u, Children: %u, IsRoot: %u", 
            indent.c_str(), child->id(), child->masterId(), child->m_children.size(), child->isRoot());
        visualizeTreeToLog(child, level + 1U);
    }
}

// ---------------------------------------------------------------------------
//  Private Static Class Members
// ---------------------------------------------------------------------------

/* Erase all children of a master tree node. */

void MasterTree::eraseChildren(MasterTree* node)
{
    if (node == nullptr)
        return;

    for (auto child : node->m_children) {
        if (child != nullptr) {
            eraseChildren(child);
            delete child;
        }
    }

    node->m_children.clear();
}
