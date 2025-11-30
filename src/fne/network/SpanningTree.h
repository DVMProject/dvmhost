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
 * @file SpanningTree.h
 * @ingroup fne_network
 * @file SpanningTree.cpp
 * @ingroup fne_network
 */
#if !defined(__SPANNING_TREE_H__)
#define __SPANNING_TREE_H__

#include "fne/Defines.h"
#include "common/json/json.h"

#include <string>
#include <mutex>
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
     *  the linked FNE tree topology.
     * 
     * Each node represents a master FNE in the tree. The root node is the master FNE
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
     * - Nodes can have multiple child nodes, and child nodes can have their own children, forming a hierarchical tree.
     * - Nodes with child nodes can determine duplicate connections and enforce tree integrity.     * 
     * - Each node in the tree assumes it is the root of its own subtree. For instance, B considers itself
     *      the root of the subtree containing B, D, E, and G. This allows for easy traversal and management of
     *      the tree structure.
     */
    class HOST_SW_API SpanningTree {
    public:
        auto operator=(SpanningTree&) -> SpanningTree& = delete;
        auto operator=(SpanningTree&&) -> SpanningTree& = delete;
        SpanningTree(SpanningTree&) = delete;

        /**
         * @brief Initializes a new instance of the SpanningTree class
         * @param id Peer ID.
         * @param parent Parent server tree node.
         */
        SpanningTree(uint32_t id, uint32_t masterId, SpanningTree* parent);
        /**
         * @brief Finalizes a instance of the SpanningTree class
         */
        ~SpanningTree();

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
         * @brief Find a peer tree by peer ID.
         * @param peerId Peer ID.
         * @return SpanningTree* Pointer to the SpanningTree instance, or nullptr if not found.
         */
        static SpanningTree* findByPeerID(const uint32_t peerId);
        /**
         * @brief Find a peer tree by master peer ID.
         * @param masterId Master Peer ID.
         * @return SpanningTree* Pointer to the SpanningTree instance, or nullptr if not found.
         */
        static SpanningTree* findByMasterID(const uint32_t masterId);

        /**
         * @brief Count all children of a tree node.
         * @param node Pointer to SpanningTree node.
         * @return uint32_t Number of child nodes.
         */
        static uint32_t countChildren(SpanningTree* node);

        /**
         * @brief Erase a peer from the tree.
         * @param peerId Peer ID.
         */
        static void erasePeer(const uint32_t peerId);

        /**
         * @brief Helper to recursively serialize tree node to JSON array.
         * @param node Pointer to SpanningTree node.
         * @param jsonArray JSON array to write node to.
         */
        static void serializeTree(SpanningTree* node, json::array& jsonArray);

        /**
         * @brief Helper to recursively deserialize tree node from JSON array.
         * @param jsonArray JSON array to read node from.
         * @param parent Pointer to parent SpanningTree node.
         * @param duplicatePeers Pointer to vector to receive duplicate peer IDs found during deserialization.
         */
        static void deserializeTree(json::array& jsonArray, SpanningTree* parent, std::vector<uint32_t>* duplicatePeers);

        /**
         * @brief Helper to move the tree node to a different parent tree node.
         * @param node Pointer to a SpanningTree node.
         * @param parent Pointer to new parent SpanningTree Node.
         */
        static void moveParent(SpanningTree* node, SpanningTree* parent);

        /**
         * @brief Helper to visualize the tree structure in the log.
         * @param node Pointer to SpanningTree node.
         * @param level Current tree level.
         */
        static void visualizeTreeToLog(SpanningTree* node, uint32_t level = 0U);

    public:
        SpanningTree* m_parent;                   //!< Parent tree node. (i.e. master FNE above this)
        std::vector<SpanningTree*> m_children;    //!< Child tree nodes. (i.e. peer FNEs below this)

        static std::mutex s_mutex;
        static std::unordered_map<uint32_t, SpanningTree*> s_spanningTrees; //!< Static map of all trees by peer ID.
        static uint8_t s_maxUpdatesBeforeReparent; //!< Maximum count of updates before allowing node reparenting.

        /**
         * @brief Peer Identity.
         */
        DECLARE_PROPERTY_PLAIN(std::string, identity);
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
         * @brief Helper to recursively deserialize tree node from JSON array.
         * @param jsonArray JSON array to read node from.
         * @param parent Pointer to parent SpanningTree node.
         * @param duplicatePeers Pointer to vector to receive duplicate peer IDs found during deserialization.
         * @param noReparent If true, disables reparenting during deserialization.
         */
        static void internalDeserializeTree(json::array& jsonArray, SpanningTree* parent, 
            std::vector<uint32_t>* duplicatePeers, bool noReparent = false);
        /**
         * @brief Helper to move the tree node to a different parent tree node.
         * @param node Pointer to a SpanningTree node.
         * @param parent Pointer to new parent SpanningTree Node.
         */
        static void internalMoveParent(SpanningTree* node, SpanningTree* parent);
        /**
         * @brief Erase a peer from the tree.
         * @param peerId Peer ID.
         */
        static void internalErasePeer(const uint32_t peerId);
    };
} // namespace network

#endif // __SPANNING_TREE_H__
