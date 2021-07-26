/**
 * These codes are licensed under the Unlicense.
 * http://unlicense.org
 */

#ifndef COMMATA_GUARD_43D98002_9D9B_407A_9017_9020D03E7A46
#define COMMATA_GUARD_43D98002_9D9B_407A_9017_9020D03E7A46

#include <cstddef>
#include <memory>
#include <type_traits>

#include "buffer_size.hpp"
#include "handler_decorator.hpp"
#include "member_like_base.hpp"

namespace commata { namespace detail {

template <class T>
struct is_with_buffer_control :
    std::integral_constant<bool,
        has_get_buffer<T>::value && has_release_buffer<T>::value>
{};

template <class T>
struct is_without_buffer_control :
    std::integral_constant<bool,
        (!has_get_buffer<T>::value) && (!has_release_buffer<T>::value)>
{};

template <class Allocator>
class default_buffer_control :
    detail::member_like_base<Allocator>
{
    using alloc_traits_t = std::allocator_traits<Allocator>;

    std::size_t buffer_size_;
    typename alloc_traits_t::pointer buffer_;

public:
    default_buffer_control(
        std::size_t buffer_size, const Allocator& alloc) noexcept :
        detail::member_like_base<Allocator>(alloc),
        buffer_size_(detail::sanitize_buffer_size(buffer_size, this->get())),
        buffer_()
    {}

    default_buffer_control(default_buffer_control&& other) noexcept :
        detail::member_like_base<Allocator>(std::move(other.get())),
        buffer_size_(other.buffer_size_),
        buffer_(other.buffer_)
    {
        other.buffer_ = nullptr;
    }

    ~default_buffer_control()
    {
        if (buffer_) {
            alloc_traits_t::deallocate(this->get(), buffer_, buffer_size_);
        }
    }

    std::pair<typename alloc_traits_t::value_type*, std::size_t>
        do_get_buffer(...)
    {
        if (!buffer_) {
            buffer_ = alloc_traits_t::allocate(
                this->get(), buffer_size_);     // throw
        }
        return std::make_pair(std::addressof(*buffer_), buffer_size_);
    }

    void do_release_buffer(...) noexcept
    {}
};

struct thru_buffer_control
{
    template <class Handler>
    std::pair<typename Handler::char_type*, std::size_t> do_get_buffer(
        Handler* f)
    {
        return f->get_buffer(); // throw
    }

    template <class Handler>
    void do_release_buffer(
        const typename Handler::char_type* buffer, Handler* f)
    {
        return f->release_buffer(buffer);
    }
};

template <class Handler>
struct is_full_fledged :
    std::integral_constant<bool,
        is_with_buffer_control<Handler>::value
     && has_start_buffer<Handler>::value && has_end_buffer<Handler>::value
     && has_empty_physical_line<Handler>::value>
{};

// noexcept-ness of the member functions except the ctor and the dtor does not
// count because they are invoked as parts of a willingly-throwing operation,
// so we do not specify "noexcept" to the member functions except the ctor and
// the dtor
template <class Handler, class BufferControl>
class full_fledged_handler :
    BufferControl
{
    static_assert(!std::is_reference<Handler>::value, "");
    static_assert(!is_full_fledged<Handler>::value, "");

    Handler handler_;

public:
    using char_type = typename Handler::char_type;

    template <class HandlerR,
        std::enable_if_t<
            std::is_same<Handler, std::decay_t<HandlerR>>::value,
            std::nullptr_t> = nullptr>
    explicit full_fledged_handler(HandlerR&& handler,
        BufferControl&& buffer_engine = BufferControl())
        noexcept(std::is_nothrow_constructible<Handler, HandlerR&&>::value) :
        BufferControl(std::move(buffer_engine)),
        handler_(std::move(handler))
    {}

    std::pair<char_type*, std::size_t> get_buffer()
    {
        return this->do_get_buffer(std::addressof(handler_));
    }

    void release_buffer(const char_type* buffer)
    {
        this->do_release_buffer(buffer, std::addressof(handler_));
    }

    void start_buffer(
        const char_type* buffer_begin, const char_type* buffer_end)
    {
        start_buffer(has_start_buffer<Handler>(), buffer_begin, buffer_end);
    }

    void end_buffer(const char_type* buffer_end)
    {
        end_buffer(has_end_buffer<Handler>(), buffer_end);
    }

    auto start_record(const char_type* record_begin)
    {
        return handler_.start_record(record_begin);
    }

    auto update(const char_type* first, const char_type* last)
    {
        return handler_.update(first, last);
    }

    auto finalize(const char_type* first, const char_type* last)
    {
        return handler_.finalize(first, last);
    }

    auto end_record(const char_type* end)
    {
        return handler_.end_record(end);
    }

    auto empty_physical_line(const char_type* where)
    {
        return empty_physical_line(has_empty_physical_line<Handler>(), where);
    }

private:
    void start_buffer(std::true_type,
        const char_type* buffer_begin, const char_type* buffer_end)
    {
        handler_.start_buffer(buffer_begin, buffer_end);
    }

    void start_buffer(std::false_type, ...)
    {}

    void end_buffer(std::true_type, const char_type* buffer_end)
    {
        handler_.end_buffer(buffer_end);
    }

    void end_buffer(std::false_type, ...)
    {}

    auto empty_physical_line(std::true_type, const char_type* where)
    {
        return handler_.empty_physical_line(where);
    }

    void empty_physical_line(std::false_type, ...)
    {}
};

}}

#endif