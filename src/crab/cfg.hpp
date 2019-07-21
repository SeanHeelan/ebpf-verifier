#pragma once

/*
 * Build a CFG to interface with the abstract domains and fixpoint
 * iterators.
 *
 * All the CFG statements are strongly typed. However, only variables
 * need to be typed. The types of constants can be inferred from the
 * context since they always appear together with at least one
 * variable. Types form a **flat** lattice consisting of:
 *
 * - integers,
 * - array of integers,
 *
 * Crab CFG supports the modelling of:
 *
 *   - arithmetic operations over integers or reals,
 *   - boolean operations,
 *   - C-like pointers,
 *   - uni-dimensional arrays of booleans, integers or pointers
 *     (useful for C-like arrays and heap abstractions),
 *   - and functions
 *
 * Important notes:
 *
 * - Objects of the class cfg_t are not copyable. Instead, we provide a
 *   class cfg_ref_t that wraps cfg_t references into copyable and
 *   assignable objects.
 *
 */
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <variant>

#include <boost/iterator/transform_iterator.hpp>
#include <boost/range/iterator_range.hpp>

#include "crab/bignums.hpp"
#include "crab/crab_syntax.hpp"
#include "crab/discrete_domains.hpp"
#include "crab/interval.hpp"
#include "crab/linear_constraints.hpp"
#include "crab/types.hpp"

#include "asm_syntax.hpp"

namespace crab {

class cfg_t;

template <typename Language>
class basic_block final {
    using basic_block_t = basic_block<Language>;

    basic_block(const basic_block_t&) = delete;

    friend class cfg_t;

  private:
    using bb_id_set_t = std::vector<label_t>;
    using stmt_list_t = std::vector<Language>;

  public:
    // -- iterators

    using succ_iterator = bb_id_set_t::iterator;
    using const_succ_iterator = bb_id_set_t::const_iterator;
    using pred_iterator = succ_iterator;
    using const_pred_iterator = const_succ_iterator;
    using iterator = typename stmt_list_t::iterator;
    using const_iterator = typename stmt_list_t::const_iterator;
    using reverse_iterator = typename stmt_list_t::reverse_iterator;
    using const_reverse_iterator = typename stmt_list_t::const_reverse_iterator;

  private:
    label_t m_label;
    stmt_list_t m_ts;
    bb_id_set_t m_prev, m_next;

    void insert_adjacent(bb_id_set_t& c, label_t e) {
        if (std::find(c.begin(), c.end(), e) == c.end()) {
            c.push_back(e);
        }
    }

    void remove_adjacent(bb_id_set_t& c, label_t e) {
        if (std::find(c.begin(), c.end(), e) != c.end()) {
            c.erase(std::remove(c.begin(), c.end(), e), c.end());
        }
    }

  public:
    template <typename T, typename... Args>
    void insert(Args&&... args) {
        m_ts.emplace_back(T{std::forward<Args>(args)...});
    }

    basic_block(const label_t& _label) : m_label(_label) {}

    basic_block(basic_block_t&& bb)
        : m_label(bb.label()), m_ts(std::move(bb.m_ts)), m_prev(bb.m_prev), m_next(bb.m_next) {}

    ~basic_block() = default;

    label_t label() const { return m_label; }

    std::string name() const { return m_label; }

    iterator begin() { return (m_ts.begin()); }
    iterator end() { return (m_ts.end()); }
    const_iterator begin() const { return (m_ts.begin()); }
    const_iterator end() const { return (m_ts.end()); }

    reverse_iterator rbegin() { return (m_ts.rbegin()); }
    reverse_iterator rend() { return (m_ts.rend()); }
    const_reverse_iterator rbegin() const { return (m_ts.rbegin()); }
    const_reverse_iterator rend() const { return (m_ts.rend()); }

    size_t size() const { return std::distance(begin(), end()); }

    std::pair<succ_iterator, succ_iterator> next_blocks() { return std::make_pair(m_next.begin(), m_next.end()); }

    std::pair<pred_iterator, pred_iterator> prev_blocks() { return std::make_pair(m_prev.begin(), m_prev.end()); }

    std::pair<const_succ_iterator, const_succ_iterator> next_blocks() const {
        return std::make_pair(m_next.begin(), m_next.end());
    }

    std::pair<const_pred_iterator, const_pred_iterator> prev_blocks() const {
        return std::make_pair(m_prev.begin(), m_prev.end());
    }

    // Add a cfg_t edge from *this to b
    void operator>>(basic_block_t& b) {
        insert_adjacent(m_next, b.m_label);
        insert_adjacent(b.m_prev, m_label);
    }

    // Remove a cfg_t edge from *this to b
    void operator-=(basic_block_t& b) {
        remove_adjacent(m_next, b.m_label);
        remove_adjacent(b.m_prev, m_label);
    }

    // insert all statements of other at the back
    void move_back(basic_block_t& other) {
        m_ts.reserve(m_ts.size() + other.m_ts.size());
        std::move(other.m_ts.begin(), other.m_ts.end(), std::back_inserter(m_ts));
    }

    void write(crab_os& o) const {
        o << m_label << ":\n";
        for (auto const& s : *this) {
            o << "  " << s << ";\n";
        }
        auto [it, et] = next_blocks();
        if (it != et) {
            o << "  "
              << "goto ";
            for (; it != et;) {
                o << *it;
                ++it;
                if (it == et) {
                    o << ";";
                } else {
                    o << ",";
                }
            }
        }
        o << "\n";
    }

    // for gdb
    void dump() const { write(errs()); }

    friend crab_os& operator<<(crab_os& o, const basic_block_t& b) {
        b.write(o);
        return o;
    }
};

// Viewing basic_block_t with all statements reversed. Useful for
// backward analysis.
template <typename Language>
class basic_block_rev final {
    using basic_block_rev_t = basic_block_rev<Language>;
    using basic_block_t = basic_block<Language>;
  public:
    using succ_iterator = typename basic_block_t::succ_iterator;
    using const_succ_iterator = typename basic_block_t::const_succ_iterator;
    using pred_iterator = succ_iterator;
    using const_pred_iterator = const_succ_iterator;

    using iterator = typename basic_block_t::reverse_iterator;
    using const_iterator = typename basic_block_t::const_reverse_iterator;

  private:
  public:
    basic_block_t& _bb;

    basic_block_rev(basic_block_t& bb) : _bb(bb) {}

    label_t label() const { return _bb.label(); }

    std::string name() const { return _bb.name(); }

    iterator begin() { return _bb.rbegin(); }

    iterator end() { return _bb.rend(); }

    const_iterator begin() const { return _bb.rbegin(); }

    const_iterator end() const { return _bb.rend(); }

    std::size_t size() const { return std::distance(begin(), end()); }

    std::pair<succ_iterator, succ_iterator> next_blocks() { return _bb.prev_blocks(); }

    std::pair<pred_iterator, pred_iterator> prev_blocks() { return _bb.next_blocks(); }

    std::pair<const_succ_iterator, const_succ_iterator> next_blocks() const { return _bb.prev_blocks(); }

    std::pair<const_pred_iterator, const_pred_iterator> prev_blocks() const { return _bb.next_blocks(); }

    void write(crab_os& o) const {
        o << name() << ":\n";
        for (auto const& s : *this) {
            o << "  " << s << ";\n";
        }
        o << "--> [";
        for (auto const& n : boost::make_iterator_range(next_blocks())) {
            o << n << ";";
        }
        o << "]\n";
    }

    // for gdb
    void dump() const { write(errs()); }

    friend crab_os& operator<<(crab_os& o, const basic_block_rev_t& b) {
        b.write(o);
        return o;
    }
};

using basic_block_t = basic_block<new_statement_t>;
using basic_block_rev_t = basic_block_rev<new_statement_t>;

class cfg_t final {
  public:
    using node_t = label_t; // for Bgl graphs

    using succ_iterator = basic_block_t::succ_iterator;
    using pred_iterator = basic_block_t::pred_iterator;
    using const_succ_iterator = basic_block_t::const_succ_iterator;
    using const_pred_iterator = basic_block_t::const_pred_iterator;

    using succ_range = boost::iterator_range<succ_iterator>;
    using pred_range = boost::iterator_range<pred_iterator>;
    using const_succ_range = boost::iterator_range<const_succ_iterator>;
    using const_pred_range = boost::iterator_range<const_pred_iterator>;

  private:
    using basic_block_map_t = std::unordered_map<label_t, basic_block_t>;
    using binding_t = basic_block_map_t::value_type;

    struct get_label : public std::unary_function<binding_t, label_t> {
        get_label() {}
        label_t operator()(const binding_t& p) const { return p.second.label(); }
    };

  public:
    using iterator = basic_block_map_t::iterator;
    using const_iterator = basic_block_map_t::const_iterator;
    using label_iterator = boost::transform_iterator<get_label, basic_block_map_t::iterator>;
    using const_label_iterator = boost::transform_iterator<get_label, basic_block_map_t::const_iterator>;

    using var_iterator = std::vector<varname_t>::iterator;
    using const_var_iterator = std::vector<varname_t>::const_iterator;

  private:
    label_t m_entry;
    std::optional<label_t> m_exit;
    basic_block_map_t m_blocks;

    using visited_t = std::unordered_set<label_t>;
    template <typename T>
    void dfs_rec(label_t curId, visited_t& visited, T f) const {
        if (!visited.insert(curId).second)
            return;

        const auto& cur = get_node(curId);
        f(cur);
        for (auto const n : boost::make_iterator_range(cur.next_blocks())) {
            dfs_rec(n, visited, f);
        }
    }

    template <typename T>
    void dfs(T f) const {
        visited_t visited;
        dfs_rec(m_entry, visited, f);
    }

  public:
    cfg_t(label_t entry) : m_entry(entry), m_exit(std::nullopt) { m_blocks.emplace(entry, entry); }

    cfg_t(label_t entry, label_t exit) : m_entry(entry), m_exit(exit) {
        m_blocks.emplace(entry, entry);
        m_blocks.emplace(exit, exit);
    }

    cfg_t(const cfg_t&) = delete;

    cfg_t(cfg_t&& o) : m_entry(o.m_entry), m_exit(o.m_exit), m_blocks(std::move(o.m_blocks)) {}

    ~cfg_t() = default;

    bool has_exit() const { return (bool)m_exit; }

    label_t exit() const {
        if (has_exit())
            return *m_exit;
        CRAB_ERROR("cfg_t does not have an exit block");
    }

    //! set method to mark the exit block after the cfg_t has been
    //! created.
    void set_exit(label_t exit) { m_exit = exit; }

    // --- Begin ikos fixpoint API

    label_t entry() const { return m_entry; }

    const_succ_range next_nodes(label_t _label) const {
        return boost::make_iterator_range(get_node(_label).next_blocks());
    }

    const_pred_range prev_nodes(label_t _label) const {
        return boost::make_iterator_range(get_node(_label).prev_blocks());
    }

    succ_range next_nodes(label_t _label) {
        return boost::make_iterator_range(get_node(_label).next_blocks());
    }

    pred_range prev_nodes(label_t _label) {
        return boost::make_iterator_range(get_node(_label).prev_blocks());
    }

    basic_block_t& get_node(label_t _label) {
        auto it = m_blocks.find(_label);
        if (it == m_blocks.end()) {
            CRAB_ERROR("Basic block ", _label, " not found in the CFG: ", __LINE__);
        }
        return it->second;
    }

    const basic_block_t& get_node(label_t _label) const {
        auto it = m_blocks.find(_label);
        if (it == m_blocks.end()) {
            CRAB_ERROR("Basic block ", _label, " not found in the CFG: ", __LINE__);
        }
        return it->second;
    }

    // --- End ikos fixpoint API

    basic_block_t& insert(label_t _label);

    void remove(label_t _label);

    //! return a begin iterator of basic_block_t's
    iterator begin() { return m_blocks.begin(); }

    //! return an end iterator of basic_block_t's
    iterator end() { return m_blocks.end(); }

    const_iterator begin() const { return m_blocks.begin(); }

    const_iterator end() const { return m_blocks.end(); }

    //! return a begin iterator of label_t's
    label_iterator label_begin() { return boost::make_transform_iterator(m_blocks.begin(), get_label()); }

    //! return an end iterator of label_t's
    label_iterator label_end() { return boost::make_transform_iterator(m_blocks.end(), get_label()); }

    const_label_iterator label_begin() const { return boost::make_transform_iterator(m_blocks.begin(), get_label()); }

    const_label_iterator label_end() const { return boost::make_transform_iterator(m_blocks.end(), get_label()); }

    size_t size() const { return std::distance(begin(), end()); }

    void write(crab_os& o) const {
        dfs([&](const auto& bb) { bb.write(o); });
    }

    // for gdb
    void dump() const {
        errs() << "number_t of basic blocks=" << size() << "\n";
        for (auto& [label, bb] : boost::make_iterator_range(begin(), end())) {
            bb.dump();
        }
    }

    friend crab_os& operator<<(crab_os& o, const cfg_t& cfg) {
        cfg.write(o);
        return o;
    }

    void simplify() {
        merge_blocks();
        remove_unreachable_blocks();
        remove_useless_blocks();
        // after removing useless blocks there can be opportunities to
        // merge more blocks.
        merge_blocks();
        merge_blocks();
    }

  private:
    ////
    // Trivial cfg_t simplifications
    // TODO: move to transform directory
    ////

    // Helpers
    bool has_one_child(label_t b) const {
        auto rng = next_nodes(b);
        return (std::distance(rng.begin(), rng.end()) == 1);
    }

    bool has_one_parent(label_t b) const {
        auto rng = prev_nodes(b);
        return (std::distance(rng.begin(), rng.end()) == 1);
    }

    basic_block_t& get_child(label_t b) {
        assert(has_one_child(b));
        auto rng = next_nodes(b);
        return get_node(*(rng.begin()));
    }

    basic_block_t& get_parent(label_t b) {
        assert(has_one_parent(b));
        auto rng = prev_nodes(b);
        return get_node(*(rng.begin()));
    }

    void merge_blocks_rec(label_t curId, visited_t& visited) {
        if (!visited.insert(curId).second)
            return;

        auto& cur = get_node(curId);

        if (has_one_child(curId) && has_one_parent(curId)) {
            auto& parent = get_parent(curId);
            auto& child = get_child(curId);

            // Merge with its parent if it's its only child.
            if (has_one_child(parent.label())) {
                // move all statements from cur to parent
                parent.move_back(cur);
                visited.erase(curId);
                remove(curId);
                parent >> child;
                merge_blocks_rec(child.label(), visited);
                return;
            }
        }

        for (auto n : boost::make_iterator_range(cur.next_blocks())) {
            merge_blocks_rec(n, visited);
        }
    }

    // Merges a basic block into its predecessor if there is only one
    // and the predecessor only has one successor.
    void merge_blocks() {
        visited_t visited;
        merge_blocks_rec(entry(), visited);
    }

    // mark reachable blocks from curId
    template <class AnyCfg>
    void mark_alive_blocks(label_t curId, AnyCfg& cfg_t, visited_t& visited) {
        if (visited.count(curId) > 0)
            return;
        visited.insert(curId);
        for (auto child : cfg_t.next_nodes(curId)) {
            mark_alive_blocks(child, cfg_t, visited);
        }
    }

    void remove_unreachable_blocks();

    // remove blocks that cannot reach the exit block
    void remove_useless_blocks();
};

// A lightweight object that wraps a reference to a CFG into a
// copyable, assignable object.
class cfg_ref_t final {
  public:
    // cfg_t's typedefs
    using node_t = cfg_t::node_t;

    using succ_iterator = cfg_t::succ_iterator;
    using pred_iterator = cfg_t::pred_iterator;
    using const_succ_iterator = cfg_t::const_succ_iterator;
    using const_pred_iterator = cfg_t::const_pred_iterator;
    using succ_range = cfg_t::succ_range;
    using pred_range = cfg_t::pred_range;
    using const_succ_range = cfg_t::const_succ_range;
    using const_pred_range = cfg_t::const_pred_range;
    using iterator = cfg_t::iterator;
    using const_iterator = cfg_t::const_iterator;
    using label_iterator = cfg_t::label_iterator;
    using const_label_iterator = cfg_t::const_label_iterator;
    using var_iterator = cfg_t::var_iterator;
    using const_var_iterator = cfg_t::const_var_iterator;

  private:
    std::reference_wrapper<cfg_t> _ref;

    const cfg_t& get() const { return _ref; }

    cfg_t& get() { return _ref; }

  public:
    cfg_ref_t(cfg_t& cfg) : _ref(std::ref(cfg)) {}

    label_t entry() const { return get().entry(); }

    const_succ_range next_nodes(label_t bb) const { return get().next_nodes(bb); }

    const_pred_range prev_nodes(label_t bb) const { return get().prev_nodes(bb); }

    succ_range next_nodes(label_t bb) { return get().next_nodes(bb); }

    pred_range prev_nodes(label_t bb) { return get().prev_nodes(bb); }

    basic_block_t& get_node(label_t bb) { return get().get_node(bb); }

    const basic_block_t& get_node(label_t bb) const { return get().get_node(bb); }

    size_t size() const { return get().size(); }

    iterator begin() { return get().begin(); }

    iterator end() { return get().end(); }

    const_iterator begin() const { return get().begin(); }

    const_iterator end() const { return get().end(); }

    label_iterator label_begin() { return get().label_begin(); }

    label_iterator label_end() { return get().label_end(); }

    const_label_iterator label_begin() const { return get().label_begin(); }

    const_label_iterator label_end() const { return get().label_end(); }

    bool has_exit() const { return get().has_exit(); }

    label_t exit() const { return get().exit(); }

    friend crab_os& operator<<(crab_os& o, const cfg_ref_t& cfg) { return o << cfg.get(); }

    // for gdb
    void dump() const { get().dump(); }

    void simplify() { get().simplify(); }
};

// Viewing a cfg_t with all edges and block statements
// reversed. Useful for backward analysis.
class cfg_rev_t final {
  public:
    using node_t = label_t; // for Bgl graphs

    using pred_range = cfg_t::succ_range;
    using succ_range = cfg_t::pred_range;
    using const_pred_range = cfg_t::const_succ_range;
    using const_succ_range = cfg_t::const_pred_range;

    // For BGL
    using succ_iterator = basic_block_t::succ_iterator;
    using pred_iterator = basic_block_t::pred_iterator;
    using const_succ_iterator = basic_block_t::const_succ_iterator;
    using const_pred_iterator = basic_block_t::const_pred_iterator;

  private:
    using visited_t = std::unordered_set<label_t>;

    template <typename T>
    void dfs_rec(label_t curId, visited_t& visited, T f) const {
        if (!visited.insert(curId).second)
            return;
        f(get_node(curId));
        for (auto const n : next_nodes(curId)) {
            dfs_rec(n, visited, f);
        }
    }

    template <typename T>
    void dfs(T f) const {
        visited_t visited;
        dfs_rec(entry(), visited, f);
    }

  public:
    using basic_block_rev_map_t = std::unordered_map<label_t, basic_block_rev_t>;
    using iterator = basic_block_rev_map_t::iterator;
    using const_iterator = basic_block_rev_map_t::const_iterator;
    using label_iterator = cfg_t::label_iterator;
    using const_label_iterator = cfg_t::const_label_iterator;
    using var_iterator = cfg_t::var_iterator;
    using const_var_iterator = cfg_t::const_var_iterator;

  private:
    cfg_t& _cfg;
    basic_block_rev_map_t _rev_bbs;

  public:
    cfg_rev_t(cfg_t& cfg) : _cfg(cfg) {
        // Create basic_block_rev_t from basic_block_t objects
        // Note that basic_block_rev_t is also a view of basic_block_t so it
        // doesn't modify basic_block_t objects.
        for (auto& [label, bb] : cfg) {
            _rev_bbs.emplace(label, bb);
        }
    }

    cfg_rev_t(const cfg_rev_t& o) : _cfg(o._cfg), _rev_bbs(o._rev_bbs) {}

    cfg_rev_t(cfg_rev_t&& o) : _cfg(o._cfg), _rev_bbs(std::move(o._rev_bbs)) {}

    label_t entry() const {
        if (!_cfg.has_exit())
            CRAB_ERROR("Entry not found!");
        return _cfg.exit();
    }

    const_succ_range next_nodes(label_t bb) const { return _cfg.prev_nodes(bb); }

    const_pred_range prev_nodes(label_t bb) const { return _cfg.next_nodes(bb); }

    succ_range next_nodes(label_t bb) { return _cfg.prev_nodes(bb); }

    pred_range prev_nodes(label_t bb) { return _cfg.next_nodes(bb); }

    basic_block_rev_t& get_node(label_t _label) {
        auto it = _rev_bbs.find(_label);
        if (it == _rev_bbs.end())
            CRAB_ERROR("Basic block ", _label, " not found in the CFG: ", __LINE__);
        return it->second;
    }

    const basic_block_rev_t& get_node(label_t _label) const {
        auto it = _rev_bbs.find(_label);
        if (it == _rev_bbs.end())
            CRAB_ERROR("Basic block ", _label, " not found in the CFG: ", __LINE__);
        return it->second;
    }

    iterator begin() { return _rev_bbs.begin(); }

    iterator end() { return _rev_bbs.end(); }

    const_iterator begin() const { return _rev_bbs.begin(); }

    const_iterator end() const { return _rev_bbs.end(); }

    label_iterator label_begin() { return _cfg.label_begin(); }

    label_iterator label_end() { return _cfg.label_end(); }

    const_label_iterator label_begin() const { return _cfg.label_begin(); }

    const_label_iterator label_end() const { return _cfg.label_end(); }

    bool has_exit() const { return true; }

    label_t exit() const { return _cfg.entry(); }

    void write(crab_os& o) const {
        dfs([&](const auto& bb) { bb.write(o); });
    }

    friend crab_os& operator<<(crab_os& o, const cfg_rev_t& cfg_t) {
        cfg_t.write(o);
        return o;
    }

    void simplify() {}
};

} // end namespace crab
