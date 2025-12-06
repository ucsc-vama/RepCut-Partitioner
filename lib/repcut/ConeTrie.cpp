#include "ConeTrie.h"

#include <algorithm>

namespace repcut {

    ConeTrie::Node* ConeTrie::visit(Node* node, uint32_t cone_id)
    {
        auto& cs = node->children;
        auto it = std::lower_bound(cs.begin(), cs.end(), cone_id,
                                   [](const std::pair<uint32_t, Node*>& a, uint32_t b) { return a.first < b; });
        if (it != cs.end() && it->first == cone_id)
            return it->second;
        arena_.emplace_back(cone_id, node);
        Node* child = &arena_.back();
        cs.insert(it, std::make_pair(cone_id, child));
        return child;
    }

    std::vector<uint32_t> ConeTrie::pathConeIds(const Node* leaf) const
    {
        std::vector<uint32_t> ids;
        for (const Node* n = leaf; n != nullptr && n->parent != nullptr; n = n->parent)
            ids.push_back(n->value);
        std::reverse(ids.begin(), ids.end());
        return ids;
    }

} // namespace repcut