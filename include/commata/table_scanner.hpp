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
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <iomanip>
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
#include "text_error.hpp"
#include "member_like_base.hpp"
#include "typing_aid.hpp"

namespace commata {

namespace detail {

struct accepts_range_impl
{
    template <class T, class Ch>
    static auto check(T*) -> decltype(
        std::declval<T&>().field_value(
            static_cast<Ch*>(nullptr), static_cast<Ch*>(nullptr)),
        std::true_type());

    template <class T, class Ch>
    static auto check(...) -> std::false_type;
};

template <class T, class Ch>
struct accepts_range :
    decltype(accepts_range_impl::check<T, Ch>(nullptr))
{};

struct accepts_x_impl
{
    template <class T, class X>
    static auto check(T*) -> decltype(
        std::declval<T&>().field_value(std::declval<X>()),
        std::true_type());

    template <class T, class Ch>
    static auto check(...) -> std::false_type;
};

template <class T, class X>
struct accepts_x :
    decltype(accepts_x_impl::check<T, X>(nullptr))
{};

// We made this class template to give nothrow-move-constructibility to
// std::vector, so we shall be able to remove this in C++17, in which it has
// a noexcept move ctor
template <class T, class = void>
class nothrow_move_constructible
{
    T* t_;

public:
    template <class AnyAllocator, class... Args>
    explicit nothrow_move_constructible(
        std::allocator_arg_t, const AnyAllocator& any_alloc, Args&&... args)
    {
        using at_t = typename std::allocator_traits<AnyAllocator>::
            template rebind_traits<T>;
        typename at_t::allocator_type alloc(any_alloc);
        const auto p = at_t::allocate(alloc, 1);
        t_ = std::addressof(*p);
        try {
            ::new(t_) T(std::forward<Args>(args)...);
        } catch (...) {
            at_t::deallocate(alloc, p, 1);
            throw;
        }
    }

    nothrow_move_constructible(nothrow_move_constructible&& other) noexcept :
        t_(other.t_)
    {
        other.t_ = nullptr;
    }

    ~nothrow_move_constructible()
    {
        assert(!t_);
    }

    explicit operator bool() const
    {
        return t_ != nullptr;
    }

    template <class AnyAllocator>
    void kill(const AnyAllocator& any_alloc)
    {
        assert(t_);
        using at_t = typename std::allocator_traits<AnyAllocator>::
            template rebind_traits<T>;
        typename at_t::allocator_type alloc(any_alloc);
        t_->~T();
        at_t::deallocate(alloc,
            std::pointer_traits<typename at_t::pointer>::pointer_to(*t_), 1);
#ifndef NDEBUG
        t_ = nullptr;
#endif
    }

    T& operator*()
    {
        assert(t_);
        return *t_;
    }

    const T& operator*() const
    {
        assert(t_);
        return *t_;
    }

    T* operator->()
    {
        assert(t_);
        return t_;
    }

    const T* operator->() const
    {
        assert(t_);
        return t_;
    }

    template <class AnyAllocator>
    void assign(const AnyAllocator& any_alloc,
        nothrow_move_constructible&& other) noexcept
    {
        kill(any_alloc);
        t_ = other.t_;
        other.t_ = nullptr;
    }
};

template <class T>
class nothrow_move_constructible<T,
    std::enable_if_t<std::is_nothrow_move_constructible<T>::value>>
{
    std::aligned_storage_t<sizeof(T), alignof(T)> t_;

public:
    template <class AnyAllocator, class... Args>
    explicit nothrow_move_constructible(
        std::allocator_arg_t, const AnyAllocator&, Args&&... args)
    {
        ::new(&*(*this)) T(std::forward<Args>(args)...);
    }

    nothrow_move_constructible(nothrow_move_constructible&& other) noexcept :
        t_(std::move(other.t_))
    {
        ::new(&*(*this)) T(std::move(*other));
        other->clear(); // inhibit overkill
    }

    ~nothrow_move_constructible()
    {
        (*this)->~T();
    }

    constexpr explicit operator bool() const
    {
        return true;
    }

    template <class AnyAllocator>
    void kill(const AnyAllocator&)
    {}

    T& operator*()
    {
        return *reinterpret_cast<T*>(&t_);
    }

    const T& operator*() const
    {
        return *reinterpret_cast<const T*>(&t_);
    }

    T* operator->()
    {
        return &*(*this);
    }

    const T* operator->() const
    {
        return &*(*this);
    }

    template <class AnyAllocator>
    void assign(const AnyAllocator&,
        nothrow_move_constructible&& other) noexcept
    {
        (*this)->~T();
        ::new(&*(*this)) T(std::move(*other));
        other->clear(); // inhibit overkill
    }
};

} // end namespace detail

template <class Ch, class Tr = std::char_traits<Ch>,
          class Allocator = std::allocator<Ch>>
class basic_table_scanner
{
    using string_t = std::basic_string<Ch, Tr, Allocator>;

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
    class typed_header_field_scanner : public header_field_scanner
    {
        HeaderScanner scanner_;

        using range_t = std::pair<Ch*, Ch*>;

    public:
        explicit typed_header_field_scanner(HeaderScanner s) :
            scanner_(std::move(s))
        {}

        void field_value(Ch* begin, Ch* end, basic_table_scanner& me) override
        {
            const range_t range(begin, end);
            if (!scanner_(me.j_, &range, me)) {
                me.remove_header_field_scanner();
            }
        }

        void field_value(string_t&& value, basic_table_scanner& me) override
        {
            field_value(&value[0], &value[0] + value.size(), me);
        }

        void so_much_for_header(basic_table_scanner& me) override
        {
            if (!scanner_(me.j_, static_cast<const range_t*>(nullptr), me)) {
                me.remove_header_field_scanner();
            }
        }
    };

    struct body_field_scanner : field_scanner, typable
    {
        virtual void field_skipped() = 0;
    };

    template <class FieldScanner>
    class typed_body_field_scanner : public body_field_scanner
    {
        FieldScanner scanner_;

    public:
        explicit typed_body_field_scanner(FieldScanner s) :
            scanner_(std::move(s))
        {}

        void field_value(Ch* begin, Ch* end, basic_table_scanner& me) override
        {
            field_value_r(
                typename detail::accepts_range<
                    std::remove_reference_t<decltype(scanner())>, Ch>(),
                begin, end, me);
        }

        void field_value(string_t&& value, basic_table_scanner& me) override
        {
            field_value_s(
                typename detail::accepts_x<
                    std::remove_reference_t<decltype(scanner())>, string_t>(),
                std::move(value), me);
        }

        void field_skipped() override
        {
            scanner().field_skipped();
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
            scanner().field_value(begin, end);
        }

        void field_value_r(std::false_type,
            Ch* begin, Ch* end, basic_table_scanner& me)
        {
            scanner().field_value(string_t(begin, end, me.get_allocator()));
        }

        void field_value_s(std::true_type,
            string_t&& value, basic_table_scanner&)
        {
            scanner().field_value(std::move(value));
        }

        void field_value_s(std::false_type,
            string_t&& value, basic_table_scanner&)
        {
            scanner().field_value(
                &*value.begin(), &*value.begin() + value.size());
        }

        const void* get_target_v() const noexcept override
        {
            return &scanner_;
        }

        void* get_target_v() noexcept override
        {
            return &scanner_;
        }

        decltype(auto) scanner() noexcept
        {
            return scanner_impl(
                detail::is_std_reference_wrapper<FieldScanner>());
        }

        decltype(auto) scanner_impl(std::false_type) noexcept
        {
            return static_cast<FieldScanner&>(scanner_);
        }

        decltype(auto) scanner_impl(std::true_type) noexcept
        {
            return static_cast<typename FieldScanner::type&>(scanner_.get());
        }
    };

    struct record_end_scanner : typable
    {
        virtual void end_record() = 0;
    };

    template <class T>
    class typed_record_end_scanner : public record_end_scanner
    {
        T scanner_;

    public:
        explicit typed_record_end_scanner(T scanner) :
            scanner_(std::move(scanner))
        {}

        void end_record() override
        {
            scanner_();
        }

        const std::type_info& get_type() const noexcept override
        {
            return typeid(T);
        }

    private:
        const void* get_target_v() const noexcept override
        {
            return &scanner_;
        }

        void* get_target_v() noexcept override
        {
            return &scanner_;
        }
    };

    using at_t = std::allocator_traits<Allocator>;
    using hfs_at_t =
        typename at_t::template rebind_traits<header_field_scanner>;
    using bfs_at_t =
        typename at_t::template rebind_traits<body_field_scanner>;
    using bfs_ptr_t = typename bfs_at_t::pointer;
    using bfs_ptr_a_t = typename at_t::template rebind_alloc<bfs_ptr_t>;
    using scanners_a_t = detail::allocation_only_allocator<bfs_ptr_a_t>;
    using res_at_t =
        typename at_t::template rebind_traits<record_end_scanner>;

    std::size_t remaining_header_records_;
    std::size_t j_;
    std::size_t buffer_size_;
    typename at_t::pointer buffer_;
    const Ch* begin_;
    const Ch* end_;
    string_t fragmented_value_;
    typename hfs_at_t::pointer header_field_scanner_;
    detail::nothrow_move_constructible<
        std::vector<bfs_ptr_t, scanners_a_t>> scanners_;
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

    template <
        class HeaderFieldScanner,
        class = std::enable_if_t<!std::is_integral<HeaderFieldScanner>::value>>
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
        remaining_header_records_(header_record_count), j_(0),
        buffer_size_(sanitize_buffer_size(buffer_size)),
        buffer_(), begin_(nullptr), fragmented_value_(alloc),
        header_field_scanner_(), scanners_(make_scanners()),
        end_scanner_(nullptr)
    {}

    template <
        class HeaderFieldScanner,
        class = std::enable_if_t<!std::is_integral<HeaderFieldScanner>::value>>
    basic_table_scanner(
        std::allocator_arg_t, const Allocator& alloc,
        HeaderFieldScanner s,
        size_type buffer_size = 0U) :
        remaining_header_records_(0U), j_(0),
        buffer_size_(sanitize_buffer_size(buffer_size)),
        buffer_(), begin_(nullptr), fragmented_value_(alloc),
        header_field_scanner_(allocate_construct<
            typed_header_field_scanner<HeaderFieldScanner>>(std::move(s))),
        scanners_(make_scanners()), end_scanner_(nullptr)
    {}

    basic_table_scanner(basic_table_scanner&& other) noexcept :
        remaining_header_records_(other.remaining_header_records_),
        j_(other.j_), buffer_size_(other.buffer_size_),
        buffer_(other.buffer_), begin_(other.begin_), end_(other.end_),
        fragmented_value_(std::move(other.fragmented_value_)),
        header_field_scanner_(other.header_field_scanner_),
        scanners_(std::move(other.scanners_)),
        end_scanner_(other.end_scanner_)
    {
        other.buffer_ = nullptr;
        other.header_field_scanner_ = nullptr;
        other.end_scanner_ = nullptr;
    }

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
            for (const auto p : *scanners_) {
                if (p) {
                    destroy_deallocate(p);
                }
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
    void set_field_scanner(std::size_t j, FieldScanner s = FieldScanner())
    {
        do_set_field_scanner(j, std::move(s));
    }

private:
    template <class FieldScanner>
    void do_set_field_scanner(std::size_t j, FieldScanner s)
    {
        using scanner_t = typed_body_field_scanner<FieldScanner>;
        if (!scanners_) {
            scanners_.assign(get_allocator(), make_scanners());     // throw
            scanners_->resize(j + 1);                               // throw
        } else if (j >= scanners_->size()) {
            scanners_->resize(j + 1);                               // throw
        }
        const auto p = allocate_construct<scanner_t>(std::move(s)); // throw
        if (const auto scanner = (*scanners_)[j]) {
            destroy_deallocate(scanner);
        }
        (*scanners_)[j] = p;
    }

    void do_set_field_scanner(std::size_t j, std::nullptr_t)
    {
        if (scanners_ && (j < scanners_->size())) {
            auto& scanner = (*scanners_)[j];
            destroy_deallocate(scanner);
            scanner = nullptr;
        }
    }

public:
    const std::type_info& get_field_scanner_type(std::size_t j) const noexcept
    {
        if (scanners_ && (j < scanners_->size()) && (*scanners_)[j]) {
            return (*scanners_)[j]->get_type();
        } else {
            return typeid(void);
        }
    }

    bool has_field_scanner(std::size_t j) const noexcept
    {
        return scanners_ && (j < scanners_->size()) && (*scanners_)[j];
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
    static auto get_field_scanner_g(ThisType& me, std::size_t j) noexcept
    {
        return me.has_field_scanner(j) ?
            (*me.scanners_)[j]->template get_target<FieldScanner>() :
            nullptr;
    }

public:
    template <class RecordEndScanner = std::nullptr_t>
    void set_record_end_scanner(RecordEndScanner s = RecordEndScanner())
    {
        do_set_record_end_scanner(std::move(s));
    }

private:
    template <class RecordEndScanner>
    void do_set_record_end_scanner(RecordEndScanner s)
    {
        using scanner_t = typed_record_end_scanner<RecordEndScanner>;
        end_scanner_ = allocate_construct<scanner_t>(std::move(s)); // throw
    }

    void do_set_record_end_scanner(std::nullptr_t)
    {
        destroy_deallocate(end_scanner_);
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
    {}

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
        ++j_;
    }

    void end_record(const Ch* /*record_end*/)
    {
        if (header_field_scanner_) {
            header_field_scanner_->so_much_for_header(*this);
        } else if (remaining_header_records_ > 0) {
            --remaining_header_records_;
        } else {
            if (scanners_) {
                for (auto j = j_; j < scanners_->size(); ++j) {
                    if (auto scanner = (*scanners_)[j]) {
                        scanner->field_skipped();
                    }
                }
            }
            if (end_scanner_) {
                end_scanner_->end_record();
            }
        }
        j_ = 0;
    }

private:
    static constexpr size_type default_buffer_size =
        std::min(std::numeric_limits<size_type>::max(),
            static_cast<size_type>(8192U));

    size_type sanitize_buffer_size(size_type buffer_size)
    {
        if (buffer_size == 0U) {
            buffer_size = default_buffer_size;
        }
        return std::min(
                std::max(buffer_size, static_cast<size_type>(2U)),
                at_t::max_size(get_allocator()));
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

    template <class T>
    void destroy_deallocate(T* p)
    {
        assert(p);
        using t_at_t = typename at_t::template rebind_traits<T>;
        typename t_at_t::allocator_type a(get_allocator());
        std::addressof(*p)->~T();
        t_at_t::deallocate(a, p, 1);
    }

    auto make_scanners()
    {
        const auto a = get_allocator();
        return decltype(scanners_)(std::allocator_arg, a,
            scanners_a_t(bfs_ptr_a_t(a)));
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
        } else if ((remaining_header_records_ == 0U)
                && scanners_ && (j_ < scanners_->size())) {
            if (const auto p = (*scanners_)[j_]) {
                return std::addressof(*p);
            }
        }
        return nullptr;
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

// In C++17, we can abolish this template and use std::optional instead
template <class T>
class replacement
{
    bool has_;
    std::aligned_storage_t<sizeof(T), alignof(T)> store_;

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

    explicit replacement(const T& t) :
        has_(true)
    {
        ::new(reinterpret_cast<T*>(&store_)) T(t);
    }

    explicit replacement(T&& t)
        noexcept(std::is_nothrow_move_constructible<T>::value) :
        has_(true)
    {
        ::new(reinterpret_cast<T*>(&store_)) T(std::move(t));
    }

    replacement(replacement&& other)
        noexcept(std::is_nothrow_move_constructible<T>::value) :
        has_(other.has_)
    {
        if (const auto p = other.get()) {
            ::new(reinterpret_cast<T*>(&store_)) T(std::move(*p));
            p->~T();
            other.has_ = false;
        }
    }

    template <class U>
    replacement(replacement<U>&& other)
        noexcept(std::is_nothrow_constructible<T, U&&>::value) :
        has_(other.has_)
    {
        if (const auto p = other.get()) {
            ::new(reinterpret_cast<T*>(&store_)) T(std::move(*p));
            p->~U();
            other.has_ = false;
        }
    }

    ~replacement()
    {
        if (has_) {
            reinterpret_cast<T*>(&store_)->~T();
        }
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
        return reinterpret_cast<const T*>(&store_);
    }

    T* operator->()
    {
        assert(has_);
        return reinterpret_cast<T*>(&store_);
    }
};

namespace detail {

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
};

template <>
struct numeric_type_traits<double>
{
    static constexpr const char* name = "double";
    static constexpr const auto strto = std::strtod;
    static constexpr const auto wcsto = std::wcstod;
};

template <>
struct numeric_type_traits<long double>
{
    static constexpr const char* name = "long double";
    static constexpr const auto strto = std::strtold;
    static constexpr const auto wcsto = std::wcstold;
};

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
            using limits_t =
                std::numeric_limits<std::remove_const_t<decltype(r)>>;
            const int s = (r == limits_t::max()) ? 1 :
                          (r == limits_t::lowest()) ? -1 : 0;
            return ret_t(this->get().out_of_range(begin, end, s));
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
};

template <class Ch>
struct has_simple_invalid_format_impl
{
    template <class F>
    static auto check(F*) -> decltype(
        std::declval<F&>().invalid_format(
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
        std::declval<F&>().out_of_range(
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
        std::declval<F&>().empty(),
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
        return h.invalid_format(begin, end);
    }

    template <class H, class Ch,
        std::enable_if_t<!has_simple_invalid_format<H, Ch>::value>* = nullptr>
    static replacement<T> invalid_format(H& h, const Ch* begin, const Ch* end)
    {
        return h.invalid_format(begin, end, static_cast<T*>(nullptr));
    }

    template <class H, class Ch,
        std::enable_if_t<has_simple_out_of_range<H, Ch>::value>* = nullptr>
    static replacement<T> out_of_range(
        H& h, const Ch* begin, const Ch* end, int sign)
    {
        return h.out_of_range(begin, end, sign);
    }

    template <class H, class Ch,
        std::enable_if_t<!has_simple_out_of_range<H, Ch>::value>* = nullptr>
    static replacement<T> out_of_range(
        H& h, const Ch* begin, const Ch* end, int sign)
    {
        return h.out_of_range(begin, end, sign, static_cast<T*>(nullptr));
    }

    template <class H,
        std::enable_if_t<has_simple_empty<H>::value>* = nullptr>
    static replacement<T> empty(H& h)
    {
        return h.empty();
    }

    template <class H,
        std::enable_if_t<!has_simple_empty<H>::value>* = nullptr>
    static replacement<T> empty(H& h)
    {
        return h.empty(static_cast<T*>(nullptr));
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
    converter(const H& h) :
        raw_converter<T, typed_conversion_error_handler<T, H>>(
            typed_conversion_error_handler<T, H>(h))
    {}

    converter(H&& h) :
        raw_converter<T, typed_conversion_error_handler<T, H>>(
            typed_conversion_error_handler<T, H>(std::move(h)))
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
        if (r <= std::numeric_limits<T>::max()) {
            return replacement<T>(static_cast<T>(r));
        } else {
            const auto s = static_cast<std::make_signed_t<U>>(r);
            if ((s < 0)
             && (static_cast<U>(-s) <= std::numeric_limits<T>::max())) {
                return replacement<T>(static_cast<T>(s));
            }
        }
        return conversion_error_facade<T>::out_of_range(
            this->get_conversion_error_handler(), begin, end, 1);
    }
};

// For types which have corresponding "raw_type"
template <class T, class H>
struct converter<T, H, void_t<typename numeric_type_traits<T>::raw_type>> :
    restrained_converter<T, typed_conversion_error_handler<T, H>,
        typename numeric_type_traits<T>::raw_type>
{
    converter(const H& h) :
        restrained_converter<T, typed_conversion_error_handler<T, H>,
            typename numeric_type_traits<T>::raw_type>(
                typed_conversion_error_handler<T, H>(h))
    {}

    converter(H&& h) :
        restrained_converter<T, typed_conversion_error_handler<T, H>,
            typename numeric_type_traits<T>::raw_type>(
                typed_conversion_error_handler<T, H>(std::move(h)))
    {}
};

template <class T, class = void>
class movable_store
{
    // we choose shared_ptr instead of unique_ptr because it is very
    // difficult and expensive to define "moved-from" states
    std::shared_ptr<T> t_;

public:
    movable_store() : t_(std::make_shared<T>())
    {}

    explicit movable_store(T t) :
        t_(std::make_shared<T>(std::move(t)))
    {}

    movable_store(const movable_store& other) noexcept = default;
    // move ctor is implicitly declared as "deleted" to make "moved-from" state
    // equal to "copied-from" state, that is, no harmfully disturbed state

    const T& operator*() const noexcept
    {
        return *t_;
    }

    T& operator*() noexcept
    {
        return *t_;
    }

    const T* operator->() const noexcept
    {
        return t_.get();
    }

    T* operator->() noexcept
    {
        return t_.get();
    }
};

template <class T>
class movable_store<T,
    std::enable_if_t<std::is_nothrow_move_constructible<T>::value>>
{
    T t_;

public:
    movable_store() : t_(T())
    {}

    explicit movable_store(T t) noexcept : t_(std::move(t))
    {}

    movable_store(const movable_store&) = default;
    movable_store(movable_store&&) noexcept = default;

    const T& operator*() const noexcept
    {
        return t_;
    }

    T& operator*() noexcept
    {
        return t_;
    }

    const T* operator->() const noexcept
    {
        return &t_;
    }

    T* operator->() noexcept
    {
        return &t_;
    }
};

} // end namespace detail

struct replacement_fail_t {};
constexpr replacement_fail_t replacement_fail = replacement_fail_t{};

struct fail_if_skipped
{
    template <class T>
    [[noreturn]]
    replacement<T> operator()(T*) const
    {
        throw field_not_found("This field did not appear in this record");
    }
};

template <class T>
class replace_if_skipped
{
    enum class mode
    {
        replace,
        fail,
        ignore
    };

    mode mode_;
    detail::movable_store<T> default_value_;

public:
    explicit replace_if_skipped(replacement_fail_t) noexcept :
        mode_(mode::fail)
    {}

    explicit replace_if_skipped(replacement_ignore_t) noexcept :
        mode_(mode::ignore)
    {}

    template <class U = T,
        std::enable_if_t<
            !std::is_base_of<replace_if_skipped<T>, std::decay_t<U>>::value
         && !std::is_base_of<replacement_ignore_t, std::decay_t<U>>::value
         && !std::is_base_of<replacement_fail_t, std::decay_t<U>>::value,
            std::nullptr_t> = nullptr>
    replace_if_skipped(U&& default_value = U()) :
        mode_(mode::replace),
        default_value_(std::forward<U>(default_value))
    {}

    replace_if_skipped(const replace_if_skipped&) = default;
    replace_if_skipped(replace_if_skipped&&) noexcept = default;

    replacement<T> operator()() const
    {
        switch (mode_) {
        case mode::replace:
            return replacement<T>(*default_value_);
        case mode::fail:
            fail_if_skipped()(static_cast<T*>(nullptr));
            // fall through
        default:
            return replacement<T>();
        }
    }
};

struct fail_if_conversion_failed
{
    template <class T, class Ch>
    [[noreturn]]
    replacement<T> invalid_format(
        const Ch* begin, const Ch* end, T* = nullptr) const
    {
        assert(*end == Ch());
        std::ostringstream s;
        narrow(s, begin, end);
        s << ": cannot convert";
        write_name<T>(s, " to an instance of ");
        throw field_invalid_format(s.str());
    }

    template <class T, class Ch>
    [[noreturn]]
    replacement<T> out_of_range(
        const Ch* begin, const Ch* end, int, T* = nullptr) const
    {
        assert(*end == Ch());
        std::ostringstream s;
        narrow(s, begin, end);
        s << ": out of range";
        write_name<T>(s, " of ");
        throw field_out_of_range(s.str());
    }

    template <class T>
    [[noreturn]]
    replacement<T> empty(T* = nullptr) const
    {
        std::ostringstream s;
        s << "Cannot convert an empty string";
        write_name<T>(s, " to an instance of ");
        throw field_empty(s.str());
    }

private:
    template <class T>
    static std::ostream& write_name(std::ostream& os, const char* prefix,
        decltype(detail::numeric_type_traits<T>::name)* = nullptr)
    {
        return os << prefix << detail::numeric_type_traits<T>::name;
    }

    template <class T>
    static std::ostream& write_name(std::ostream& os, ...)
    {
        return os;
    }

    template <class Ch>
    static void narrow(std::ostream& os, const Ch* begin, const Ch* end)
    {
        const auto& facet =
            std::use_facet<std::ctype<Ch>>(std::locale());  // current global
        while (begin != end) {
            const auto c = *begin;
            if (c == Ch()) {
                os << '[' << set_hex << setw_char<Ch> << 0 << ']';
            } else if (!facet.is(std::ctype<Ch>::print, c)) {
                os << '[' << set_hex << setw_char<Ch> << (c + 0) << ']';
            } else {
                const auto d = facet.narrow(c, '\0');
                if (d == '\0') {
                    os << '[' << set_hex << setw_char<Ch> << (c + 0) << ']';
                } else {
                    os.rdbuf()->sputc(d);
                }
            }
            ++begin;
        }
    }

    static void narrow(std::ostream& os, const char* begin, const char* end)
    {
        // [begin, end] may be a NTMBS, so we cannot determine whether a char
        // is an unprintable one or not easily
        while (begin != end) {
            const auto c = *begin;
            if (c == '\0') {
                os << '[' << set_hex << setw_char<char> << 0 << ']';
            } else {
                os.rdbuf()->sputc(c);
            }
            ++begin;
        }
    }

    static std::ostream& set_hex(std::ostream& os)
    {
        return os << std::showbase << std::hex << std::setfill('0');
    }

    template <class Ch>
    static std::ostream& setw_char(std::ostream& os)
    {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4127)
#endif
        constexpr auto m =
            std::numeric_limits<std::make_unsigned_t<Ch>>::max();
        if (m <= 0xff) {
            os << std::setw(2);
        } else if (m <= 0xffff) {
            os << std::setw(4);
        } else if (m <= 0xffffffff) {
            os << std::setw(8);
        }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
        return os;
    }
};

template <class T>
class replace_if_conversion_failed
{
    enum class mode
    {
        replace,
        fail,
        ignore
    };

    struct store
    {
        static constexpr unsigned empty = 0;
        static constexpr unsigned invalid_format = 1;
        static constexpr unsigned above_upper_limit = 2;
        static constexpr unsigned below_lower_limit = 3;
        static constexpr unsigned underflow = 4;
        static constexpr unsigned n = 5;

        std::aligned_storage_t<sizeof(T), alignof(T)> replacements_[n];
        std::uint_fast8_t has_;
        std::uint_fast8_t skips_;

        store() noexcept : has_(0U), skips_(0U)
        {}

        store(const store& other)
            noexcept(std::is_trivially_copyable<T>::value) :
            store(other, std::is_trivially_copyable<T>())
        {}

        store(store&& other)
            noexcept(
                std::is_trivially_copyable<T>::value
             || std::is_nothrow_move_constructible<T>::value) :
            store(std::move(other), std::is_trivially_copyable<T>())
        {}

    private:
        store(const store& other, std::true_type)
            noexcept :
            has_(other.has_), skips_(other.skips_)
        {
            std::memcpy(&replacements_, &other.replacements_,
                sizeof replacements_);
        }

        template <class Other>
        store(Other&& other, std::false_type) :
            has_(0U), skips_(0U)
        {
            using f_t = std::conditional_t<
                std::is_lvalue_reference<Other>::value, const T&, T>;
            for (unsigned r = 0; r < n; ++r) {
                const auto p = other.get(r);
                if (p.first == mode::replace) {
                    init(r, std::forward<f_t>(*p.second));
                }
            }
        }

    public:
        ~store()
        {
            destroy(std::is_trivially_destructible<T>());
        }

        template <class U, std::enable_if_t<
                (!std::is_base_of<
                    replacement_fail_t, std::decay_t<U>>::value)
             && (!std::is_base_of<
                    replacement_ignore_t, std::decay_t<U>>::value),
            std::nullptr_t> = nullptr>
        void init(unsigned r, U&& value)
        {
            assert(!has(r));
            assert(!skips(r));
            ::new(replacements_ + r) T(std::forward<U>(value));
            has_ |= 1U << r;
        }

        void init(unsigned r, replacement_fail_t) noexcept
        {
            static_cast<void>(r);
            assert(!has(r));
            assert(!skips(r));
        }

        void init(unsigned r, replacement_ignore_t) noexcept
        {
            assert(!has(r));
            assert(!skips(r));
            skips_ |= 1U << r;
        }

        std::pair<mode, const T*> get(unsigned r) const noexcept
        {
            using pair_t = std::pair<mode, const T*>;
            if (has(r)) {
                return pair_t(mode::replace,
                    reinterpret_cast<const T*>(replacements_ + r));
            } else if (skips(r)) {
                return pair_t(mode::ignore, nullptr);
            } else {
                return pair_t(mode::fail, nullptr);
            }
        }

    private:
        std::pair<mode, T*> get(unsigned r) noexcept
        {
            using pair_t = std::pair<mode, T*>;
            if (has(r)) {
                return pair_t(mode::replace,
                    reinterpret_cast<T*>(replacements_ + r));
            } else if (skips(r)) {
                return pair_t(mode::ignore, nullptr);
            } else {
                return pair_t(mode::fail, nullptr);
            }
        }

        unsigned has(unsigned r) const noexcept
        {
            return has_ & (1U << r);
        }

        unsigned skips(unsigned r) const noexcept
        {
            return skips_ & (1U << r);
        }

        void destroy(std::true_type) noexcept
        {}

        void destroy(std::false_type) noexcept
        {
            for (unsigned r = 0; r < n; ++r) {
                if (has(r)) {
                    reinterpret_cast<const T*>(replacements_ + r)->~T();
                }
            }
        }
    };

    detail::movable_store<store> store_;

public:
    template <class Empty = T, class InvalidFormat = T,
        class AboveUpperLimit = T, class BelowLowerLimit = T,
        class Underflow = T,
        std::enable_if_t<
            !std::is_base_of<
                replace_if_conversion_failed<T>, std::decay_t<Empty>>::value,
            std::nullptr_t> = nullptr>
    explicit replace_if_conversion_failed(
        Empty&& on_empty = Empty(),
        InvalidFormat&& on_invalid_format = InvalidFormat(),
        AboveUpperLimit&& on_above_upper_limit = AboveUpperLimit(),
        BelowLowerLimit&& on_below_lower_limit = BelowLowerLimit(),
        Underflow&& on_underflow = Underflow())
    {
        store_->init(store::empty,
            std::forward<Empty>(on_empty));
        store_->init(store::invalid_format,
            std::forward<InvalidFormat>(on_invalid_format));
        store_->init(store::above_upper_limit,
            std::forward<AboveUpperLimit>(on_above_upper_limit));
        store_->init(store::below_lower_limit,
            std::forward<BelowLowerLimit>(on_below_lower_limit));
        store_->init(store::underflow,
            std::forward<Underflow>(on_underflow));
    }

    replace_if_conversion_failed(const replace_if_conversion_failed&)
        = default;
    replace_if_conversion_failed(replace_if_conversion_failed&&) noexcept
        = default;
    ~replace_if_conversion_failed() = default;

    template <class Ch>
    replacement<T> invalid_format(const Ch* begin, const Ch* end) const
    {
        return unwrap(store_->get(store::invalid_format),
            [begin, end]() {
                fail_if_conversion_failed().invalid_format<T>(begin, end);
            });
    }

    template <class Ch>
    replacement<T> out_of_range(const Ch* begin, const Ch* end, int sign) const
    {
        const auto fail = [begin, end, sign]() {
            fail_if_conversion_failed().out_of_range<T>(begin, end, sign);
        };
        if (sign > 0) {
            return unwrap(store_->get(store::above_upper_limit), fail);
        } else if (sign < 0) {
            return unwrap(store_->get(store::below_lower_limit), fail);
        } else {
            return unwrap(store_->get(store::underflow), fail);
        }
    }

    replacement<T> empty() const
    {
        return unwrap(store_->get(store::empty),
            []() {
                fail_if_conversion_failed().empty<T>();
            });
    }

private:
    template <class Fail>
    auto unwrap(const std::pair<mode, const T*>& p, Fail fail) const
    {
        switch (p.first) {
        case mode::replace:
            return replacement<T>(*p.second);
        case mode::ignore:
            return replacement<T>();
        case mode::fail:
        default:
            break;
        }
        fail();
        assert(false);
        return replacement<T>();
    }
};

namespace detail {

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
    translator(Sink&& sink, SkippingHandler&& handle_skipping) :
        member_like_base<SkippingHandler>(std::move(handle_skipping)),
        sink_(std::move(sink))
    {}

    // VS2015 needs this ctor. I don't know why.
    translator(translator&& other) :
        member_like_base<SkippingHandler>(std::move(other.get())),
        sink_(std::move(other.sink_))
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
        if (const auto p = call_skipping_handler(
                has_simple_call<SkippingHandler>()).get()) {
            put(std::move(*p));
        }
    }

private:
    auto call_skipping_handler(std::true_type) {
        return get_skipping_handler()();
    }

    auto call_skipping_handler(std::false_type) {
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

} // end namespace detail

template <class T, class Sink,
    class SkippingHandler = fail_if_skipped,
    class ConversionErrorHandler = fail_if_conversion_failed>
class arithmetic_field_translator
{
    using converter_t = detail::converter<T, ConversionErrorHandler>;
    using translator_t = detail::translator<T, Sink, SkippingHandler>;

    detail::base_member_pair<converter_t, translator_t> ct_;

public:
    explicit arithmetic_field_translator(
        Sink sink,
        SkippingHandler handle_skipping = SkippingHandler(),
        ConversionErrorHandler handle_conversion_error
            = ConversionErrorHandler()) :
        ct_(converter_t(std::move(handle_conversion_error)),
            translator_t(std::move(sink), std::move(handle_skipping)))
    {}

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

    void field_skipped()
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
    void field_value(const Ch* begin, const Ch* end)
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
    arithmetic_field_translator<
        T, Sink, SkippingHandler, ConversionErrorHandler> out_;
    std::locale loc_;

    // These are initialized after parsing has started
    wchar_t thousands_sep_;     // of specified loc in the ctor
    wchar_t decimal_point_;     // of specified loc in the ctor
    wchar_t decimal_point_c_;   // of C's global
                                // to mimic std::strtol and its comrades

public:
    locale_based_arithmetic_field_translator(
        Sink sink, const std::locale& loc,
        SkippingHandler handle_skipping = SkippingHandler(),
        ConversionErrorHandler handle_conversion_error
            = ConversionErrorHandler()) :
        out_(std::move(sink), std::move(handle_skipping),
            std::move(handle_conversion_error)),
        loc_(loc), decimal_point_c_()
    {}

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

    void field_skipped()
    {
        out_.field_skipped();
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
    void field_value(Ch* begin, Ch* end)
    {
        if (decimal_point_c_ == wchar_t()) {
            decimal_point_c_ = widen(*std::localeconv()->decimal_point, Ch());

            const auto& facet = std::use_facet<std::numpunct<Ch>>(loc_);
            thousands_sep_ = facet.grouping().empty() ?
                Ch() : facet.thousands_sep();
            decimal_point_ = facet.decimal_point();
        }
        assert(*end == Ch());   // shall be null-terminated
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
        out_.field_value(begin, head);
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
        detail::translator<std::basic_string<Ch, Tr, Allocator>,
        Sink, SkippingHandler>;

    detail::base_member_pair<Allocator, translator_t> at_;

public:
    using allocator_type = Allocator;

    explicit string_field_translator(
        Sink sink,
        SkippingHandler handle_skipping = SkippingHandler()) :
        at_(Allocator(),
            translator_t(std::move(sink), std::move(handle_skipping)))
    {}

    string_field_translator(
        std::allocator_arg_t, const Allocator& alloc, Sink sink,
        SkippingHandler handle_skipping = SkippingHandler()) :
        at_(alloc, translator_t(std::move(sink), std::move(handle_skipping)))
    {}

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

    void field_skipped()
    {
        at_.member().field_skipped();
    }

    void field_value(const Ch* begin, const Ch* end)
    {
        at_.member().put(std::basic_string<Ch, Tr, Allocator>(
            begin, end, get_allocator()));
    }

    void field_value(std::basic_string<Ch, Tr, Allocator>&& value)
    {
        // std::basic_string which comes with gcc 7.3.1 does not seem to have
        // "move-with-specified-allocator" ctor
        if (value.get_allocator() == get_allocator()) {
            at_.member().put(std::move(value));
        } else {
            field_value(value.c_str(), value.c_str() + value.size());
        }
    }
};

namespace detail {

template <class... Ts>
struct first;

template <>
struct first<>
{
    using type = void;
};

template <class Head, class... Tail>
struct first<Head, Tail...>
{
    using type = Head;
};

template <class... Ts>
using first_t = typename first<Ts...>::type;

template <class T, class Sink, class... Appendices,
    std::enable_if_t<
        !detail::is_std_string<T>::value
     && !std::is_base_of<
            std::locale, std::decay_t<detail::first_t<Appendices...>>>::value,
        std::nullptr_t> = nullptr>
arithmetic_field_translator<T, Sink, Appendices...>
    make_field_translator_na(Sink sink, Appendices&&... appendices);

template <class T, class Sink, class... Appendices,
    std::enable_if_t<!detail::is_std_string<T>::value, std::nullptr_t>
        = nullptr>
locale_based_arithmetic_field_translator<T, Sink, Appendices...>
    make_field_translator_na(
        Sink sink, const std::locale& loc, Appendices&&... appendices);

template <class T, class Sink, class... Appendices,
    std::enable_if_t<detail::is_std_string<T>::value, std::nullptr_t>
        = nullptr>
string_field_translator<Sink,
        typename T::value_type, typename T::traits_type,
        typename T::allocator_type, Appendices...>
    make_field_translator_na(Sink sink, Appendices&&... appendices);

template <class A>
struct is_callable_impl
{
    template <class F>
    static auto check(F*) -> decltype(
        std::declval<F>()(std::declval<A>()),
        std::true_type());

    template <class F>
    static auto check(...) -> std::false_type;
};

template <class F, class A>
struct is_callable :
    decltype(is_callable_impl<A>::template check<F>(nullptr))
{};

} // end namespace detail

template <class T, class Sink, class... Appendices>
auto make_field_translator(Sink sink, Appendices&&... appendices)
 -> std::enable_if_t<
        detail::is_output_iterator<Sink>::value
     || detail::is_callable<Sink, T>::value,
        decltype(detail::make_field_translator_na<T>(
            std::move(sink), std::forward<Appendices>(appendices)...))>
{
    using t = decltype(detail::make_field_translator_na<T>(
        std::move(sink), std::forward<Appendices>(appendices)...));
    return t(std::move(sink), std::forward<Appendices>(appendices)...);
}

template <class T, class Allocator, class Sink, class... Appendices>
auto make_field_translator(std::allocator_arg_t, const Allocator& alloc,
    Sink sink, Appendices&&... appendices)
 -> std::enable_if_t<
        detail::is_output_iterator<Sink>::value
     || detail::is_callable<Sink, T>::value,
        string_field_translator<Sink,
            typename T::value_type, typename T::traits_type,
            Allocator, Appendices...>>
{
    return string_field_translator<Sink,
            typename T::value_type, typename T::traits_type,
            Allocator, Appendices...>(
        std::allocator_arg, alloc,
        std::move(sink), std::forward<Appendices>(appendices)...);
}

namespace detail {

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

} // end namespace detail

template <class Container, class... Appendices>
auto make_field_translator(Container& values, Appendices&&... appendices)
 -> std::enable_if_t<
        detail::is_insertable<Container>::value,
        decltype(
            detail::make_field_translator_na<typename Container::value_type>(
                std::declval<detail::back_insert_iterator_t<Container>>(),
                std::forward<Appendices>(appendices)...))>
{
    return make_field_translator<typename Container::value_type>(
        detail::back_insert_iterator<Container>::from(values),
        std::forward<Appendices>(appendices)...);
}

template <class Allocator, class Container, class... Appendices>
auto make_field_translator(std::allocator_arg_t, const Allocator& alloc,
    Container& values, Appendices&&... appendices)
 -> std::enable_if_t<
        detail::is_insertable<Container>::value,
        string_field_translator<
            detail::back_insert_iterator_t<Container>,
            typename Container::value_type::value_type,
            typename Container::value_type::traits_type,
            Allocator, Appendices...>>
{
    return make_field_translator<typename Container::value_type>(
        std::allocator_arg, alloc,
        detail::back_insert_iterator<Container>::from(values),
        std::forward<Appendices>(appendices)...);
}

}

#endif