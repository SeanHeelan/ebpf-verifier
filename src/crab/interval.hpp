/*******************************************************************************
 *
 * A simple class for representing intervals and performing interval arithmetic.
 *
 ******************************************************************************/

#pragma once

#include <optional>

#include "crab/stats.hpp"
#include "crab/types.hpp"

namespace crab {

class bound_t final {
  private:
    bool _is_infinite;
    number_t _n;

  private:
    bound_t();

    bound_t(bool is_infinite, number_t n) : _is_infinite(is_infinite), _n(n) {
        if (is_infinite) {
            if (n > 0)
                _n = 1;
            else
                _n = -1;
        }
    }

  public:
    static bound_t min(bound_t x, bound_t y) { return (x.operator<=(y) ? x : y); }

    static bound_t min(bound_t x, bound_t y, bound_t z) { return min(x, min(y, z)); }

    static bound_t min(bound_t x, bound_t y, bound_t z, bound_t t) { return min(x, min(y, z, t)); }

    static bound_t max(bound_t x, bound_t y) { return (x.operator<=(y) ? y : x); }

    static bound_t max(bound_t x, bound_t y, bound_t z) { return max(x, max(y, z)); }

    static bound_t max(bound_t x, bound_t y, bound_t z, bound_t t) { return max(x, max(y, z, t)); }

    static bound_t plus_infinity() { return bound_t(true, 1); }

    static bound_t minus_infinity() { return bound_t(true, -1); }

  public:
    bound_t(int n) : _is_infinite(false), _n(n) {}

    bound_t(number_t n) : _is_infinite(false), _n(n) {}

    bound_t(const bound_t& o) : _is_infinite(o._is_infinite), _n(o._n) {}

    bound_t& operator=(const bound_t& o) {
        if (this != &o) {
            _is_infinite = o._is_infinite;
            _n = o._n;
        }
        return *this;
    }

    bool is_infinite() const { return _is_infinite; }

    bool is_finite() const { return !_is_infinite; }

    bool is_plus_infinity() const { return (is_infinite() && _n > 0); }

    bool is_minus_infinity() const { return (is_infinite() && _n < 0); }

    bound_t operator-() const { return bound_t(_is_infinite, -_n); }

    bound_t operator+(bound_t x) const {
        if (is_finite() && x.is_finite()) {
            return bound_t(_n + x._n);
        } else if (is_finite() && x.is_infinite()) {
            return x;
        } else if (is_infinite() && x.is_finite()) {
            return *this;
        } else if (_n == x._n) {
            return *this;
        } else {
            CRAB_ERROR("Bound: undefined operation -oo + +oo");
        }
    }

    bound_t& operator+=(bound_t x) { return operator=(operator+(x)); }

    bound_t operator-(bound_t x) const { return operator+(x.operator-()); }

    bound_t& operator-=(bound_t x) { return operator=(operator-(x)); }

    bound_t operator*(bound_t x) const {
        if (x._n == 0)
            return x;
        else if (_n == 0)
            return *this;
        else
            return bound_t(_is_infinite || x._is_infinite, _n * x._n);
    }

    bound_t& operator*=(bound_t x) { return operator=(operator*(x)); }

    bound_t operator/(bound_t x) const {
        if (x._n == 0) {
            CRAB_ERROR("Bound: division by zero");
        } else if (is_finite() && x.is_finite()) {
            return bound_t(false, _n / x._n);
        } else if (is_finite() && x.is_infinite()) {
            if (_n > 0) {
                return x;
            } else if (_n == 0) {
                return *this;
            } else {
                return x.operator-();
            }
        } else if (is_infinite() && x.is_finite()) {
            if (x._n > 0) {
                return *this;
            } else {
                return operator-();
            }
        } else {
            return bound_t(true, _n * x._n);
        }
    }

    bound_t& operator/=(bound_t x) { return operator=(operator/(x)); }

    bool operator<(bound_t x) const { return !operator>=(x); }

    bool operator>(bound_t x) const { return !operator<=(x); }

    bool operator==(bound_t x) const { return (_is_infinite == x._is_infinite && _n == x._n); }

    bool operator!=(bound_t x) const { return !operator==(x); }

    /*	operator<= and operator>= use a somewhat optimized implementation.
     *	results include up to 20% improvements in performance in the octagon domain
     *	over a more naive implementation.
     */
    bool operator<=(bound_t x) const {
        if (_is_infinite xor x._is_infinite) {
            if (_is_infinite) {
                return _n < 0;
            }
            return x._n > 0;
        }
        return _n <= x._n;
    }

    bool operator>=(bound_t x) const {
        if (_is_infinite xor x._is_infinite) {
            if (_is_infinite) {
                return _n > 0;
            }
            return x._n < 0;
        }
        return _n >= x._n;
    }

    bound_t abs() const {
        if (operator>=(0)) {
            return *this;
        } else {
            return operator-();
        }
    }

    std::optional<number_t> number() const {
        if (is_infinite()) {
            return std::optional<number_t>();
        } else {
            return std::optional<number_t>(_n);
        }
    }

    void write(std::ostream& o) const {
        if (is_plus_infinity()) {
            o << "+oo";
        } else if (is_minus_infinity()) {
            o << "-oo";
        } else {
            o << _n;
        }
    }

}; // class bound

inline std::ostream& operator<<(std::ostream& o, const bound_t& b) {
    b.write(o);
    return o;
}

using z_bound = bound_t;

class interval_t final {
  private:
    bound_t _lb;
    bound_t _ub;

  public:
    static interval_t top() { return interval_t(bound_t::minus_infinity(), bound_t::plus_infinity()); }

    static interval_t bottom() { return interval_t(); }

  private:
    interval_t() : _lb(0), _ub(-1) {}

    static number_t abs(number_t x) { return x < 0 ? -x : x; }

    static number_t max(number_t x, number_t y) { return x <= y ? y : x; }

    static number_t min(number_t x, number_t y) { return x < y ? x : y; }

  public:
    interval_t(bound_t lb, bound_t ub) : _lb(lb), _ub(ub) {
        if (lb > ub) {
            _lb = 0;
            _ub = -1;
        }
    }

    interval_t(bound_t b) : _lb(b), _ub(b) {
        if (b.is_infinite()) {
            _lb = 0;
            _ub = -1;
        }
    }

    interval_t(number_t n) : _lb(n), _ub(n) {}

    interval_t(const interval_t& i) : _lb(i._lb), _ub(i._ub) {}

    interval_t& operator=(interval_t i) {
        _lb = i._lb;
        _ub = i._ub;
        return *this;
    }

    bound_t lb() const { return _lb; }

    bound_t ub() const { return _ub; }

    bool is_bottom() const { return (_lb > _ub); }

    bool is_top() const { return (_lb.is_infinite() && _ub.is_infinite()); }

    interval_t lower_half_line() const { return interval_t(bound_t::minus_infinity(), _ub); }

    interval_t upper_half_line() const { return interval_t(_lb, bound_t::plus_infinity()); }

    bool operator==(interval_t x) const {
        if (is_bottom()) {
            return x.is_bottom();
        } else {
            return (_lb == x._lb) && (_ub == x._ub);
        }
    }

    bool operator!=(interval_t x) const { return !operator==(x); }

    bool operator<=(interval_t x) const {
        if (is_bottom()) {
            return true;
        } else if (x.is_bottom()) {
            return false;
        } else {
            return (x._lb <= _lb) && (_ub <= x._ub);
        }
    }

    interval_t operator|(interval_t x) const {
        if (is_bottom()) {
            return x;
        } else if (x.is_bottom()) {
            return *this;
        } else {
            return interval_t(bound_t::min(_lb, x._lb), bound_t::max(_ub, x._ub));
        }
    }

    interval_t operator&(interval_t x) const {
        if (is_bottom() || x.is_bottom()) {
            return bottom();
        } else {
            return interval_t(bound_t::max(_lb, x._lb), bound_t::min(_ub, x._ub));
        }
    }

    interval_t widen(interval_t x) const {
        if (is_bottom()) {
            return x;
        } else if (x.is_bottom()) {
            return *this;
        } else {
            return interval_t(x._lb < _lb ? bound_t::minus_infinity() : _lb,
                              _ub < x._ub ? bound_t::plus_infinity() : _ub);
        }
    }

    template <typename Thresholds>
    interval_t widening_thresholds(interval_t x, const Thresholds& ts) {
        if (is_bottom()) {
            return x;
        } else if (x.is_bottom()) {
            return *this;
        } else {
            bound_t lb = (x._lb < _lb ? ts.get_prev(x._lb) : _lb);
            bound_t ub = (_ub < x._ub ? ts.get_next(x._ub) : _ub);
            return interval_t(lb, ub);
        }
    }

    interval_t narrow(interval_t x) const {
        if (is_bottom() || x.is_bottom()) {
            return bottom();
        } else {
            return interval_t(_lb.is_infinite() && x._lb.is_finite() ? x._lb : _lb,
                              _ub.is_infinite() && x._ub.is_finite() ? x._ub : _ub);
        }
    }

    interval_t operator+(interval_t x) const {
        if (is_bottom() || x.is_bottom()) {
            return bottom();
        } else {
            return interval_t(_lb + x._lb, _ub + x._ub);
        }
    }

    interval_t& operator+=(interval_t x) { return operator=(operator+(x)); }

    interval_t operator-() const {
        if (is_bottom()) {
            return bottom();
        } else {
            return interval_t(-_ub, -_lb);
        }
    }

    interval_t operator-(interval_t x) const {
        if (is_bottom() || x.is_bottom()) {
            return bottom();
        } else {
            return interval_t(_lb - x._ub, _ub - x._lb);
        }
    }

    interval_t& operator-=(interval_t x) { return operator=(operator-(x)); }

    interval_t operator*(interval_t x) const {
        if (is_bottom() || x.is_bottom()) {
            return bottom();
        } else {
            bound_t ll = _lb * x._lb;
            bound_t lu = _lb * x._ub;
            bound_t ul = _ub * x._lb;
            bound_t uu = _ub * x._ub;
            return interval_t(bound_t::min(ll, lu, ul, uu), bound_t::max(ll, lu, ul, uu));
        }
    }

    interval_t& operator*=(interval_t x) { return operator=(operator*(x)); }

    interval_t operator/(interval_t x) const;

    interval_t& operator/=(interval_t x) { return operator=(operator/(x)); }

    std::optional<number_t> singleton() const {
        if (!is_bottom() && _lb == _ub) {
            return _lb.number();
        } else {
            return std::optional<number_t>();
        }
    }

    bool operator[](number_t n) const {
        if (is_bottom()) {
            return false;
        } else {
            bound_t b(n);
            return (_lb <= b) && (b <= _ub);
        }
    }

    void write(std::ostream& o) const {
        if (is_bottom()) {
            o << "_|_";
        } else {
            o << "[" << _lb << ", " << _ub << "]";
        }
    }

    // division and remainder operations

    interval_t UDiv(interval_t x) const {
        if (is_bottom() || x.is_bottom()) {
            return bottom();
        } else {
            return top();
        }
    }

    interval_t SRem(interval_t x) const;

    interval_t URem(interval_t x) const;

    // bitwise operations
    interval_t And(interval_t x) const;

    interval_t Or(interval_t x) const;

    interval_t Xor(interval_t x) const;

    interval_t Shl(interval_t x) const;

    interval_t LShr(interval_t x) const;

    interval_t AShr(interval_t x) const;

}; //  class interval

inline interval_t operator+(number_t c, interval_t x) { return interval_t(c) + x; }

inline interval_t operator+(interval_t x, number_t c) { return x + interval_t(c); }

inline interval_t operator*(number_t c, interval_t x) { return interval_t(c) * x; }

inline interval_t operator*(interval_t x, number_t c) { return x * interval_t(c); }

inline interval_t operator/(number_t c, interval_t x) { return interval_t(c) / x; }

inline interval_t operator/(interval_t x, number_t c) { return x / interval_t(c); }

inline interval_t operator-(number_t c, interval_t x) { return interval_t(c) - x; }

inline interval_t operator-(interval_t x, number_t c) { return x - interval_t(c); }

inline std::ostream& operator<<(std::ostream& o, const interval_t& i) {
    i.write(o);
    return o;
}

inline interval_t trim_interval(interval_t i, interval_t j) {
    if (std::optional<z_number> c = j.singleton()) {
        if (i.lb() == *c) {
            return interval_t(*c + 1, i.ub());
        } else if (i.ub() == *c) {
            return interval_t(i.lb(), *c - 1);
        } else {
        }
    }
    return i;
}

} // namespace crab
