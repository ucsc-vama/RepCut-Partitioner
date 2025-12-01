//
// Persistent cone-id trie implementation.
//

#include "ConeTrie.h"

#include <algorithm>

namespace repcut {

    ConeTrie::Node* ConeTrie::visit(Node* node, uint32_t cone_id) {
        auto& cs = node->children;

        // Binary search by cone id (children are kept sorted by insert key,
        // and cone ids are processed in increasing order so inserts append
        // to the end in practice, but we still handle generic insertion).
        auto it = std::lower_bound(
                cs.begin(), cs.end(), cone_id,
                [](const std::pair<uint32_t, Node*>& a, uint32_t b) {
                    return a.first < b;
                });

        if (it != cs.end() && it->first == cone_id) {
            return it->second;
        }

        arena_.emplace_back(cone_id, node);
        Node* child = &arena_.back();
        cs.insert(it, std::make_pair(cone_id, child));
        return child;
    }

    std::vector<uint32_t> ConeTrie::pathConeIds(const Node* leaf) const {
        std::vector<uint32_t> ids;
        // Walk leaf -> root.  Skip the root node itself (parent == nullptr).
        for (const Node* n = leaf; n != nullptr && n->parent != nullptr; n = n->parent) {
            ids.push_back(n->value);
        }
        // Path was built root -> leaf in increasing cone-id order, so the
        // reverse gives ascending order.
        std::reverse(ids.begin(), ids.end());
        return ids;
    }

}