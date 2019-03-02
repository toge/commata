/**
 * These codes are licensed under the Unlicense.
 * http://unlicense.org
 */

#ifndef COMMATA_GUARD_815C71AA_E8EC_4B62_98C0_03D99230C7BB
#define COMMATA_GUARD_815C71AA_E8EC_4B62_98C0_03D99230C7BB

#include <type_traits>
#include <utility>

namespace commata {
namespace detail {

template <class F, class = void>
class member_like_base;

template <class F>
class member_like_base<F,
    std::enable_if_t<!std::is_reference<F>::value
                  && std::is_final<F>::value>>
{
    F f_;

public:
    member_like_base()
        noexcept(std::is_nothrow_default_constructible<F>::value)
    {}

    explicit member_like_base(const F& f)
        noexcept(std::is_nothrow_copy_constructible<F>::value) :
        f_(f)
    {}

    explicit member_like_base(F&& f)
        noexcept(std::is_nothrow_move_constructible<F>::value) :
        f_(std::move(f))
    {}

    F& get() noexcept
    {
        return f_;
    }

    const F& get() const noexcept
    {
        return f_;
    }
};

template <class F>
class member_like_base<F,
    std::enable_if_t<!std::is_reference<F>::value
                  && !std::is_final<F>::value>> :
    F
{
public:
    member_like_base()
        noexcept(std::is_nothrow_default_constructible<F>::value)
    {}

    explicit member_like_base(const F& f)
        noexcept(std::is_nothrow_copy_constructible<F>::value) :
        F(f)
    {}

    explicit member_like_base(F&& f)
        noexcept(std::is_nothrow_move_constructible<F>::value) :
        F(std::move(f))
    {}

    F& get() noexcept
    {
        return *this;
    }

    const F& get() const noexcept
    {
        return *this;
    }
};

template <class B, class M>
class base_member_pair
{
    struct pair : member_like_base<B>
    {
        M m;

        pair(B base, M member) :
            member_like_base<B>(std::move(base)), m(std::move(member))
        {}
    };

    pair p_;

public:
    base_member_pair(B first, M second) :
        p_(std::move(first), std::move(second))
    {}

    B& base() noexcept
    {
        return p_.get();
    }

    const B& base() const noexcept
    {
        return p_.get();
    }

    M& member() noexcept
    {
        return p_.m;
    }

    const M& member() const noexcept
    {
        return p_.m;
    }
};

}}

#endif
