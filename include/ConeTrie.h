// Persistent cone-id trie: vertices with the same cone set share a leaf
// node (pointer equality).  Cones MUST be inserted in strictly increasing
// cone-id order (see ordering invariant).

#pragma once

#include <cstdint>
#include <deque>
#include <utility>
#include <vector>
#include <cstddef>

namespace repcut {

    class ConeTrie
    {
    public:
        struct Node
        {
            // Cone id added at this trie level.  Root uses UINT32_MAX as sentinel.
            uint32_t value;
            Node* parent;
            // Children sorted by cone id for binary search on insert/lookup.
            std::vector<std::pair<uint32_t, Node*>> children;

            Node() : value(UINT32_MAX), parent(nullptr) {}
            Node(uint32_t v, Node* p) : value(v), parent(p) {}
        };

        ConeTrie() = default;

        // Root node (stable address, never reallocated).
        Node* root() { return &root_; }
        const Node* root() const { return &root_; }

        // Descend to child keyed by cone_id, allocating if needed.
        Node* visit(Node* node, uint32_t cone_id);

        // Cone ids from root to leaf in increasing order (empty for root).
        std::vector<uint32_t> pathConeIds(const Node* leaf) const;

        size_t nodeCount() const { return arena_.size(); }

    private:
        Node root_;
        // Stable-address node pool (deque never invalidates pointers).
        std::deque<Node> arena_;
    };

} // namespace repcut
