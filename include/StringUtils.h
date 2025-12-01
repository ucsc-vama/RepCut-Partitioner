//
// Zero-allocation whitespace token splitter.
//
// Yields successive whitespace-separated tokens as string_views into the
// source buffer.  Tokens are already trimmed of leading/trailing whitespace
// by construction.  Intended for line-oriented parsing of the input graph
// and KaHyPar result files, where per-line std::string allocations would
// otherwise dominate parse time on multi-million-node designs.
//

#pragma once

#include <cctype>
#include <cerrno>
#include <string_view>

namespace repcut {

class TokenView {
    std::string_view sv;
public:
    explicit TokenView(std::string_view s) : sv(s) {}

    // Return the next whitespace-delimited token, or an empty string_view
    // if none remain.  Consumes leading/trailing whitespace automatically.
    std::string_view next() {
        while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front())))
            sv.remove_prefix(1);
        auto end = sv.begin();
        while (end != sv.end() && !std::isspace(static_cast<unsigned char>(*end)))
            ++end;
        std::string_view tok(sv.data(), static_cast<size_t>(end - sv.begin()));
        sv.remove_prefix(tok.size());
        return tok;
    }

    // True when no non-whitespace characters remain.
    bool done() const {
        for (auto c : sv) if (!std::isspace(static_cast<unsigned char>(c))) return false;
        return true;
    }

    // Count remaining whitespace-separated tokens without consuming them.
    size_t count() const {
        size_t n = 0;
        bool in_tok = false;
        for (auto c : sv) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                in_tok = false;
            } else if (!in_tok) {
                in_tok = true;
                ++n;
            }
        }
        return n;
    }
};

}  // namespace repcut