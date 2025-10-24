// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file MasterTree.h
 * @ingroup fne_network
 * @file MasterTree.cpp
 * @ingroup fne_network
 */
#if !defined(__MASTER_TREE_H__)
#define __MASTER_TREE_H__

#include "fne/Defines.h"
#include "common/network/json/json.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace network
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents an FNE spanning tree.
     * @ingroup fne_network
     * @remarks
     * This class implements a extremely rudimentary spanning tree structure to represent
     * the master FNE tree topology.
     * 
     * Each MasterTree node represents a single FNE in the tree. The root node is the master FNE
     * at the top of the tree. Each node contains a list of child nodes that are directly connected
     * to it downstream.
     * 
     * For example, consider the following tree structure:
     * 
     *            A
     *           / \
     *          B   C
     *         / \   \
     *        D   E   F
     *       /         \
     *      G           H
     * 
     * In this example, A is the root node (master FNE), B and C are its children,
     * D and E are children of B, F is a child of C, G is a child of D, and H is a child of F.
     * 
     * Child nodes always send their data upstream to their parent node. The tree is always a top-down
     * structure, with data flowing from the leaves up to the root. The root node does not have a parent.
     * 
     * Root nodes can have multiple child nodes, and child nodes can have their own children, forming a hierarchical tree.
     * Root nodes determine duplicate connections and enforce tree integrity.
     * 
     * Each node in the tree assumes it is a root of its own subtree. For instance, B considers itself
     * the root of the subtree containing B, D, E, and G. This allows for easy traversal and management of
     * the tree structure.
     */
    class HOST_SW_API MasterTree {
    public:
        auto operator=(MasterTree&) -> MasterTree& = delete;
        auto operator=(MasterTree&&) -> MasterTree& = delete;
        MasterTree(MasterTree&) = delete;

        /**
         * @brief Initializes a new instance of the MasterTree class
         * @param id Peer ID.
         * @param parent Parent server tree node.
         */
        MasterTree(uint32_t id, uint32_t masterId, MasterTree* parent);
        /**
         * @brief Finalizes a instance of the MasterTree class
         */
        ~MasterTree();

        /**
         * @brief Flag indicating whether or not this server is a tree root.
         * @return bool True, if server is the tree root, otherwise false.
         */
        bool isRoot() const { return (m_parent == nullptr); }
        /**
         * @brief Flag indicating whether or not this server has child nodes.
         * @return bool True, if server has child nodes, otherwise false.
         */
        bool hasChildren() const { return !m_children.empty(); }

        /**
         * @brief Find a peer master tree by peer ID.
         * @param peerId Peer ID.
         * @return MasterTree* Pointer to the MasterTree instance, or nullptr if not found.
         */
        static MasterTree* findByPeerID(const uint32_t peerId);
        /**
         * @brief Find a peer master tree by master peer ID.
         * @param masterId Master Peer ID.
         * @return MasterTree* Pointer to the MasterTree instance, or nullptr if not found.
         */
        static MasterTree* findByMasterID(const uint32_t masterId);

        /**
         * @brief Count all children of a master tree node.
         * @param node Pointer to MasterTree node.
         * @return uint32_t Number of child nodes.
         */
        static uint32_t countChildren(MasterTree* node);

        /**
         * @brief Erase a peer from the tree.
         * @param peerId Peer ID.
         */
        static void erasePeer(const uint32_t peerId);

        /**
         * @brief Helper to recursively serialize master tree node to JSON array.
         * @param node Pointer to MasterTree node.
         * @param jsonArray JSON array to write node to.
         */
        static void serializeTree(MasterTree* node, json::array& jsonArray);

        /**
         * @brief Helper to recursively deserialize master tree node from JSON array.
         * @param jsonArray JSON array to read node from.
         * @param parent Pointer to parent MasterTree node.
         * @param duplicatePeers Pointer to vector to receive duplicate peer IDs found during deserialization.
         */
        static void deserializeTree(json::array& jsonArray, MasterTree* parent, std::vector<uint32_t>* duplicatePeers);

        /**
         * @brief Helper to move the master tree node to a different parent master tree node.
         * @param node Pointer to a MasterTree node.
         * @param parent Pointer to new parent MasterTree Node.
         */
        static void moveParent(MasterTree* node, MasterTree* parent);

        /**
         * @brief Helper to visualize the tree structure in the log.
         * @param node Pointer to MasterTree node.
         * @param level Current tree level.
         */
        static void visualizeTreeToLog(MasterTree* node, uint32_t level = 0U);

    public:
        MasterTree* m_parent;                   //!< Parent master tree node. (i.e. master FNE above this)
        std::vector<MasterTree*> m_children;    //!< Child master tree nodes. (i.e. peer FNEs below this)

        static std::unordered_map<uint32_t, MasterTree*> m_masterTrees; //!< Static map of all master trees by peer ID.
        static uint8_t m_maxUpdatesBeforeReparent; //!< Maximum count of updates before allowing node reparenting.

        /**
         * @brief Peer ID.
         */
        DECLARE_PROPERTY_PLAIN(uint32_t, id);
        /**
         * @brief Master Peer ID.
         */
        DECLARE_PROPERTY_PLAIN(uint32_t, masterId);

    private:
        uint8_t m_updatesBeforeReparent;

        /**
         * @brief Helper to erase all children of a master tree node.
         * @param node Pointer to MasterTree node.
         */
        static void eraseChildren(MasterTree* node);
    };
} // namespace network

#endif // __MASTER_TREE_H__
