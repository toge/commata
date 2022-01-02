/**
 * These codes are licensed under the Unlicense.
 * http://unlicense.org
 */

#ifndef COMMATA_GUARD_555AFEA0_404A_4BF3_AF38_4F653A0B76E6
#define COMMATA_GUARD_555AFEA0_404A_4BF3_AF38_4F653A0B76E6

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <clocale>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <iterator>
#include <limits>
#include <locale>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <typeinfo>
#include <type_traits>
#include <utility>
#include <vector>

#include "allocation_only_allocator.hpp"
#include "buffer_size.hpp"
#include "nothrow_move_constructible.hpp"
#include "text_error.hpp"
#include "member_like_base.hpp"
#include "typing_aid.hpp"
#include "write_ntmbs.hpp"

namespace commata {

namespace detail { namespace scanner {

template <class T, class Ch>
using accepts_range = is_callable<T&, Ch*, Ch*>;

template <class T, class X>
using accepts_x = is_callable<T&, X>;

struct typable
{
    virtual ~typable() {}
    virtual const std::type_info& get_type() const = 0;

    template <class T>
    const T* get_target() const noexcept
    {
        return (get_type() == typeid(T)) ?
            static_cast<const T*>(get_target_v()) : nullptr;
    }

    template <class T>
    T* get_target() noexcept
    {
        return (get_type() == typeid(T)) ?
            static_cast<T*>(get_target_v()) : nullptr;
    }

private:
    virtual const void* get_target_v() const = 0;
    virtual void* get_target_v() = 0;
};

}} // end detail::scanner

template <class Ch, class Tr = std::char_traits<Ch>,
          class Allocator = std::allocator<Ch>>
class basic_table_scanner
{
    using string_t = std::basic_string<Ch, Tr, Allocator>;

    struct field_scanner
    {
        virtual ~field_scanner() {}
        virtual void field_value(
            Ch* begin, Ch* end, basic_table_scanner& me) = 0;
        virtual void field_value(
            string_t&& value, basic_table_scanner& me) = 0;
    };

    struct header_field_scanner : field_scanner
    {
        virtual void so_much_for_header(basic_table_scanner& me) = 0;
    };

    template <class HeaderScanner>
    class typed_header_field_scanner :
        public header_field_scanner, detail::member_like_base<HeaderScanner>
    {
        using range_t = std::pair<Ch*, Ch*>;

    public:
        template <class T>
        explicit typed_header_field_scanner(T&& s) :
            detail::member_like_base<HeaderScanner>(std::forward<T>(s))
        {}

        typed_header_field_scanner(typed_header_field_scanner&&) = delete;

        void field_value(Ch* begin, Ch* end, basic_table_scanner& me) override
        {
            const range_t range(begin, end);
            if (!scanner()(me.j_, std::addressof(range), me)) {
                me.remove_header_field_scanner();
            }
        }

        void field_value(string_t&& value, basic_table_scanner& me) override
        {
            field_value(&value[0], &value[0] + value.size(), me);
        }

        void so_much_for_header(basic_table_scanner& me) override
        {
            if (!scanner()(me.j_, static_cast<const range_t*>(nullptr), me)) {
                me.remove_header_field_scanner();
            }
        }

    private:
        HeaderScanner& scanner()
        {
            return this->get();
        }
    };

    struct body_field_scanner : field_scanner, detail::scanner::typable
    {
        virtual void field_skipped() = 0;
    };

    template <class FieldScanner>
    struct typed_body_field_scanner :
        body_field_scanner, private detail::member_like_base<FieldScanner>
    {
        template <class T>
        explicit typed_body_field_scanner(T&& s) :
            detail::member_like_base<FieldScanner>(std::forward<T>(s))
        {}

        typed_body_field_scanner(typed_body_field_scanner&&) = delete;

        void field_value(Ch* begin, Ch* end, basic_table_scanner& me) override
        {
            field_value_r(
                typename detail::scanner::accepts_range<
                    std::remove_reference_t<decltype(scanner())>, Ch>(),
                begin, end, me);
        }

        void field_value(string_t&& value, basic_table_scanner& me) override
        {
            field_value_s(
                typename detail::scanner::accepts_x<
                    std::remove_reference_t<decltype(scanner())>, string_t>(),
                std::move(value), me);
        }

        void field_skipped() override
        {
            scanner()();
        }

        const std::type_info& get_type() const noexcept override
        {
            // Static types suffice for we do slicing on construction
            return typeid(FieldScanner);
        }

    private:
        void field_value_r(std::true_type,
            Ch* begin, Ch* end, basic_table_scanner&)
        {
            scanner()(begin, end);
        }

        void field_value_r(std::false_type,
            Ch* begin, Ch* end, basic_table_scanner& me)
        {
            scanner()(string_t(begin, end, me.get_allocator()));
        }

        void field_value_s(std::true_type,
            string_t&& value, basic_table_scanner&)
        {
            scanner()(std::move(value));
        }

        void field_value_s(std::false_type,
            string_t&& value, basic_table_scanner&)
        {
            scanner()(&*value.begin(), &*value.begin() + value.size());
        }

        const void* get_target_v() const noexcept override
        {
            return std::addressof(this->get());
        }

        void* get_target_v() noexcept override
        {
            return std::addressof(this->get());
        }

        decltype(auto) scanner() noexcept
        {
            return this->get();
        }
    };

    struct record_end_scanner : detail::scanner::typable
    {
        virtual bool end_record(basic_table_scanner& me) = 0;
    };

    template <class T>
    struct typed_record_end_scanner :
        record_end_scanner, private detail::member_like_base<T>
    {
        template <class U>
        explicit typed_record_end_scanner(U&& scanner) :
            detail::member_like_base<T>(std::forward<U>(scanner))
        {}

        typed_record_end_scanner(typed_record_end_scanner&&) = delete;

        bool end_record(basic_table_scanner& me) override
        {
            return do_end_record(this->get(), me);
        }

        const std::type_info& get_type() const noexcept override
        {
            return typeid(T);
        }

    private:
        const void* get_target_v() const noexcept override
        {
            return std::addressof(this->get());
        }

        void* get_target_v() noexcept override
        {
            return std::addressof(this->get());
        }

        template <class U>
        static auto do_end_record(U& u, basic_table_scanner& me)
         -> std::enable_if_t<
                detail::is_callable<U&, basic_table_scanner&>::value, bool>
        {
            return do_end_record_no_arg(std::bind(std::ref(u), std::ref(me)));
        }

        template <class U>
        static auto do_end_record(U& u, basic_table_scanner&)
         -> std::enable_if_t<
                !detail::is_callable<U&, basic_table_scanner&>::value
             && detail::is_callable<U&>::value,
                bool>
        {
            return do_end_record_no_arg(u);
        }

        template <class F>
        static auto do_end_record_no_arg(F&& f)
         -> std::enable_if_t<
                std::is_void<decltype(std::forward<F>(f)())>::value, bool>
        {
            std::forward<F>(f)();
            return true;
        }

        template <class F>
        static auto do_end_record_no_arg(F&& f)
         -> std::enable_if_t<
                !std::is_void<decltype(std::forward<F>(f)())>::value, bool>
        {
            return std::forward<F>(f)();
        }
    };

    using at_t = std::allocator_traits<Allocator>;
    using hfs_at_t =
        typename at_t::template rebind_traits<header_field_scanner>;
    using bfs_at_t =
        typename at_t::template rebind_traits<body_field_scanner>;
    using bfs_ptr_t = typename bfs_at_t::pointer;
    using bfs_ptr_p_t = typename std::pair<bfs_ptr_t, std::size_t>;
    using bfs_ptr_p_a_t = typename at_t::template rebind_alloc<bfs_ptr_p_t>;
    using scanners_a_t = detail::allocation_only_allocator<bfs_ptr_p_a_t>;
    using res_at_t = typename at_t::template rebind_traits<record_end_scanner>;

    std::size_t remaining_header_records_;
    std::size_t j_;
    std::size_t buffer_size_;
    typename at_t::pointer buffer_;
    const Ch* begin_;
    const Ch* end_;
    string_t fragmented_value_;
    typename hfs_at_t::pointer header_field_scanner_;
    detail::nothrow_move_constructible<
        std::vector<bfs_ptr_p_t, scanners_a_t>> scanners_;
    typename std::vector<bfs_ptr_p_t, scanners_a_t>::size_type sj_;
        // possibly active scanner: can't be an iterator
        // because it would be invalidated by header scanners
    typename res_at_t::pointer end_scanner_;

public:
    using char_type = Ch;
    using traits_type = Tr;
    using allocator_type = Allocator;
    using size_type = typename std::allocator_traits<Allocator>::size_type;

    explicit basic_table_scanner(
        std::size_t header_record_count = 0U,
        size_type buffer_size = 0U) :
        basic_table_scanner(std::allocator_arg, Allocator(),
            header_record_count, buffer_size)
    {}

    template <class HeaderFieldScanner,
        std::enable_if_t<!std::is_integral<HeaderFieldScanner>::value>*
            = nullptr>
    explicit basic_table_scanner(
        HeaderFieldScanner s,
        size_type buffer_size = 0U) :
        basic_table_scanner(
            std::allocator_arg, Allocator(), std::move(s), buffer_size)
    {}

    basic_table_scanner(
        std::allocator_arg_t, const Allocator& alloc,
        std::size_t header_record_count = 0U,
        size_type buffer_size = 0U) :
        remaining_header_records_(header_record_count),
        buffer_size_(sanitize_buffer_size(buffer_size)), buffer_(),
        begin_(nullptr), fragmented_value_(alloc),
        header_field_scanner_(), scanners_(make_scanners()),
        end_scanner_(nullptr)
    {}

    template <class HeaderFieldScanner,
        std::enable_if_t<
            !std::is_integral<std::decay_t<HeaderFieldScanner>>::value>*
                = nullptr>
    basic_table_scanner(
        std::allocator_arg_t, const Allocator& alloc,
        HeaderFieldScanner&& s,
        size_type buffer_size = 0U) :
        remaining_header_records_(0U),
        buffer_size_(sanitize_buffer_size(buffer_size)),
        buffer_(), begin_(nullptr), fragmented_value_(alloc),
        header_field_scanner_(allocate_construct<
            typed_header_field_scanner<std::decay_t<HeaderFieldScanner>>>(
                std::forward<HeaderFieldScanner>(s))),
        scanners_(make_scanners()), end_scanner_(nullptr)
    {}

    basic_table_scanner(basic_table_scanner&& other) noexcept :
        remaining_header_records_(other.remaining_header_records_),
        buffer_size_(other.buffer_size_),
        buffer_(std::exchange(other.buffer_, nullptr)),
        begin_(other.begin_), end_(other.end_),
        fragmented_value_(std::move(other.fragmented_value_)),
        header_field_scanner_(std::exchange(other.header_field_scanner_,
                                            nullptr)),
        scanners_(std::move(other.scanners_)),
        end_scanner_(std::exchange(other.end_scanner_, nullptr))
    {}

    ~basic_table_scanner()
    {
        auto a = get_allocator();
        if (buffer_) {
            at_t::deallocate(a, buffer_, buffer_size_);
        }
        if (header_field_scanner_) {
            destroy_deallocate(header_field_scanner_);
        }
        if (scanners_) {
            for (const auto& p : *scanners_) {
                destroy_deallocate(p.first);
            }
            scanners_.kill(a);
        }
        if (end_scanner_) {
            destroy_deallocate(end_scanner_);
        }
    }

    allocator_type get_allocator() const noexcept
    {
        return fragmented_value_.get_allocator();
    }

    template <class FieldScanner = std::nullptr_t>
    void set_field_scanner(std::size_t j, FieldScanner&& s = FieldScanner())
    {
        do_set_field_scanner(j, std::forward<FieldScanner>(s));
    }

private:
    struct scanner_less
    {
        bool operator()(
            const bfs_ptr_p_t& left, std::size_t right) const noexcept
        {
            return left.second < right;
        }

        bool operator()(
            std::size_t left, const bfs_ptr_p_t& right) const noexcept
        {
            return left < right.second;
        }
    };

    template <class FieldScanner>
    auto do_set_field_scanner(std::size_t j, FieldScanner&& s)
     -> std::enable_if_t<
            !std::is_base_of<
                std::nullptr_t,
                std::decay_t<FieldScanner>>::value>
    {
        using scanner_t = typed_body_field_scanner<std::decay_t<FieldScanner>>;
        if (!scanners_) {
            scanners_.assign(get_allocator(), make_scanners());     // throw
        }
        const auto it = std::lower_bound(
            scanners_->begin(), scanners_->end(), j, scanner_less());
        const auto p = allocate_construct<scanner_t>(
                                    std::forward<FieldScanner>(s)); // throw
        if ((it != scanners_->end()) && (it->second == j)) {
            destroy_deallocate(it->first);
            it->first = p;
        } else {
            try {
                scanners_->emplace(it, p, j);                       // throw
            } catch (...) {
                destroy_deallocate(p);
                throw;
            }
        }
    }

    void do_set_field_scanner(std::size_t j, std::nullptr_t)
    {
        if (scanners_) {
            const auto it = std::lower_bound(
                scanners_->begin(), scanners_->end(), j, scanner_less());
            if ((it != scanners_->end()) && (it->second == j)) {
                destroy_deallocate(it->first);
                scanners_->erase(it);
            }
        }
    }

public:
    const std::type_info& get_field_scanner_type(std::size_t j) const noexcept
    {
        if (scanners_) {
            const auto it = std::lower_bound(
                scanners_->cbegin(), scanners_->cend(), j, scanner_less());
            if ((it != scanners_->cend()) && (it->second == j)) {
                return it->first->get_type();
            }
        }
        return typeid(void);
    }

    bool has_field_scanner(std::size_t j) const noexcept
    {
        if (scanners_) {
            return std::binary_search(
                scanners_->cbegin(), scanners_->cend(), j, scanner_less());
        }
        return false;
    }

    template <class FieldScanner>
    const FieldScanner* get_field_scanner(std::size_t j) const noexcept
    {
        return get_field_scanner_g<FieldScanner>(*this, j);
    }

    template <class FieldScanner>
    FieldScanner* get_field_scanner(std::size_t j) noexcept
    {
        return get_field_scanner_g<FieldScanner>(*this, j);
    }

private:
    template <class FieldScanner, class ThisType>
    static std::conditional_t<
        std::is_const<std::remove_reference_t<ThisType>>::value,
        const FieldScanner, FieldScanner>*
    get_field_scanner_g(ThisType& me, std::size_t j) noexcept
    {
        if (me.scanners_) {
            const auto it = std::lower_bound(me.scanners_->begin(),
                me.scanners_->end(), j, scanner_less());
            if ((it != me.scanners_->end()) && (it->second == j)) {
                return it->first->template get_target<FieldScanner>();
            }
        }
        return nullptr;
    }

public:
    template <class RecordEndScanner = std::nullptr_t>
    void set_record_end_scanner(RecordEndScanner&& s = RecordEndScanner())
    {
        do_set_record_end_scanner(std::forward<RecordEndScanner>(s));
    }

private:
    template <class RecordEndScanner>
    auto do_set_record_end_scanner(RecordEndScanner&& s)
     -> std::enable_if_t<
            !std::is_base_of<
                std::nullptr_t,
                std::decay_t<RecordEndScanner>>::value>
    {
        using scanner_t = typed_record_end_scanner<
            std::decay_t<RecordEndScanner>>;
        const auto p = allocate_construct<scanner_t>(
            std::forward<RecordEndScanner>(s)); // throw
        if (end_scanner_) {
            destroy_deallocate(end_scanner_);
        }
        end_scanner_ = p;
    }

    void do_set_record_end_scanner(std::nullptr_t)
    {
        if (end_scanner_) {
            destroy_deallocate(end_scanner_);
            end_scanner_ = nullptr;
        }
    }

public:
    const std::type_info& get_record_end_scanner_type() const noexcept
    {
        if (end_scanner_) {
            return end_scanner_->get_type();
        } else {
            return typeid(void);
        }
    }

    bool has_record_end_scanner() const noexcept
    {
        return end_scanner_ != nullptr;
    }

    template <class RecordEndScanner>
    const RecordEndScanner* get_record_end_scanner() const noexcept
    {
        return get_record_end_scanner_g<RecordEndScanner>(*this);
    }

    template <class RecordEndScanner>
    RecordEndScanner* get_record_end_scanner() noexcept
    {
        return get_record_end_scanner_g<RecordEndScanner>(*this);
    }

private:
    template <class RecordEndScanner, class ThisType>
    static auto get_record_end_scanner_g(ThisType& me)
    {
        return me.has_record_end_scanner() ?
            me.end_scanner_->template get_target<RecordEndScanner>() :
            nullptr;
    }

public:
    std::pair<Ch*, std::size_t> get_buffer()
    {
        if (!buffer_) {
            auto a = get_allocator();
            buffer_ = at_t::allocate(a, buffer_size_);  // throw
        } else if (begin_) {
            fragmented_value_.assign(begin_, end_);     // throw
            begin_ = nullptr;
        }
        return { true_buffer(), static_cast<std::size_t>(buffer_size_ - 1) };
        // We'd like to push buffer_[buffer_size_] with '\0' on EOF
        // so we tell the driver that the buffer size is smaller by one
    }

    void release_buffer(const Ch*)
    {}

    void start_record(const Ch* /*record_begin*/)
    {
        sj_ = 0;
        j_ = 0;
    }

    void update(const Ch* first, const Ch* last)
    {
        if (get_scanner() && (first != last)) {
            if (begin_) {
                assert(fragmented_value_.empty());
                fragmented_value_.
                    reserve((end_ - begin_) + (last - first));  // throw
                fragmented_value_.assign(begin_, end_);
                fragmented_value_.append(first, last);
                begin_ = nullptr;
            } else if (!fragmented_value_.empty()) {
                fragmented_value_.append(first, last);          // throw
            } else {
                begin_ = first;
                end_ = last;
            }
        }
    }

    void finalize(const Ch* first, const Ch* last)
    {
        if (const auto scanner = get_scanner()) {
            if (begin_) {
                if (first != last) {
                    fragmented_value_.
                        reserve((end_ - begin_) + (last - first));  // throw
                    fragmented_value_.assign(begin_, end_);
                    fragmented_value_.append(first, last);
                    scanner->field_value(std::move(fragmented_value_), *this);
                    fragmented_value_.clear();
                } else {
                    *uc(end_) = Ch();
                    scanner->field_value(uc(begin_), uc(end_), *this);
                }
                begin_ = nullptr;
            } else if (!fragmented_value_.empty()) {
                fragmented_value_.append(first, last);              // throw
                scanner->field_value(std::move(fragmented_value_), *this);
                fragmented_value_.clear();
            } else {
                *uc(last) = Ch();
                scanner->field_value(uc(first), uc(last), *this);
            }
        }
        if (scanners_ && (sj_ < scanners_->size())
         && (j_ == (*scanners_)[sj_].second)) {
            ++sj_;
        }
        ++j_;
    }

    bool end_record(const Ch* /*record_end*/)
    {
        if (header_field_scanner_) {
            header_field_scanner_->so_much_for_header(*this);
        } else if (remaining_header_records_ > 0) {
            --remaining_header_records_;
        } else {
            if (scanners_) {
                for (auto i = sj_, ie = scanners_->size(); i != ie; ++i) {
                    (*scanners_)[i].first->field_skipped();
                }
            }
            if (end_scanner_) {
                return end_scanner_->end_record(*this);
            }
        }
        return true;
    }

    bool is_in_header() const noexcept
    {
        return header_field_scanner_ || (remaining_header_records_ > 0);
    }

private:
    std::size_t sanitize_buffer_size(std::size_t buffer_size) noexcept
    {
        return std::max(
            static_cast<std::size_t>(2U),
            detail::sanitize_buffer_size(buffer_size, get_allocator()));
    }

    template <class T, class... Args>
    auto allocate_construct(Args&&... args)
    {
        using t_alloc_traits_t =
            typename at_t::template rebind_traits<T>;
        typename t_alloc_traits_t::allocator_type a(get_allocator());
        const auto p = t_alloc_traits_t::allocate(a, 1);    // throw
        try {
            ::new(std::addressof(*p))
                T(std::forward<Args>(args)...);             // throw
        } catch (...) {
            a.deallocate(p, 1);
            throw;
        }
        return p;
    }

    template <class P>
    void destroy_deallocate(P p)
    {
        assert(p);
        using v_t = typename std::pointer_traits<P>::element_type;
        p->~v_t();
        using t_at_t = typename at_t::template rebind_traits<v_t>;
        typename t_at_t::allocator_type a(get_allocator());
        t_at_t::deallocate(a, p, 1);
    }

    auto make_scanners()
    {
        const auto a = get_allocator();
        return decltype(scanners_)(std::allocator_arg, a,
            scanners_a_t(bfs_ptr_p_a_t(a)));
    }

    Ch* true_buffer() const noexcept
    {
        assert(buffer_);
        return std::addressof(*buffer_);
    }

    field_scanner* get_scanner()
    {
        if (header_field_scanner_) {
            return std::addressof(*header_field_scanner_);
        } else if ((remaining_header_records_ == 0U) && scanners_
                && (sj_ < scanners_->size())
                && (j_ == (*scanners_)[sj_].second)) {
            return std::addressof(*(*scanners_)[sj_].first);
        } else {
            return nullptr;
        }
    }

    Ch* uc(const Ch* s) const
    {
        const auto tb = true_buffer();
        return tb + (s - tb);
    }

    void remove_header_field_scanner()
    {
        destroy_deallocate(header_field_scanner_);
        header_field_scanner_ = nullptr;
    }
};

using table_scanner = basic_table_scanner<char>;
using wtable_scanner = basic_table_scanner<wchar_t>;

class field_translation_error : public text_error
{
public:
    using text_error::text_error;
};

class field_not_found : public field_translation_error
{
public:
    using field_translation_error::field_translation_error;
};

class field_invalid_format : public field_translation_error
{
public:
    using field_translation_error::field_translation_error;
};

class field_empty : public field_invalid_format
{
public:
    using field_invalid_format::field_invalid_format;
};

class field_out_of_range : public field_translation_error
{
public:
    using field_translation_error::field_translation_error;
};

struct replacement_ignore_t {};
constexpr replacement_ignore_t replacement_ignore = replacement_ignore_t{};

struct replacement_fail_t {};
constexpr replacement_fail_t replacement_fail = replacement_fail_t{};

struct invalid_format_t {};
struct out_of_range_t {};
struct empty_t {};

template <class T>
class replacement;

namespace detail { namespace scanner {

template <class X>
struct is_replacement : std::false_type
{};

template <class T>
struct is_replacement<replacement<T>> : std::true_type
{};

}} // end detail::scanner

// In C++17, we can abolish this template and use std::optional instead
template <class T>
class replacement
{
    alignas(T) char store_[sizeof(T)];
    bool has_;

    template <class U>
    friend class replacement;

public:
    using value_type = T;

    replacement() noexcept :
        has_(false)
    {}

    replacement(replacement_ignore_t) noexcept :
        has_(false)
    {}

    template <class U,
        std::enable_if_t<
            !detail::scanner::is_replacement<std::decay_t<U>>::value
         && !std::is_same<replacement_ignore_t, std::decay_t<U>>::value>*
            = nullptr>
    explicit replacement(U&& t)
        noexcept(std::is_nothrow_constructible<T, U>::value) :
        has_(true)
    {
        ::new(store_) T(std::forward<U>(t));        // throw
    }

    replacement(const replacement& other)
        noexcept(std::is_nothrow_copy_constructible<T>::value) :
        replacement(privy_t(), other)
    {}

    replacement(replacement&& other)
        noexcept(std::is_nothrow_move_constructible<T>::value) :
        replacement(privy_t(), std::move(other))
    {}

    template <class U>
    replacement(const replacement<U>& other)
        noexcept(std::is_nothrow_constructible<T, const U&>::value) :
        replacement(privy_t(), other)
    {}

    template <class U>
    replacement(replacement<U>&& other)
        noexcept(std::is_nothrow_constructible<T, U>::value) :
        replacement(privy_t(), std::move(other))
    {}

private:
    struct privy_t {};

    template <class U>
    replacement(privy_t, const replacement<U>& other)
        noexcept(std::is_nothrow_constructible<T, const U&>::value) :
        has_(other.has_)
    {
        if (const auto p = other.get()) {
            ::new(&store_) T(*p);                   // throw
        }
    }

    template <class U>
    replacement(privy_t, replacement<U>&& other)
        noexcept(std::is_nothrow_constructible<T, U>::value) :
        has_(other.has_)
    {
        if (const auto p = other.get()) {
            ::new(&store_) T(std::move(*p));        // throw
            p->~U();
            other.has_ = false;
        }
    }

public:
    ~replacement()
    {
        destroy();
    }

    replacement& operator=(replacement_ignore_t) noexcept
    {
        destroy();
        has_ = false;
        return *this;
    }

    template <class U>
    auto operator=(U&& t)
        noexcept(std::is_nothrow_constructible<T, U>::value
              && std::is_nothrow_assignable<T, U>::value)
      -> std::enable_if_t<
            !detail::scanner::is_replacement<std::decay_t<U>>::value
         && !std::is_same<replacement_ignore_t, std::decay_t<U>>::value,
            replacement&>
    {
        if (has_) {
            **this = std::forward<U>(t);            // throw
        } else {
            ::new(&store_) T(std::forward<U>(t));   // throw
            has_ = true;
        }
        return *this;
    }

    replacement& operator=(const replacement& other)
        noexcept(std::is_nothrow_copy_constructible<T>::value
              && std::is_nothrow_copy_assignable<T>::value)
    {
        assign(other);
        return *this;
    }

    replacement& operator=(replacement&& other)
        noexcept(std::is_nothrow_move_constructible<T>::value
              && std::is_nothrow_move_assignable<T>::value)
    {
        if (this != std::addressof(other)) {
            // This branching is essential
            assign(std::move(other));
        }
        return *this;
    }

    template <class U>
    replacement& operator=(const replacement<U>& other)
        noexcept(std::is_nothrow_constructible<T, const U&>::value
              && std::is_nothrow_assignable<T, const U&>::value)
    {
        assign(other);
        return *this;
    }

    template <class U>
    replacement& operator=(replacement<U>&& other)
        noexcept(std::is_nothrow_constructible<T, U>::value
              && std::is_nothrow_assignable<T, U>::value)
    {
        assign(std::move(other));
        return *this;
    }

private:
    template <class U>
    void assign(const replacement<U>& other)
        noexcept(std::is_nothrow_constructible<T, const U&>::value
              && std::is_nothrow_assignable<T, const U&>::value)
    {
        if (const auto p = other.get()) {
            *this = *other;
        } else {
            *this = replacement_ignore;
        }
    }

    template <class U>
    void assign(replacement<U>&& other)
        noexcept(std::is_nothrow_constructible<T, U>::value
              && std::is_nothrow_assignable<T, U>::value)
    {
        if (const auto p = other.get()) {
            *this = std::move(*other);
            other = replacement_ignore;
                // Requires: *this and other are different objects
        } else {
            *this = replacement_ignore;
        }
    }

public:
    explicit operator bool() const noexcept
    {
        return has_;
    }

    const T* get() const noexcept
    {
        return has_ ? operator->() : nullptr;
    }

    T* get() noexcept
    {
        return has_ ? operator->() : nullptr;
    }

    const T& operator*() const
    {
        return *operator->();
    }

    T& operator*()
    {
        return *operator->();
    }

    const T* operator->() const
    {
        assert(has_);
        return static_cast<const T*>(static_cast<const void*>(store_));
    }

    T* operator->()
    {
        assert(has_);
        return static_cast<T*>(static_cast<void*>(store_));
    }

    void swap(replacement& other)
        noexcept(detail::is_nothrow_swappable<T>()
              && std::is_nothrow_constructible<T>::value)
    {
        if (const auto p = other.get()) {
            if (has_) {
                using std::swap;
                swap(**this, *p);                   // throw
                swap(has_, other.has_);
            } else {
                *this = std::move(other);           // throw
                    // No assignment of T will occur
            }
        } else {
            other = std::move(*this);               // throw
                    // ditto
        }
    }

private:
    void destroy() noexcept
    {
        if (has_) {
            static_cast<const T*>(static_cast<const void*>(store_))->~T();
        }
    }
};

template <class T>
void swap(replacement<T>& left, replacement<T>& right)
    noexcept(noexcept(left.swap(right)))
{
    left.swap(right);
}

namespace detail { namespace scanner {

template <class T>
struct numeric_type_traits;

template <>
struct numeric_type_traits<char>
{
    static constexpr const char* name = "char";
    using raw_type =
        std::conditional_t<std::is_signed<char>::value, long, unsigned long>;
};

template <>
struct numeric_type_traits<signed char>
{
    static constexpr const char* name = "signed char";
    using raw_type = long;
};

template <>
struct numeric_type_traits<unsigned char>
{
    static constexpr const char* name = "unsigned char";
    using raw_type = unsigned long;
};

template <>
struct numeric_type_traits<short>
{
    static constexpr const char* name = "short int";
    using raw_type = long;
};

template <>
struct numeric_type_traits<unsigned short>
{
    static constexpr const char* name = "unsigned short int";
    using raw_type = unsigned long;
};

template <>
struct numeric_type_traits<int>
{
    static constexpr const char* name = "int";
    using raw_type = long;
};

template <>
struct numeric_type_traits<unsigned>
{
    static constexpr const char* name = "unsigned int";
    using raw_type = unsigned long;
};

template <>
struct numeric_type_traits<long>
{
    static constexpr const char* name = "long int";
    static constexpr const auto strto = std::strtol;
    static constexpr const auto wcsto = std::wcstol;
};

template <>
struct numeric_type_traits<unsigned long>
{
    static constexpr const char* name = "unsigned long int";
    static constexpr const auto strto = std::strtoul;
    static constexpr const auto wcsto = std::wcstoul;
};

template <>
struct numeric_type_traits<long long>
{
    static constexpr const char* name = "long long int";
    static constexpr const auto strto = std::strtoll;
    static constexpr const auto wcsto = std::wcstoll;
};

template <>
struct numeric_type_traits<unsigned long long>
{
    static constexpr const char* name = "unsigned long long int";
    static constexpr const auto strto = std::strtoull;
    static constexpr const auto wcsto = std::wcstoull;
};

template <>
struct numeric_type_traits<float>
{
    static constexpr const char* name = "float";
    static constexpr const auto strto = std::strtof;
    static constexpr const auto wcsto = std::wcstof;
    static constexpr const float huge = HUGE_VALF;
};

template <>
struct numeric_type_traits<double>
{
    static constexpr const char* name = "double";
    static constexpr const auto strto = std::strtod;
    static constexpr const auto wcsto = std::wcstod;
    static constexpr const double huge = HUGE_VAL;
};

template <>
struct numeric_type_traits<long double>
{
    static constexpr const char* name = "long double";
    static constexpr const auto strto = std::strtold;
    static constexpr const auto wcsto = std::wcstold;
    static constexpr const long double huge = HUGE_VALL;
};

struct is_convertible_numeric_type_impl
{
    template <class T>
    static auto check(T* = nullptr)
     -> decltype(numeric_type_traits<T>::name, std::true_type());

    template <class T>
    static auto check(...) -> std::false_type;
};

template <class T>
struct is_convertible_numeric_type :
    decltype(is_convertible_numeric_type_impl::check<T>(nullptr))
{};

// D must derive from raw_converter_base<D, H> (CRTP)
// and must have member functions "engine" for const char* and const wchar_t*
template <class D, class H>
class raw_converter_base :
    member_like_base<H>
{
public:
    using member_like_base<H>::member_like_base;

    template <class Ch>
    auto convert_raw(const Ch* begin, const Ch* end)
    {
        Ch* middle;
        errno = 0;
        const auto r = static_cast<const D*>(this)->engine(begin, &middle);
        using ret_t = replacement<std::remove_const_t<decltype(r)>>;

        const auto has_postfix =
            std::find_if<const Ch*>(middle, end, [](Ch c) {
                return !is_space(c);
            }) != end;
        if (has_postfix) {
            // if a not-whitespace-extra-character found, it is NG
            return ret_t(this->get().invalid_format(begin, end));
        } else if (begin == middle) {
            // whitespace only
            return ret_t(this->get().empty());
        } else if (errno == ERANGE) {
            return ret_t(this->get().out_of_range(begin, end,
                static_cast<const D*>(this)->erange(r)));
        } else {
            return ret_t(r);
        }
    }

    decltype(auto) get_conversion_error_handler() noexcept
    {
        return this->get().get();
    }

    decltype(auto) get_conversion_error_handler() const noexcept
    {
        return this->get().get();
    }

private:
    // To mimic space skipping by std::strtol and its comrades,
    // we have to refer current C locale
    static bool is_space(char c)
    {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
    }

    // ditto
    static bool is_space(wchar_t c)
    {
        return std::iswspace(c) != 0;
    }
};

template <class...>
using void_t = void;

template <class T, class H, class = void>
struct raw_converter;

// For integral types
template <class T, class H>
struct raw_converter<T, H, std::enable_if_t<std::is_integral<T>::value,
        void_t<decltype(numeric_type_traits<T>::strto)>>> :
    raw_converter_base<raw_converter<T, H>, H>
{
    using raw_converter_base<raw_converter<T, H>, H>
        ::raw_converter_base;
    using raw_converter_base<raw_converter<T, H>, H>
        ::get_conversion_error_handler;

    auto engine(const char* s, char** e) const
    {
        return numeric_type_traits<T>::strto(s, e, 10);
    }

    auto engine(const wchar_t* s, wchar_t** e) const
    {
        return numeric_type_traits<T>::wcsto(s, e, 10);
    }

    int erange(T v) const
    {
        return erange_impl(v, std::is_signed<T>());
    }

private:
    int erange_impl(T v, std::true_type) const
    {
        if (v == std::numeric_limits<T>::max()) {
            return 1;
        } else if (v == std::numeric_limits<T>::min()) {
            return -1;
        } else {
            return 0;
        }
    }

    int erange_impl(T v, std::false_type) const
    {
        if (v == std::numeric_limits<T>::max()) {
            return 1;
        } else {
            return -1;
        }
    }
};

// For floating-point types
template <class T, class H>
struct raw_converter<T, H, std::enable_if_t<std::is_floating_point<T>::value,
        void_t<decltype(numeric_type_traits<T>::strto)>>> :
    raw_converter_base<raw_converter<T, H>, H>
{
    using raw_converter_base<raw_converter<T, H>, H>
        ::raw_converter_base;
    using raw_converter_base<raw_converter<T, H>, H>
        ::get_conversion_error_handler;

    auto engine(const char* s, char** e) const
    {
        return numeric_type_traits<T>::strto(s, e);
    }

    auto engine(const wchar_t* s, wchar_t** e) const
    {
        return numeric_type_traits<T>::wcsto(s, e);
    }

    int erange(T v) const
    {
        if (v == numeric_type_traits<T>::huge) {
            return 1;
        } else if (v == -numeric_type_traits<T>::huge) {
            return -1;
        } else {
            return 0;
        }
    }
};

template <class Ch>
struct has_simple_invalid_format_impl
{
    template <class F>
    static auto check(F*) -> decltype(
        std::declval<F&>()(std::declval<invalid_format_t>(),
            std::declval<const Ch*>(), std::declval<const Ch*>()),
        std::true_type());

    template <class F>
    static auto check(...) -> std::false_type;
};

template <class F, class Ch>
struct has_simple_invalid_format :
    decltype(has_simple_invalid_format_impl<Ch>::template
        check<F>(nullptr))
{};

template <class Ch>
struct has_simple_out_of_range_impl
{
    template <class F>
    static auto check(F*) -> decltype(
        std::declval<F&>()(std::declval<out_of_range_t>(),
            std::declval<const Ch*>(), std::declval<const Ch*>(),
            std::declval<int>()),
        std::true_type());

    template <class F>
    static auto check(...) -> std::false_type;
};

template <class F, class Ch>
struct has_simple_out_of_range :
    decltype(has_simple_out_of_range_impl<Ch>::template
        check<F>(nullptr))
{};

struct has_simple_empty_impl
{
    template <class F>
    static auto check(F*) -> decltype(
        std::declval<F&>()(std::declval<empty_t>()),
        std::true_type());

    template <class F>
    static auto check(...) -> std::false_type;
};

template <class F>
struct has_simple_empty :
    decltype(has_simple_empty_impl::check<F>(nullptr))
{};

template <class T>
struct conversion_error_facade
{
    template <class H, class Ch,
        std::enable_if_t<has_simple_invalid_format<H, Ch>::value>* = nullptr>
    static replacement<T> invalid_format(H& h, const Ch* begin, const Ch* end)
    {
        return h(invalid_format_t(), begin, end);
    }

    template <class H, class Ch,
        std::enable_if_t<!has_simple_invalid_format<H, Ch>::value>* = nullptr>
    static replacement<T> invalid_format(H& h, const Ch* begin, const Ch* end)
    {
        return h(invalid_format_t(), begin, end, static_cast<T*>(nullptr));
    }

    template <class H, class Ch,
        std::enable_if_t<has_simple_out_of_range<H, Ch>::value>* = nullptr>
    static replacement<T> out_of_range(
        H& h, const Ch* begin, const Ch* end, int sign)
    {
        return h(out_of_range_t(), begin, end, sign);
    }

    template <class H, class Ch,
        std::enable_if_t<!has_simple_out_of_range<H, Ch>::value>* = nullptr>
    static replacement<T> out_of_range(
        H& h, const Ch* begin, const Ch* end, int sign)
    {
        return h(out_of_range_t(), begin, end, sign, static_cast<T*>(nullptr));
    }

    template <class H,
        std::enable_if_t<has_simple_empty<H>::value>* = nullptr>
    static replacement<T> empty(H& h)
    {
        return h(empty_t());
    }

    template <class H,
        std::enable_if_t<!has_simple_empty<H>::value>* = nullptr>
    static replacement<T> empty(H& h)
    {
        return h(empty_t(), static_cast<T*>(nullptr));
    }
};

template <class T, class H>
struct typed_conversion_error_handler :
    private member_like_base<H>
{
    using member_like_base<H>::member_like_base;
    using member_like_base<H>::get;

    template <class Ch>
    replacement<T> invalid_format(const Ch* begin, const Ch* end) const
    {
        return conversion_error_facade<T>::
            invalid_format(this->get(), begin, end);
    }

    template <class Ch>
    replacement<T> out_of_range(const Ch* begin, const Ch* end, int sign) const
    {
        return conversion_error_facade<T>::
            out_of_range(this->get(), begin, end, sign);
    }

    replacement<T> empty() const
    {
        return conversion_error_facade<T>::empty(this->get());
    }
};

// For types without corresponding "raw_type"
template <class T, class H, class = void>
struct converter :
    private raw_converter<T, typed_conversion_error_handler<T, H>>
{
    template <class K,
        std::enable_if_t<!std::is_base_of<converter, std::decay_t<K>>::value>*
            = nullptr>
    converter(K&& h) :
        raw_converter<T, typed_conversion_error_handler<T, H>>(
            typed_conversion_error_handler<T, H>(std::forward<K>(h)))
    {}

    using raw_converter<T, typed_conversion_error_handler<T, H>>::
        get_conversion_error_handler;

    template <class Ch>
    auto convert(const Ch* begin, const Ch* end)
    {
        return this->convert_raw(begin, end);
    }
};

template <class T, class H, class U, class = void>
struct restrained_converter :
    private raw_converter<U, H>
{
    static_assert(!std::is_floating_point<T>::value, "");

    using raw_converter<U, H>::raw_converter;
    using raw_converter<U, H>::get_conversion_error_handler;

    template <class Ch>
    replacement<T> convert(const Ch* begin, const Ch* end)
    {
        const auto result = this->convert_raw(begin, end);
        const auto p = result.get();
        if (!p) {
            return replacement<T>();
        }
        const auto r = *p;
        if (r < std::numeric_limits<T>::lowest()) {
            return conversion_error_facade<T>::out_of_range(
                this->get_conversion_error_handler(), begin, end, -1);
        } else if (std::numeric_limits<T>::max() < r) {
            return conversion_error_facade<T>::out_of_range(
                this->get_conversion_error_handler(), begin, end, 1);
        } else {
            return replacement<T>(static_cast<T>(r));
        }
    }
};

template <class T, class H, class U>
struct restrained_converter<T, H, U,
        std::enable_if_t<std::is_unsigned<T>::value>> :
    private raw_converter<U, H>
{
    static_assert(!std::is_floating_point<T>::value, "");

    using raw_converter<U, H>::raw_converter;
    using raw_converter<U, H>::get_conversion_error_handler;

    template <class Ch>
    replacement<T> convert(const Ch* begin, const Ch* end)
    {
        const auto result = this->convert_raw(begin, end);
        const auto p = result.get();
        if (!p) {
            return replacement<T>();
        }
        const auto r = convert_impl(*p,
            std::integral_constant<bool, sizeof(T) < sizeof(U)>());
        if (r.second) {
            return replacement<T>(r.first);
        } else {
            return conversion_error_facade<T>::out_of_range(
                this->get_conversion_error_handler(), begin, end, 1);
        }
    }

    std::pair<T, bool> convert_impl(U r, std::true_type)
    {
        constexpr auto t_max = std::numeric_limits<T>::max();
        if (r <= t_max) {
            return { static_cast<T>(r), true };
        } else {
            const auto s = static_cast<std::make_signed_t<U>>(r);
            if (s < 0) {
                // -t_max is the lowest number that can be wrapped around
                // and then returned
                const auto s_wrapped_around = s + t_max + 1;
                if (s_wrapped_around > 0) {
                    return { static_cast<T>(s_wrapped_around), true };
                }
            }
        }
        return { static_cast<T>(0U), false };
    }

    std::pair<T, bool> convert_impl(U r, std::false_type)
    {
        return { static_cast<T>(r), true };
    }
};

// For types which have corresponding "raw_type"
template <class T, class H>
struct converter<T, H, void_t<typename numeric_type_traits<T>::raw_type>> :
    restrained_converter<T, typed_conversion_error_handler<T, H>,
        typename numeric_type_traits<T>::raw_type>
{
    using restrained_converter<T, typed_conversion_error_handler<T, H>,
        typename numeric_type_traits<T>::raw_type>::restrained_converter;
};

}} // end detail::scanner

struct fail_if_skipped
{
    explicit fail_if_skipped(replacement_fail_t = replacement_fail)
    {}

    template <class T>
    [[noreturn]]
    replacement<T> operator()(T* = nullptr) const
    {
        throw field_not_found("This field did not appear in this record");
    }
};

namespace detail { namespace scanner { 

enum class replace_mode
{
    replace,
    fail,
    ignore
};

struct generic_args_t {};

namespace replace_if_skipped_impl {

template <class T>
class trivial_store
{
    alignas(T) char value_[sizeof(T)];
    replace_mode mode_;

public:
    template <class... Args>
    trivial_store(generic_args_t, Args&&... args)
        noexcept(std::is_nothrow_constructible<T, Args&&...>::value) :
        mode_(replace_mode::replace)
    {
        emplace(std::forward<Args>(args)...);
    }

    explicit trivial_store(replace_mode mode) noexcept :
        mode_(mode)
    {}

    replace_mode mode() const noexcept
    {
        return mode_;
    }

    const T& value() const
    {
        assert(mode_ == replace_mode::replace);
        return *static_cast<const T*>(static_cast<const void*>(value_));
    }

protected:
    replace_mode& mode() noexcept
    {
        return mode_;
    }

    template <class... Args>
    void emplace(Args&&... args)
        noexcept(std::is_nothrow_constructible<T, Args&&...>::value)
    {
        ::new(value_) T(std::forward<Args>(args)...);
    }

    T& value()
    {
        assert(mode_ == replace_mode::replace);
        return *static_cast<T*>(static_cast<void*>(value_));
    }
};

template <class T>
class nontrivial_store : public trivial_store<T>
{
public:
    // VS2015 dislikes:
    // using trivial_store<T>::trivial_store;
    // and raises an internal compiler error (C1001)

    template <class... Args>
    nontrivial_store(generic_args_t, Args&&... args)
        noexcept(std::is_nothrow_constructible<T, Args&&...>::value) :
        trivial_store<T>(generic_args_t(), std::forward<Args>(args)...)
    {}

    explicit nontrivial_store(replace_mode mode) noexcept :
        trivial_store<T>(mode)
    {}

    nontrivial_store(const nontrivial_store& other)
        noexcept(std::is_nothrow_copy_constructible<T>::value) :
        trivial_store<T>(other.mode())
    {
        if (this->mode() == replace_mode::replace) {
            this->emplace(other.value());                       // throw
        }
    }

    nontrivial_store(nontrivial_store&& other)
        noexcept(std::is_nothrow_move_constructible<T>::value) :
        trivial_store<T>(other.mode())
    {
        if (this->mode() == replace_mode::replace) {
            this->emplace(std::move(other.value()));            // throw
        }
    }

    ~nontrivial_store()
    {
        if (this->mode() == replace_mode::replace) {
            this->value().~T();
        }
    }

    nontrivial_store& operator=(const nontrivial_store& other)
        noexcept(std::is_nothrow_copy_constructible<T>::value
              && std::is_nothrow_copy_assignable<T>::value)
    {
        assign(other);
        return *this;
    }

    nontrivial_store& operator=(nontrivial_store&& other)
        noexcept(std::is_nothrow_move_constructible<T>::value
              && std::is_nothrow_move_assignable<T>::value)
    {
        assign(std::move(other));
        return *this;
    }

private:
    template <class Other>
    void assign(Other&& other)
    {
        using f_t = std::conditional_t<
            std::is_lvalue_reference<Other>::value, const T&, T>;
        if (this->mode() == replace_mode::replace) {
            if (other.mode() == replace_mode::replace) {
                if (this != std::addressof(other)) {
                    // Avoid self move assignment, which is unsafe at least
                    // for standard library types
                    this->value() =
                        std::forward<f_t>(other.value());       // throw
                }
                return;
            } else {
                this->value().~T();
            }
        } else if (other.mode() == replace_mode::replace) {
            this->emplace(std::forward<f_t>(other.value()));    // throw
        }
        this->mode() = other.mode();
    }

public:
    void swap(nontrivial_store& other)
        noexcept(detail::is_nothrow_swappable<T>()
              && std::is_nothrow_move_constructible<T>::value)
    {
        using std::swap;
        if (this->mode() == replace_mode::replace) {
            if (other.mode() == replace_mode::replace) {
                swap(this->value(), other.value());             // throw
                return;
            } else {
                other.emplace(std::move(this->value()));        // throw
                this->value().~T();
            }
        } else if (other.mode() == replace_mode::replace) {
            this->emplace(std::move(other.value()));            // throw
            other.value().~T();
        }
        swap(this->mode(), other.mode());
    }
};

template <class T>
void swap(nontrivial_store<T>& left, nontrivial_store<T>& right)
    noexcept(noexcept(left.swap(right)))
{
    left.swap(right);
}

template <class T>
using store_t = std::conditional_t<std::is_trivially_copyable<T>::value,
    trivial_store<T>, nontrivial_store<T>>;

}}}

template <class T>
class replace_if_skipped
{
    using replace_mode = detail::scanner::replace_mode;

    using generic_args_t = detail::scanner::generic_args_t;

    using store_t = detail::scanner::replace_if_skipped_impl::store_t<T>;
    store_t store_;

public:
    template <class... Args,
        std::enable_if_t<
            std::is_constructible<T, Args...>::value
         && !((sizeof...(Args) == 1)
           && (std::is_base_of<replace_if_skipped,
                detail::first_t<std::decay_t<Args>...>>::value
            || std::is_base_of<replacement_ignore_t,
                detail::first_t<std::decay_t<Args>...>>::value
            || std::is_base_of<replacement_fail_t,
                detail::first_t<std::decay_t<Args>...>>::value))>* = nullptr>
    explicit replace_if_skipped(Args&&... args)
        noexcept(std::is_nothrow_constructible<T, Args&&...>::value) :
        store_(generic_args_t(), std::forward<Args>(args)...)
    {}

    explicit replace_if_skipped(replacement_fail_t) noexcept :
        store_(detail::scanner::replace_mode::fail)
    {}

    explicit replace_if_skipped(replacement_ignore_t) noexcept :
        store_(detail::scanner::replace_mode::ignore)
    {}

    replace_if_skipped(const replace_if_skipped&) = default;
    replace_if_skipped(replace_if_skipped&&) = default;
    ~replace_if_skipped() = default;
    replace_if_skipped& operator=(const replace_if_skipped&) = default;
    replace_if_skipped& operator=(replace_if_skipped&&) = default;

    replacement<T> operator()() const
    {
        switch (store_.mode()) {
        case replace_mode::replace:
            return replacement<T>(store_.value());
        case replace_mode::fail:
            fail_if_skipped().operator()<T>();
            // fall through
        default:
            return replacement<T>();
        }
    }

    void swap(replace_if_skipped& other)
        noexcept(detail::is_nothrow_swappable<store_t>())
    {
        using std::swap;
        swap(store_, other.store_);
    }
};

template <class T>
void swap(replace_if_skipped<T>& left, replace_if_skipped<T>& right)
    noexcept(noexcept(left.swap(right)))
{
    left.swap(right);
}

struct fail_if_conversion_failed
{
    template <class T, class Ch>
    [[noreturn]]
    replacement<T> operator()(invalid_format_t,
        const Ch* begin, const Ch* end, T* = nullptr) const
    {
        assert(*end == Ch());
        std::ostringstream s;
        detail::write_ntmbs(s, begin, end);
        s << ": cannot convert";
        write_name<T>(s, " to an instance of ");
        throw field_invalid_format(std::move(s).str());
    }

    template <class T, class Ch>
    [[noreturn]]
    replacement<T> operator()(out_of_range_t,
        const Ch* begin, const Ch* end, int, T* = nullptr) const
    {
        assert(*end == Ch());
        std::ostringstream s;
        detail::write_ntmbs(s, begin, end);
        s << ": out of range";
        write_name<T>(s, " of ");
        throw field_out_of_range(std::move(s).str());
    }

    template <class T>
    [[noreturn]]
    replacement<T> operator()(empty_t, T* = nullptr) const
    {
        std::ostringstream s;
        s << "Cannot convert an empty string";
        write_name<T>(s, " to an instance of ");
        throw field_empty(std::move(s).str());
    }

private:
    template <class T>
    static std::ostream& write_name(std::ostream& os, const char* prefix,
        decltype(detail::scanner::numeric_type_traits<T>::name)* = nullptr)
    {
        return os << prefix << detail::scanner::numeric_type_traits<T>::name;
    }

    template <class T>
    static std::ostream& write_name(std::ostream& os, ...)
    {
        return os;
    }
};

namespace detail { namespace scanner {
namespace replace_if_conversion_failed_impl {

enum slot : unsigned {
    slot_empty = 0,
    slot_invalid_format = 1,
    slot_above_upper_limit = 2,
    slot_below_lower_limit = 3,
    slot_underflow = 4
};

struct copy_mode_t {};

template <class T, unsigned N>
struct trivial_store
{
private:
    alignas(T) char replacements_[N][sizeof(T)];
    std::uint_fast8_t has_;
    std::uint_fast8_t skips_;

protected:
    trivial_store(copy_mode_t, const trivial_store& other) noexcept :
        has_(other.has_), skips_(other.skips_)
    {}

public:
    template <class Head, class... Tails>
    trivial_store(generic_args_t, Head&& head, Tails&&... tails) :
        trivial_store(std::integral_constant<std::size_t, 0>(),
            std::forward<Head>(head), std::forward<Tails>(tails)...)
    {}

private:
    template <std::size_t Slot, class Head, class... Tails>
    trivial_store(std::integral_constant<std::size_t, Slot>,
                  Head&& head, Tails&&... tails) :
        trivial_store(std::integral_constant<std::size_t, Slot + 1>(),
            std::forward<Tails>(tails)...)
    {
        init<Slot>(std::forward<Head>(head));
    }

    template <std::size_t Slot>
    trivial_store(std::integral_constant<std::size_t, Slot>) :
        trivial_store(std::integral_constant<std::size_t, Slot + 1>())
    {
        init<Slot>();
    }

    trivial_store(std::integral_constant<std::size_t, N>) noexcept :
        has_(0), skips_(0)
    {}

    template <unsigned Slot, class U, std::enable_if_t<
        (!std::is_base_of<replacement_fail_t, std::decay_t<U>>::value)
     && (!std::is_base_of<replacement_ignore_t, std::decay_t<U>>::value)>*
        = nullptr>
    void init(U&& value)
        noexcept(std::is_nothrow_constructible<T, U&&>::value)
    {
        static_assert(Slot < N, "");
        assert(!has(Slot));
        assert(!skips(Slot));
        emplace(Slot, std::forward<U>(value));
        set_has(Slot);
    }

    template <unsigned Slot>
    void init() noexcept(std::is_nothrow_default_constructible<T>::value)
    {
        static_assert(Slot < N, "");
        assert(!has(Slot));
        assert(!skips(Slot));
        emplace(Slot);
        set_has(Slot);
    }

    template <unsigned Slot>
    void init(replacement_fail_t) noexcept
    {
        static_assert(Slot < N, "");
        assert(!has(Slot));
        assert(!skips(Slot));
    }

    template <unsigned Slot>
    void init(replacement_ignore_t) noexcept
    {
        static_assert(Slot < N, "");
        assert(!has(Slot));
        assert(!skips(Slot));
        set_skips(Slot);
    }

public:
    std::pair<replace_mode, const T*> get(unsigned r) const noexcept
    {
        return get_g(this, r);
    }

private:
    template <class ThisType>
    static auto get_g(ThisType* me, unsigned r) noexcept
    {
        using t_t = std::conditional_t<
            std::is_const<ThisType>::value, const T, T>;
        using pair_t = std::pair<replace_mode, t_t*>;
        if (me->has(r)) {
            return pair_t(replace_mode::replace, std::addressof((*me)[r]));
        } else if (me->skips(r)) {
            return pair_t(replace_mode::ignore, nullptr);
        } else {
            return pair_t(replace_mode::fail, nullptr);
        }
    }

protected:
    T& operator[](unsigned r)
    {
        return *static_cast<T*>(static_cast<void*>(replacements_[r]));
    }

    const T& operator[](unsigned r) const
    {
        return *static_cast<const T*>(
            static_cast<const void*>(replacements_[r]));
    }

    template <class... Args>
    void emplace(unsigned r, Args&&... args)
        noexcept(std::is_nothrow_constructible<T, Args&&...>::value)
    {
        ::new(replacements_[r]) T(std::forward<Args>(args)...);
    }

    bool has(unsigned r) const noexcept
    {
        return (has_ & (1 << r)) != 0;
    }

    void set_has(unsigned r) noexcept
    {
        has_ |= (1 << r);
    }

    void set_has(unsigned r, bool v) noexcept
    {
        if (v) {
            set_has(r);
        } else {
            has_ &= ~(1 << r);
        }
    }

    bool skips(unsigned r) const noexcept
    {
        return (skips_ & (1 << r)) != 0;
    }

    void set_skips(unsigned r) noexcept
    {
        skips_ |= (1 << r);
    }

    void set_skips(unsigned r, bool v) noexcept
    {
        if (v) {
            set_skips(r);
        } else {
            skips_ &= ~(1 << r);
        }
    }
};

template <class T, unsigned N>
struct nontrivial_store : trivial_store<T, N>
{
    template <class... Args>
    nontrivial_store(generic_args_t, Args&&... args) :
        trivial_store<T, N>(generic_args_t(), std::forward<Args>(args)...)
    {}

    nontrivial_store(const nontrivial_store& other)
        noexcept(std::is_nothrow_copy_constructible<T>::value) :
        trivial_store<T, N>(copy_mode_t(), other)
    {
        for (unsigned r = 0; r < N; ++r) {
            if (this->has(r)) {
                this->emplace(r, other[r]);
            }
        }
    }

    nontrivial_store(nontrivial_store&& other)
        noexcept(std::is_nothrow_move_constructible<T>::value) :
        trivial_store<T, N>(copy_mode_t(), other)
    {
        for (unsigned r = 0; r < N; ++r) {
            if (this->has(r)) {
                this->emplace(r, std::move(other[r]));
            }
        }
    }

    ~nontrivial_store()
    {
        for (unsigned r = 0; r < N; ++r) {
            if (this->has(r)) {
                (*this)[r].~T();
            }
        }
    }

    nontrivial_store& operator=(const nontrivial_store& other)
        noexcept(std::is_nothrow_copy_constructible<T>::value
              && std::is_nothrow_copy_assignable<T>::value)
    {
        assign(other);
        return *this;
    }

    nontrivial_store& operator=(nontrivial_store&& other)
        noexcept(std::is_nothrow_move_constructible<T>::value
              && std::is_nothrow_move_assignable<T>::value)
    {
        assign(std::move(other));
        return *this;
    }

private:
    template <class Other>
    void assign(Other&& other)
    {
        if (this == std::addressof(other)) {
            return; // see comments in replace_if_skipped
        }
        using f_t = std::conditional_t<
            std::is_lvalue_reference<Other>::value, const T&, T>;
        for (unsigned r = 0; r < N; ++r) {
            if (this->has(r)) {
                if (other.has(r)) {
                    (*this)[r] = std::forward<f_t>(other[r]);
                    continue;
                } else {
                    (*this)[r].~T();
                }
            } else if (other.has(r)) {
                this->emplace(r, std::forward<f_t>(other[r]));
            }
            this->set_has(r, other.has(r));
            this->set_skips(r, other.skips(r));
        }
    }

public:
    void swap(nontrivial_store& other)
        noexcept(detail::is_nothrow_swappable<T>()
              && std::is_nothrow_constructible<T>::value)
    {
        for (unsigned r = 0; r < N; ++r) {
            if (this->has(r)) {
                if (other.has(r)) {
                    using std::swap;
                    swap((*this)[r], other[r]);
                    continue;
                } else {
                    other.emplace(r, std::move((*this)[r]));
                    (*this)[r].~T();
                }
            } else if (other.has(r)) {
                this->emplace(r, std::move(other[r]));
                other[r].~T();
            }

            {
                const bool t = this->has(r);
                this->set_has(r, other.has(r));
                other.set_has(r, t);
            }
            {
                const bool t = this->skips(r);
                this->set_skips(r, other.skips(r));
                other.set_skips(r, t);
            }
        }
    }
};

template <class T, unsigned N>
void swap(nontrivial_store<T, N>& left, nontrivial_store<T, N>& right)
    noexcept(noexcept(left.swap(right)))
{
    left.swap(right);
}

template <class T, class A>
struct is_acceptable_arg :
    std::integral_constant<bool,
        std::is_base_of<replacement_fail_t, std::decay_t<A>>::value
     || std::is_base_of<replacement_ignore_t, std::decay_t<A>>::value
     || std::is_constructible<T, A>::value>
{};

template <class T, class A>
struct is_nothrow_arg :
    std::integral_constant<bool,
        std::is_base_of<replacement_fail_t, std::decay_t<A>>::value
     || std::is_base_of<replacement_ignore_t, std::decay_t<A>>::value
     || std::is_nothrow_constructible<T, A>::value>
{};

template <class T>
constexpr unsigned base_n =
    std::is_integral<T>::value ? std::is_signed<T>::value ? 4 : 3 : 5;

template <class T, unsigned N>
struct base;

template <class T>
struct base<T, 0>
{
protected:
    using store_t = std::conditional_t<std::is_trivially_copyable<T>::value,
        trivial_store<T, base_n<T>>, nontrivial_store<T, base_n<T>>>;

private:
    store_t store_;

protected:
    template <class... As>
    base(detail::scanner::generic_args_t, As&&... as) :
        store_(detail::scanner::generic_args_t(), std::forward<As>(as)...)
    {}

    const store_t& store() const
    {
        return store_;
    }

    store_t& store()
    {
        return store_;
    }
};

template <class T>
struct base<T, 3> : base<T, 0>
{
    using base<T, 0>::base;

    template <class Empty = T, class InvalidFormat = T,
        class AboveUpperLimit = T,
        std::enable_if_t<
            is_acceptable_arg<T, Empty>::value
         && is_acceptable_arg<T, InvalidFormat>::value
         && is_acceptable_arg<T, AboveUpperLimit>::value>* = nullptr>
    explicit base(
        Empty&& on_empty = Empty(),
        InvalidFormat&& on_invalid_format = InvalidFormat(),
        AboveUpperLimit&& on_above_upper_limit = AboveUpperLimit())
            noexcept(std::is_nothrow_default_constructible<T>::value
                  && is_nothrow_arg<T, Empty>::value
                  && is_nothrow_arg<T, InvalidFormat>::value
                  && is_nothrow_arg<T, AboveUpperLimit>::value) :
        base<T, 0>(
            detail::scanner::generic_args_t(),
            std::forward<Empty>(on_empty),
            std::forward<InvalidFormat>(on_invalid_format),
            std::forward<AboveUpperLimit>(on_above_upper_limit))
    {}
};

template <class T>
struct base<T, 4> : base<T, 3>
{
    using base<T, 3>::base;

    template <class Empty, class InvalidFormat,
        class AboveUpperLimit, class BelowLowerLimit,
        std::enable_if_t<
            is_acceptable_arg<T, Empty>::value
         && is_acceptable_arg<T, InvalidFormat>::value
         && is_acceptable_arg<T, AboveUpperLimit>::value
         && is_acceptable_arg<T, BelowLowerLimit>::value>* = nullptr>
    base(
        Empty&& on_empty,
        InvalidFormat&& on_invalid_format,
        AboveUpperLimit&& on_above_upper_limit,
        BelowLowerLimit&& on_below_lower_limit)
            noexcept(std::is_nothrow_default_constructible<T>::value
                  && is_nothrow_arg<T, Empty>::value
                  && is_nothrow_arg<T, InvalidFormat>::value
                  && is_nothrow_arg<T, AboveUpperLimit>::value
                  && is_nothrow_arg<T, BelowLowerLimit>::value) :
        base<T, 3>(
            detail::scanner::generic_args_t(),
            std::forward<Empty>(on_empty),
            std::forward<InvalidFormat>(on_invalid_format),
            std::forward<AboveUpperLimit>(on_above_upper_limit),
            std::forward<BelowLowerLimit>(on_below_lower_limit))
    {}
};

template <class T>
struct base<T, 5> : base<T, std::is_default_constructible<T>::value ? 4 : 0>
{
    using base<T, std::is_default_constructible<T>::value ? 4 : 0>::base;

    template <class Empty, class InvalidFormat,
        class AboveUpperLimit, class BelowLowerLimit,
        class Underflow,
        std::enable_if_t<
            is_acceptable_arg<T, Empty>::value
         && is_acceptable_arg<T, InvalidFormat>::value
         && is_acceptable_arg<T, AboveUpperLimit>::value
         && is_acceptable_arg<T, BelowLowerLimit>::value
         && is_acceptable_arg<T, Underflow>::value>* = nullptr>
    base(
        Empty&& on_empty,
        InvalidFormat&& on_invalid_format,
        AboveUpperLimit&& on_above_upper_limit,
        BelowLowerLimit&& on_below_lower_limit,
        Underflow&& on_underflow)
            noexcept(is_nothrow_arg<T, Empty>::value
                  && is_nothrow_arg<T, InvalidFormat>::value
                  && is_nothrow_arg<T, AboveUpperLimit>::value
                  && is_nothrow_arg<T, BelowLowerLimit>::value
                  && is_nothrow_arg<T, Underflow>::value) :
        base<T, std::is_default_constructible<T>::value ? 4 : 0>(
            detail::scanner::generic_args_t(),
            std::forward<Empty>(on_empty),
            std::forward<InvalidFormat>(on_invalid_format),
            std::forward<AboveUpperLimit>(on_above_upper_limit),
            std::forward<BelowLowerLimit>(on_below_lower_limit),
            std::forward<Underflow>(on_underflow))
    {}
};

template <class T>
using base_t = base<T, base_n<T>>;

}}}

template <class T>
class replace_if_conversion_failed :
    public detail::scanner::replace_if_conversion_failed_impl::base_t<T>
{
    using replace_mode = detail::scanner::replace_mode;

    static constexpr unsigned slot_empty =
        detail::scanner::replace_if_conversion_failed_impl::slot_empty;
    static constexpr unsigned slot_invalid_format =
        detail::scanner::replace_if_conversion_failed_impl::
            slot_invalid_format;
    static constexpr unsigned slot_above_upper_limit =
        detail::scanner::replace_if_conversion_failed_impl::
            slot_above_upper_limit;
    static constexpr unsigned slot_below_lower_limit =
        detail::scanner::replace_if_conversion_failed_impl::
            slot_below_lower_limit;
    static constexpr unsigned slot_underflow =
        detail::scanner::replace_if_conversion_failed_impl::slot_underflow;

private:
    template <class... As>
    replace_if_conversion_failed(detail::scanner::generic_args_t, As&&...)
        = delete;

public:
    // VS2015 refuses to compile "base_t<T>" here
    using detail::scanner::replace_if_conversion_failed_impl::base<T,
        detail::scanner::replace_if_conversion_failed_impl::base_n<T>>::base;

    replace_if_conversion_failed(
        const replace_if_conversion_failed&) = default;
    replace_if_conversion_failed(
        replace_if_conversion_failed&&) = default;
    ~replace_if_conversion_failed() = default;
    replace_if_conversion_failed& operator=(
        const replace_if_conversion_failed&) = default;
    replace_if_conversion_failed& operator=(
        replace_if_conversion_failed&&) = default;

    template <class Ch>
    replacement<T> operator()(invalid_format_t,
        const Ch* begin, const Ch* end) const
    {
        return unwrap(this->store().get(slot_invalid_format),
            [begin, end]() {
                fail_if_conversion_failed()(invalid_format_t(),
                    begin, end, static_cast<T*>(nullptr));
            });
    }

    template <class Ch>
    replacement<T> operator()(out_of_range_t,
        const Ch* begin, const Ch* end, int sign) const
    {
        const auto fail = [begin, end, sign]() {
            fail_if_conversion_failed()(out_of_range_t(),
                begin, end, sign, static_cast<T*>(nullptr));
        };
        if (sign > 0) {
            return unwrap(this->store().get(slot_above_upper_limit), fail);
        } else if (sign < 0) {
            return unwrap(this->store().get(slot_below_lower_limit), fail);
        } else {
            return unwrap(this->store().get(slot_underflow), fail);
        }
    }

    replacement<T> operator()(empty_t) const
    {
        return unwrap(this->store().get(slot_empty),
            []() {
                fail_if_conversion_failed()(empty_t(),
                    static_cast<T*>(nullptr));
            });
    }

    void swap(replace_if_conversion_failed& other)
        noexcept(detail::is_nothrow_swappable<T>()
              && std::is_nothrow_constructible<T>::value)
    {
        using std::swap;
        swap(this->store(), other.store());
    }

private:
    template <class Fail>
    auto unwrap(const std::pair<replace_mode, const T*>& p, Fail fail) const
    {
        switch (p.first) {
        case replace_mode::replace:
            return replacement<T>(*p.second);
        case replace_mode::ignore:
            return replacement<T>();
        case replace_mode::fail:
        default:
            break;
        }
        fail();
        assert(false);
        return replacement<T>();
    }
};

template <class T>
void swap(
    replace_if_conversion_failed<T>& left,
    replace_if_conversion_failed<T>& right)
    noexcept(noexcept(left.swap(right)))
{
    left.swap(right);
}

namespace detail { namespace scanner {

template <class T, class = void>
struct is_output_iterator : std::false_type
{};

template <class T>
struct is_output_iterator<T,
    std::enable_if_t<
        std::is_base_of<
            std::output_iterator_tag,
            typename std::iterator_traits<T>::iterator_category>::value
     || std::is_base_of<
            std::forward_iterator_tag,
            typename std::iterator_traits<T>::iterator_category>::value>> :
    std::true_type
{};

struct has_simple_call_impl
{
    template <class F>
    static auto check(F*) -> decltype(
        std::declval<F&>()(),
        std::true_type());

    template <class F>
    static auto check(...) -> std::false_type;
};

template <class F>
struct has_simple_call :
    decltype(has_simple_call_impl::check<F>(nullptr))
{};

template <class T, class Sink, class SkippingHandler>
class translator :
    member_like_base<SkippingHandler>
{
    Sink sink_;

public:
    template <class SinkR, class SkippingHandlerR>
    translator(SinkR&& sink, SkippingHandlerR&& handle_skipping) :
        member_like_base<SkippingHandler>(
            std::forward<SkippingHandlerR>(handle_skipping)),
        sink_(std::forward<SinkR>(sink))
    {}

    const SkippingHandler& get_skipping_handler() const noexcept
    {
        return this->get();
    }

    SkippingHandler& get_skipping_handler() noexcept
    {
        return this->get();
    }

    void field_skipped()
    {
        if (auto r = call_skipping_handler(
                has_simple_call<SkippingHandler>())) {
            put(std::move(*r));
        }
    }

private:
    auto call_skipping_handler(std::true_type)
    {
        return get_skipping_handler()();
    }

    auto call_skipping_handler(std::false_type)
    {
        return get_skipping_handler()(static_cast<T*>(nullptr));
    }

public:
    template <class U>
    void put(U&& value)
    {
        do_put(std::forward<U>(value), is_output_iterator<Sink>());
    }

    template <class U>
    void put(replacement<U>&& value)
    {
        do_put(std::move(value), is_output_iterator<Sink>());
    }

private:
    template <class U>
    void do_put(U&& value, std::true_type)
    {
        *sink_ = std::forward<U>(value);
        ++sink_;
    }

    template <class U>
    void do_put(U&& value, std::false_type)
    {
        sink_(std::forward<U>(value));
    }

    template <class U>
    void do_put(replacement<U>&& value, std::true_type)
    {
        if (auto p = value.get()) {
            *sink_ = std::move(*p);
            ++sink_;
        }
    }

    template <class U>
    void do_put(replacement<U>&& value, std::false_type)
    {
        if (auto p = value.get()) {
            sink_(std::move(*p));
        }
    }
};

}} // end detail::scanner

template <class T, class Sink,
    class SkippingHandler = fail_if_skipped,
    class ConversionErrorHandler = fail_if_conversion_failed>
class arithmetic_field_translator
{
    using converter_t = detail::scanner::converter<T, ConversionErrorHandler>;
    using translator_t = detail::scanner::translator<T, Sink, SkippingHandler>;

    detail::base_member_pair<converter_t, translator_t> ct_;

public:
    template <class SinkR, class SkippingHandlerR = SkippingHandler,
              class ConversionErrorHandlerR = ConversionErrorHandler,
              std::enable_if_t<
                !std::is_base_of<
                    arithmetic_field_translator, std::decay_t<SinkR>>::value>*
                        = nullptr>
    explicit arithmetic_field_translator(
        SinkR&& sink,
        SkippingHandlerR&& handle_skipping = SkippingHandler(),
        ConversionErrorHandlerR&& handle_conversion_error
            = ConversionErrorHandler()) :
        ct_(converter_t(std::forward<ConversionErrorHandlerR>(
                            handle_conversion_error)),
            translator_t(std::forward<SinkR>(sink),
                         std::forward<SkippingHandlerR>(handle_skipping)))
    {}

    arithmetic_field_translator(const arithmetic_field_translator&) = default;
    arithmetic_field_translator(arithmetic_field_translator&&) = default;
    ~arithmetic_field_translator() = default;

    const SkippingHandler& get_skipping_handler() const noexcept
    {
        return ct_.member().get_skipping_handler();
    }

    SkippingHandler& get_skipping_handler() noexcept
    {
        return ct_.member().get_skipping_handler();
    }

    void operator()()
    {
        ct_.member().field_skipped();
    }

    ConversionErrorHandler& get_conversion_error_handler() noexcept
    {
        return ct_.base().get_conversion_error_handler();
    }

    const ConversionErrorHandler& get_conversion_error_handler() const noexcept
    {
        return ct_.base().get_conversion_error_handler();
    }

    template <class Ch>
    void operator()(const Ch* begin, const Ch* end)
    {
        assert(*end == Ch());
        ct_.member().put(ct_.base().convert(begin, end));
    }
};

template <class T, class Sink,
    class SkippingHandler = fail_if_skipped,
    class ConversionErrorHandler = fail_if_conversion_failed>
class locale_based_arithmetic_field_translator
{
    std::locale loc_;
    arithmetic_field_translator<
        T, Sink, SkippingHandler, ConversionErrorHandler> out_;

    // These are initialized after parsing has started
    wchar_t decimal_point_;     // of specified loc in the ctor
    wchar_t thousands_sep_;     // ditto
    wchar_t decimal_point_c_;   // of C's global
                                // to mimic std::strtol and its comrades
    wchar_t thousands_sep_c_;   // ditto
    bool mimics_;

public:
    template <class SinkR, class SkippingHandlerR = SkippingHandler,
              class ConversionErrorHandlerR = ConversionErrorHandler,
              std::enable_if_t<
                !std::is_base_of<
                    locale_based_arithmetic_field_translator,
                    std::decay_t<SinkR>>::value>* = nullptr>
    locale_based_arithmetic_field_translator(
        SinkR&& sink, const std::locale& loc,
        SkippingHandlerR&& handle_skipping = SkippingHandler(),
        ConversionErrorHandlerR&& handle_conversion_error
            = ConversionErrorHandler()) :
        loc_(loc),
        out_(std::forward<SinkR>(sink),
             std::forward<SkippingHandlerR>(handle_skipping),
             std::forward<ConversionErrorHandlerR>(handle_conversion_error)),
        decimal_point_c_(), mimics_()
    {}

    locale_based_arithmetic_field_translator(
        const locale_based_arithmetic_field_translator&) = default;
    locale_based_arithmetic_field_translator(
        locale_based_arithmetic_field_translator&&) = default;
    ~locale_based_arithmetic_field_translator() = default;

    const SkippingHandler& get_skipping_handler() const noexcept
    {
        return out_.get_skipping_handler();
    }

    SkippingHandler& get_skipping_handler() noexcept
    {
        return out_.get_skipping_handler();
    }

    void operator()()
    {
        out_();
    }

    ConversionErrorHandler& get_conversion_error_handler() noexcept
    {
        return out_.get_conversion_error_handler();
    }

    const ConversionErrorHandler& get_conversion_error_handler() const noexcept
    {
        return out_.get_conversion_error_handler();
    }

    template <class Ch>
    void operator()(Ch* begin, Ch* end)
    {
        if (decimal_point_c_ == wchar_t()) {
            decimal_point_c_ = widen(*std::localeconv()->decimal_point, Ch());
            thousands_sep_c_ = widen(*std::localeconv()->thousands_sep, Ch());

            const auto& facet = std::use_facet<std::numpunct<Ch>>(loc_);
            thousands_sep_ = facet.grouping().empty() ?
                Ch() : facet.thousands_sep();
            decimal_point_ = facet.decimal_point();

            mimics_ = (decimal_point_ != decimal_point_c_)
                   || ((thousands_sep_ != Ch()) // means needs to take care of
                                                // separators in field values
                    && (thousands_sep_ != thousands_sep_c_));
        }
        assert(*end == Ch());   // shall be null-terminated
        if (mimics_) {
            bool decimal_point_appeared = false;
            Ch* head = begin;
            for (Ch* b = begin; b != end; ++b) {
                Ch c = *b;
                assert(c != Ch());
                if (c == static_cast<Ch>(decimal_point_)) {
                    if (!decimal_point_appeared) {
                        c = static_cast<Ch>(decimal_point_c_);
                        decimal_point_appeared = true;
                    }
                } else if (c == static_cast<Ch>(thousands_sep_)) {
                    continue;
                }
                *head = c;
                ++head;
            }
            *head = Ch();
            out_(begin, head);
        } else {
            out_(begin, end);
        }
    }

private:
    static char widen(char c, char)
    {
        return c;
    }

    static wchar_t widen(char c, wchar_t)
    {
        return std::btowc(static_cast<unsigned char>(c));
    }
};

template <class Sink, class Ch,
    class Tr = std::char_traits<Ch>, class Allocator = std::allocator<Ch>,
    class SkippingHandler = fail_if_skipped>
class string_field_translator
{
    using translator_t =
        detail::scanner::translator<std::basic_string<Ch, Tr, Allocator>,
        Sink, SkippingHandler>;

    detail::base_member_pair<Allocator, translator_t> at_;

public:
    using allocator_type = Allocator;

    template <class SinkR, class SkippingHandlerR = SkippingHandler,
              std::enable_if_t<
                !std::is_base_of<
                    string_field_translator, std::decay_t<SinkR>>::value>*
                        = nullptr>
    explicit string_field_translator(
        SinkR&& sink, SkippingHandlerR&& handle_skipping = SkippingHandler()) :
        at_(Allocator(),
            translator_t(std::forward<SinkR>(sink),
                         std::forward<SkippingHandlerR>(handle_skipping)))
    {}

    template <class SinkR, class SkippingHandlerR = SkippingHandler>
    string_field_translator(
        std::allocator_arg_t, const Allocator& alloc, SinkR&& sink,
        SkippingHandlerR&& handle_skipping = SkippingHandler()) :
        at_(alloc,
            translator_t(std::forward<SinkR>(sink),
                         std::forward<SkippingHandlerR>(handle_skipping)))
    {}

    string_field_translator(const string_field_translator&) = default;
    string_field_translator(string_field_translator&&) = default;
    ~string_field_translator() = default;

    allocator_type get_allocator() const noexcept
    {
        return at_.base();
    }

    const SkippingHandler& get_skipping_handler() const noexcept
    {
        return at_.member().get_skipping_handler();
    }

    SkippingHandler& get_skipping_handler() noexcept
    {
        return at_.member().get_skipping_handler();
    }

    void operator()()
    {
        at_.member().field_skipped();
    }

    void operator()(const Ch* begin, const Ch* end)
    {
        at_.member().put(std::basic_string<Ch, Tr, Allocator>(
            begin, end, get_allocator()));
    }

    void operator()(std::basic_string<Ch, Tr, Allocator>&& value)
    {
        // std::basic_string which comes with gcc 7.3.1 does not seem to have
        // "move-with-specified-allocator" ctor
        if (value.get_allocator() == get_allocator()) {
            at_.member().put(std::move(value));
        } else {
            (*this)(value.data(), value.data() + value.size());
        }
    }
};

namespace detail { namespace scanner {

template <class T, class Sink, class... As,
    std::enable_if_t<
        !detail::is_std_string<T>::value,
        std::nullptr_t> = nullptr>
arithmetic_field_translator<T, Sink, fail_if_skipped, As...>
    make_field_translator_na(Sink, replacement_fail_t, As...);

template <class T, class Sink, class ReplaceIfSkipped, class... As,
    std::enable_if_t<
        !detail::is_std_string<T>::value
     && !std::is_base_of<std::locale, std::decay_t<ReplaceIfSkipped>>::value
     && !std::is_base_of<replacement_fail_t,
                         std::decay_t<ReplaceIfSkipped>>::value
     && std::is_constructible<replace_if_skipped<T>, ReplaceIfSkipped>::value,
        std::nullptr_t> = nullptr>
arithmetic_field_translator<T, Sink, replace_if_skipped<T>, As...>
    make_field_translator_na(Sink, ReplaceIfSkipped&&, As...);

template <class T, class Sink, class... As,
    std::enable_if_t<
        !detail::is_std_string<T>::value
     && ((sizeof...(As) == 0)
      || !(std::is_base_of<std::locale,
                           std::decay_t<detail::first_t<As...>>>::value
        || std::is_constructible<replace_if_skipped<T>,
                                 detail::first_t<As...>>::value)),
        std::nullptr_t> = nullptr>
arithmetic_field_translator<T, Sink, std::decay_t<As>...>
    make_field_translator_na(Sink, As&&...);

template <class T, class Sink, class... As,
    std::enable_if_t<
        !detail::is_std_string<T>::value,
        std::nullptr_t> = nullptr>
locale_based_arithmetic_field_translator<T, Sink, fail_if_skipped, As...>
    make_field_translator_na(Sink, std::locale, replacement_fail_t, As...);

template <class T, class Sink, class ReplaceIfSkipped, class... As,
    std::enable_if_t<
        !detail::is_std_string<T>::value
     && !std::is_base_of<replacement_fail_t,
                         std::decay_t<ReplaceIfSkipped>>::value
     && std::is_constructible<replace_if_skipped<T>, ReplaceIfSkipped>::value,
        std::nullptr_t> = nullptr>
locale_based_arithmetic_field_translator<T, Sink, replace_if_skipped<T>, As...>
    make_field_translator_na(Sink, std::locale, ReplaceIfSkipped&&, As...);

template <class T, class Sink, class... As,
    std::enable_if_t<
        !detail::is_std_string<T>::value
     && ((sizeof...(As) == 0)
      || !std::is_constructible<replace_if_skipped<T>,
                                detail::first_t<As...>>::value),
        std::nullptr_t> = nullptr>
locale_based_arithmetic_field_translator<T, Sink, std::decay_t<As>...>
    make_field_translator_na(Sink, std::locale, As&&... as);

template <class T, class Sink,
    std::enable_if_t<detail::is_std_string<T>::value, std::nullptr_t>
        = nullptr>
string_field_translator<Sink,
        typename T::value_type, typename T::traits_type,
        typename T::allocator_type, fail_if_skipped>
    make_field_translator_na(Sink, replacement_fail_t);

template <class T, class Sink, class ReplaceIfSkipped,
    std::enable_if_t<
        detail::is_std_string<T>::value
     && !std::is_base_of<replacement_fail_t,
                         std::decay_t<ReplaceIfSkipped>>::value
     && std::is_constructible<replace_if_skipped<T>, ReplaceIfSkipped>::value,
        std::nullptr_t> = nullptr>
string_field_translator<Sink,
        typename T::value_type, typename T::traits_type,
        typename T::allocator_type, replace_if_skipped<T>>
    make_field_translator_na(Sink, ReplaceIfSkipped&&);

template <class T, class Sink, class... As,
    std::enable_if_t<
        detail::is_std_string<T>::value
     && ((sizeof...(As) == 0)
      || !std::is_constructible<replace_if_skipped<T>,
                                detail::first_t<As...>>::value),
        std::nullptr_t> = nullptr>
string_field_translator<Sink,
        typename T::value_type, typename T::traits_type,
        typename T::allocator_type, std::decay_t<As>...>
    make_field_translator_na(Sink, As&&...);

}} // end detail::scanner

template <class T, class Sink, class... Appendices>
auto make_field_translator(Sink&& sink, Appendices&&... appendices)
 -> std::enable_if_t<
        (detail::scanner::is_convertible_numeric_type<T>::value
      || detail::is_std_string<T>::value)
     && (detail::scanner::is_output_iterator<std::decay_t<Sink>>::value
      || detail::is_callable<std::decay_t<Sink>&, T>::value),
        decltype(detail::scanner::make_field_translator_na<T>(
                    std::forward<Sink>(sink),
                    std::forward<Appendices>(appendices)...))>
{
    using t = decltype(detail::scanner::make_field_translator_na<T>(
        std::forward<Sink>(sink), std::forward<Appendices>(appendices)...));
    return t(std::forward<Sink>(sink),
             std::forward<Appendices>(appendices)...);
}

template <class T, class Allocator, class Sink, class... Appendices>
auto make_field_translator(std::allocator_arg_t, const Allocator& alloc,
    Sink&& sink, Appendices&&... appendices)
 -> std::enable_if_t<
        detail::is_std_string<T>::value
     && std::is_same<Allocator, typename T::allocator_type>::value
     && (detail::scanner::is_output_iterator<std::decay_t<Sink>>::value
      || detail::is_callable<std::decay_t<Sink>&, T>::value),
        decltype(detail::scanner::make_field_translator_na<T>(
                    std::forward<Sink>(sink),
                    std::forward<Appendices>(appendices)...))>
{
    using t = decltype(detail::scanner::make_field_translator_na<T>(
        std::forward<Sink>(sink), std::forward<Appendices>(appendices)...));
    return t(std::allocator_arg, alloc,
        std::forward<Sink>(sink), std::forward<Appendices>(appendices)...);
}

namespace detail { namespace scanner {

struct is_back_insertable_impl
{
    template <class T>
    static auto check(T*) -> decltype(
        std::declval<T&>().push_back(std::declval<typename T::value_type>()),
        std::true_type());

    template <class T>
    static auto check(...) -> std::false_type;
};

template <class T>
struct is_back_insertable :
    decltype(is_back_insertable_impl::check<T>(nullptr))
{};

struct is_insertable_impl
{
    template <class T>
    static auto check(T*) -> decltype(
        std::declval<T&>().insert(std::declval<T&>().end(),
            std::declval<typename T::value_type>()),
        std::true_type());

    template <class T>
    static auto check(...)->std::false_type;
};

template <class T>
struct is_insertable :
    decltype(is_insertable_impl::check<T>(nullptr))
{};

template <class Container, class = void>
struct back_insert_iterator;

template <class Container>
struct back_insert_iterator<Container,
    std::enable_if_t<is_insertable<Container>::value
                  && is_back_insertable<Container>::value>>
{
    using type = std::back_insert_iterator<Container>;

    static type from(Container& c)
    {
        return std::back_inserter(c);
    }
};

template <class Container>
struct back_insert_iterator<Container,
    std::enable_if_t<is_insertable<Container>::value
                  && !is_back_insertable<Container>::value>>
{
    using type = std::insert_iterator<Container>;

    static type from(Container& c)
    {
        return std::inserter(c, c.end());
    }
};

template <class Container>
using back_insert_iterator_t = typename back_insert_iterator<Container>::type;

}} // end detail::scanner

template <class Container, class... Appendices>
auto make_field_translator(Container& values, Appendices&&... appendices)
 -> std::enable_if_t<
        (detail::scanner::is_convertible_numeric_type<
            typename Container::value_type>::value
      || detail::is_std_string<typename Container::value_type>::value)
     && (detail::scanner::is_back_insertable<Container>::value
      || detail::scanner::is_insertable<Container>::value),
        decltype(
            detail::scanner::make_field_translator_na<
                    typename Container::value_type>(
                std::declval<detail::scanner::
                    back_insert_iterator_t<Container>>(),
                std::forward<Appendices>(appendices)...))>
{
    return make_field_translator<typename Container::value_type>(
        detail::scanner::back_insert_iterator<Container>::from(values),
        std::forward<Appendices>(appendices)...);
}

template <class Allocator, class Container, class... Appendices>
auto make_field_translator(std::allocator_arg_t, const Allocator& alloc,
    Container& values, Appendices&&... appendices)
 -> std::enable_if_t<
        detail::is_std_string<typename Container::value_type>::value
     && (detail::scanner::is_back_insertable<Container>::value
      || detail::scanner::is_insertable<Container>::value),
        string_field_translator<
            detail::scanner::back_insert_iterator_t<Container>,
            typename Container::value_type::value_type,
            typename Container::value_type::traits_type,
            Allocator, Appendices...>>
{
    return make_field_translator<typename Container::value_type>(
        std::allocator_arg, alloc,
        detail::scanner::back_insert_iterator<Container>::from(values),
        std::forward<Appendices>(appendices)...);
}

}

#endif
