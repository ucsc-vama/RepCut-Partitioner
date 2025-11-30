//
// Persistent cone-id trie.
//
// Every vertex in the design accumulates the set of cones (sink ancestor
// subgraphs) it belongs to.  Rather than storing one set per vertex, this
// trie shares common prefixes/suffixes across vertices that belong to the
// same cones.  Vertices whose cone-id sets are identical end up pointing at
// the exact same leaf node, so set equality reduces to pointer equality.
//
// Ordering invariant (critical):
//   Cones MUST be inserted in strictly increasing cone-id order, and within
//   a single cone a vertex is visited at most once.  This guarantees that
//   for each vertex the sequence of cone ids appended along its root-to-leaf
//   path is the canonical (sorted, strictly increasing, duplicate-free) set
//   of cone ids that vertex belongs to.  Two vertices sharing the same set
//   of cones then share the exact same leaf node, so set equality reduces
//   to pointer equality (used by the cluster grouping pass).
//
//   Violating the order invariant (e.g. processing cones out of order, or
//   concurrently inserting different cone ids into the same vertex) would
//   produce non-canonical paths and break the equality-by-pointer property,
//   silently corrupting the clustering.  This is why the cone-marking pass
//   is sequential.
//

#ifndef RCP_CONE_TRIE_H
#define RCP_CONE_TRIE_H

#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

namespace repcut {

    class ConeTrie {
    public:
        struct Node {
            // Cone id added at this trie level.  Root uses UINT32_MAX as sentinel.
            uint32_t value;
            Node* parent;
            // Children keyed by cone id.  Kept sorted by cone id so we can use
            // binary search on insert/lookup.  Vectors stay small in practice
            // (most nodes have 1-2 children), so the per-node overhead is far
            // smaller than an unordered_map per node.
            std::vector<std::pair<uint32_t, Node*>> children;

            Node() : value(UINT32_MAX), parent(nullptr) {}
            Node(uint32_t v, Node* p) : value(v), parent(p) {}
        };

        ConeTrie() = default;

        // Root node.  Stable address (it is a member, never reallocated).
        Node* root() { return &root_; }
        const Node* root() const { return &root_; }

        // Descend from `node` along the child keyed by `cone_id`, allocating
        // a new child node if none exists.  Returned pointer is stable for the
        // lifetime of this ConeTrie.  `node` must be a node owned by this trie
        // (e.g. previously returned by visit() or root()).
        Node* visit(Node* node, uint32_t cone_id);

        // Returns the cone ids on the path from root to `leaf` in increasing
        // order.  Passing root() yields an empty vector.
        std::vector<uint32_t> pathConeIds(const Node* leaf) const;

        size_t nodeCount() const { return arena_.size(); }

    private:
        Node root_;
        // Stable-address node pool: appending to a deque never invalidates
        // pointers/references to its elements, so child pointers in the trie
        // remain valid as the arena grows.
        std::deque<Node> arena_;
    };

}

#endif //RCP_CONE_TRIE_H