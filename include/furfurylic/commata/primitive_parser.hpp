/**
 * These codes are licensed under the Unlicense.
 * http://unlicense.org
 */

#ifndef FURFURYLIC_GUARD_4B6A00F6_E33D_4114_9F57_D2C2E984E809
#define FURFURYLIC_GUARD_4B6A00F6_E33D_4114_9F57_D2C2E984E809

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <memory>
#include <string>
#include <streambuf>
#include <utility>

#include "csv_error.hpp"
#include "key_chars.hpp"

namespace furfurylic {
namespace commata {

class parse_error :
    public csv_error
{
public:
    explicit parse_error(std::string what_arg) :
        csv_error(std::move(what_arg))
    {}
};

namespace detail {

enum class state : std::int_fast8_t
{
    left_of_value,
    in_value,
    right_of_open_quote,
    in_quoted_value,
    in_quoted_value_after_quote,
    after_cr,
    after_lf
};

template <state s>
struct handler
{};

template <>
struct handler<state::left_of_value>
{
    template <class Parser>
    void normal(Parser& parser, typename Parser::char_type c) const
    {
        switch (c) {
        case key_chars<typename Parser::char_type>::COMMA:
            parser.set_first_last();
            parser.finalize();
            break;
        case key_chars<typename Parser::char_type>::DQUOTE:
            parser.change_state(state::right_of_open_quote);
            break;
        case key_chars<typename Parser::char_type>::CR:
            parser.set_first_last();
            parser.finalize();
            parser.end_record();
            parser.change_state(state::after_cr);
            break;
        case key_chars<typename Parser::char_type>::LF:
            parser.set_first_last();
            parser.finalize();
            parser.end_record();
            parser.change_state(state::after_lf);
            break;
        default:
            parser.set_first_last();
            parser.update_last();
            parser.change_state(state::in_value);
            break;
        }
    }

    template <class Parser>
    void underflow(Parser& /*parser*/) const
    {}

    template <class Parser>
    void eof(Parser& parser) const
    {
        parser.finalize();
    }
};

template <>
struct handler<state::in_value>
{
    template <class Parser>
    void normal(Parser& parser, typename Parser::char_type c) const
    {
        switch (c) {
        case key_chars<typename Parser::char_type>::COMMA:
            parser.finalize();
            parser.change_state(state::left_of_value);
            break;
        case key_chars<typename Parser::char_type>::DQUOTE:
            throw parse_error(
                "A quotation mark found in a non-escaped value");
        case key_chars<typename Parser::char_type>::CR:
            parser.finalize();
            parser.end_record();
            parser.change_state(state::after_cr);
            break;
        case key_chars<typename Parser::char_type>::LF:
            parser.finalize();
            parser.end_record();
            parser.change_state(state::after_lf);
            break;
        default:
            parser.update_last();
            break;
        }
    }

    template <class Parser>
    void underflow(Parser& parser) const
    {
        parser.update();
    }

    template <class Parser>
    void eof(Parser& parser) const
    {
        parser.finalize();
    }
};

template <>
struct handler<state::right_of_open_quote>
{
    template <class Parser>
    void normal(Parser& parser, typename Parser::char_type c) const
    {
        parser.set_first_last();
        if (c == key_chars<typename Parser::char_type>::DQUOTE) {
            parser.change_state(state::in_quoted_value_after_quote);
        } else {
            parser.update_last();
            parser.change_state(state::in_quoted_value);
        }
    }

    template <class Parser>
    void underflow(Parser& /*parser*/) const
    {}

    template <class Parser>
    void eof(Parser& /*parser*/) const
    {
        throw parse_error("EOF reached with an open escaped value");
    }
};

template <>
struct handler<state::in_quoted_value>
{
    template <class Parser>
    void normal(Parser& parser, typename Parser::char_type c) const
    {
        if (c == key_chars<typename Parser::char_type>::DQUOTE) {
            parser.update();
            parser.set_first_last();
            parser.change_state(state::in_quoted_value_after_quote);
        } else {
            parser.update_last();
        }
    }

    template <class Parser>
    void underflow(Parser& parser) const
    {
        parser.update();
    }

    template <class Parser>
    void eof(Parser& /*parser*/) const
    {
        throw parse_error("EOF reached with an open escaped value");
    }
};

template <>
struct handler<state::in_quoted_value_after_quote>
{
    template <class Parser>
    void normal(Parser& parser, typename Parser::char_type c) const
    {
        switch (c) {
        case key_chars<typename Parser::char_type>::COMMA:
            parser.finalize();
            parser.change_state(state::left_of_value);
            break;
        case key_chars<typename Parser::char_type>::DQUOTE:
            parser.set_first_last();
            parser.update_last();
            parser.change_state(state::in_quoted_value);
            break;
        case key_chars<typename Parser::char_type>::CR:
            parser.finalize();
            parser.end_record();
            parser.change_state(state::after_cr);
            break;
        case key_chars<typename Parser::char_type>::LF:
            parser.finalize();
            parser.end_record();
            parser.change_state(state::after_lf);
            break;
        default:
            throw parse_error(
                "An invalid character found after a closed escaped value");
        }
    }

    template <class Parser>
    void underflow(Parser& /*parser*/) const
    {}

    template <class Parser>
    void eof(Parser& parser) const
    {
        parser.finalize();
    }
};

template <>
struct handler<state::after_cr>
{
    template <class Parser>
    void normal(Parser& parser, typename Parser::char_type c) const
    {
        switch (c) {
        case key_chars<typename Parser::char_type>::COMMA:
            parser.new_physical_row();
            parser.set_first_last();
            parser.finalize();
            parser.change_state(state::left_of_value);
            break;
        case key_chars<typename Parser::char_type>::DQUOTE:
            parser.new_physical_row();
            parser.force_start_record();
            parser.change_state(state::right_of_open_quote);
            break;
        case key_chars<typename Parser::char_type>::CR:
            parser.new_physical_row();
            parser.empty_physical_row();
            break;
        case key_chars<typename Parser::char_type>::LF:
            parser.change_state(state::after_lf);
            break;
        default:
            parser.new_physical_row();
            parser.set_first_last();
            parser.update_last();
            parser.change_state(state::in_value);
            break;
        }
    }

    template <class Parser>
    void underflow(Parser& /*parser*/) const
    {}

    template <class Parser>
    void eof(Parser& /*parser*/) const
    {}
};

template <>
struct handler<state::after_lf>
{
    template <class Parser>
    void normal(Parser& parser, typename Parser::char_type c) const
    {
        switch (c) {
        case key_chars<typename Parser::char_type>::COMMA:
            parser.new_physical_row();
            parser.set_first_last();
            parser.finalize();
            parser.change_state(state::left_of_value);
            break;
        case key_chars<typename Parser::char_type>::DQUOTE:
            parser.new_physical_row();
            parser.force_start_record();
            parser.change_state(state::right_of_open_quote);
            break;
        case key_chars<typename Parser::char_type>::CR:
            parser.new_physical_row();
            parser.empty_physical_row();
            parser.change_state(state::after_cr);
            break;
        case key_chars<typename Parser::char_type>::LF:
            parser.new_physical_row();
            parser.empty_physical_row();
            break;
        default:
            parser.new_physical_row();
            parser.set_first_last();
            parser.update_last();
            parser.change_state(state::in_value);
            break;
        }
    }

    template <class Parser>
    void underflow(Parser& /*parser*/) const
    {}

    template <class Parser>
    void eof(Parser& /*parser*/) const
    {}
};

struct has_get_release_buffer_impl
{
    template <class Ch, class T>
    static auto check(T*)
     -> decltype(std::declval<T&>().release_buffer(
        std::pair<Ch*, std::size_t>(std::declval<T&>().get_buffer()).first),
        std::true_type());

    template <class Ch, class T>
    static auto check(...)
     -> std::false_type;
};

template <class Ch, class T>
struct has_get_release_buffer :
    decltype(has_get_release_buffer_impl::check<Ch, T>(nullptr))
{};

template <class Ch, class Sink, bool X>
class buffer_control;

template <class Ch, class Sink>
class buffer_control<Ch, Sink, false>
{
    std::size_t buffer_size_;
    Ch* buffer_;

protected:
    explicit buffer_control(std::size_t buffer_size) :
        buffer_size_((buffer_size < 1) ? 8192 : buffer_size),
        buffer_(nullptr)
    {}

    ~buffer_control()
    {
        delete [] buffer_;
    }

    std::pair<Ch*, std::size_t> do_get_buffer(Sink&)
    {
        if (!buffer_) {
            buffer_ = new Ch[buffer_size_]; // throw
        }
        return std::make_pair(buffer_, buffer_size_);
    }

    void do_release_buffer(Sink&, const Ch*) noexcept
    {}
};

template <class Ch, class Sink>
class buffer_control<Ch, Sink, true>
{
protected:
    explicit buffer_control(std::size_t)
    {}

    std::pair<Ch*, std::size_t> do_get_buffer(Sink& f)
    {
        return f.get_buffer();  // throw
    }

    void do_release_buffer(Sink& f, const Ch* buffer) noexcept
    {
        return f.release_buffer(buffer);
    }
};

struct has_start_end_buffer_impl
{
    template <class Ch, class T>
    static auto check(T*)
        -> decltype((std::declval<T&>().start_buffer(std::declval<Ch*>()),
            std::declval<T&>().end_buffer(std::declval<Ch*>())),
            std::true_type());

    template <class Ch, class T>
    static auto check(...)
        ->std::false_type;
};

template <class Ch, class T>
struct has_start_end_buffer :
    decltype(has_start_end_buffer_impl::check<Ch, T>(nullptr))
{};

struct has_empty_record_impl
{
    template <class Ch, class T>
    static auto check(T*) -> decltype(
        std::declval<bool&>() =
            std::declval<T&>().empty_physical_row(std::declval<const Ch*>()),
        std::true_type());

    template <class Ch, class T>
    static auto check(...)
     -> std::false_type;
};

template <class Ch, class T>
struct has_empty_record :
    decltype(has_empty_record_impl::check<Ch, T>(nullptr))
{};

template <class Ch, class Sink>
class full_fledged_sink :
    public buffer_control<Ch, Sink, has_get_release_buffer<Ch, Sink>::value>
{
    Sink sink_;

public:
    explicit full_fledged_sink(Sink&& sink, std::size_t buffer_size_hint) :
        buffer_control<Ch, Sink,
            has_get_release_buffer<Ch, Sink>::value>(buffer_size_hint),
        sink_(std::move(sink))
    {}

    std::pair<Ch*, std::size_t> get_buffer()
    {
        return this->do_get_buffer(sink_);
    }

    void release_buffer(const Ch* buffer) noexcept
    {
        this->do_release_buffer(sink_, buffer);
    }

    void start_buffer(const Ch* buffer_begin)
    {
        start_buffer(buffer_begin, has_start_end_buffer<Ch, Sink>());
    }

    void end_buffer(const Ch* buffer_end)
    {
        end_buffer(buffer_end, has_start_end_buffer<Ch, Sink>());
    }

    void start_record(const Ch* record_begin)
    {
        sink_.start_record(record_begin);
    }

    bool update(const Ch* first, const Ch* last)
    {
        return sink_.update(first, last);
    }

    bool finalize(const Ch* first, const Ch* last)
    {
        return sink_.finalize(first, last);
    }

    bool end_record(const Ch* end)
    {
        return sink_.end_record(end);
    }

    bool empty_physical_row(const Ch* where)
    {
        return empty_physical_row(where, has_empty_record<Ch, Sink>());
    }

private:
    void start_buffer(const Ch* buffer_begin, std::true_type)
    {
        sink_.start_buffer(buffer_begin);
    }

    void start_buffer(const Ch*, std::false_type)
    {}

    void end_buffer(const Ch* buffer_end, std::true_type)
    {
        sink_.end_buffer(buffer_end);
    }

    void end_buffer(const Ch*, std::false_type)
    {}

    bool empty_physical_row(const Ch* where, std::true_type)
    {
        return sink_.empty_physical_row(where);
    }

    bool empty_physical_row(const Ch*, std::false_type)
    {
        return true;
    }
};

template <class Ch, class Sink>
auto make_full_fledged(Sink&& sink, std::size_t buffer_size_hint)
 -> std::enable_if_t<!has_get_release_buffer<Ch, Sink>::value
                  || !has_start_end_buffer<Ch, Sink>::value
                  || !has_empty_record<Ch, Sink>::value,
    full_fledged_sink<Ch, Sink>>
{
    return full_fledged_sink<Ch, Sink>(std::move(sink), buffer_size_hint);
}

template <class Ch, class Sink>
auto make_full_fledged(Sink&& sink, std::size_t)
 -> std::enable_if_t<has_get_release_buffer<Ch, Sink>::value
                  && has_start_end_buffer<Ch, Sink>::value
                  && has_empty_record<Ch, Sink>::value,
    Sink&&>
{
    return std::forward<Sink>(sink);
}

template <class Ch, class Sink>
class primitive_parser
{
    const Ch* p_;
    Sink f_;

    bool record_started_;
    state s_;
    const Ch* field_start_;
    const Ch* field_end_;

    std::size_t physical_row_index_;
    const Ch* physical_row_or_buffer_begin_;
    std::size_t physical_row_chars_passed_away_;

private:
    template <state>
    friend struct handler;

    // To make control flows clearer, we adopt exceptions. Sigh...
    struct parse_aborted
    {};

public:
    using char_type = Ch;

    explicit primitive_parser(Sink f) :
        f_(std::move(f)),
        record_started_(false), s_(state::after_lf),
        physical_row_index_(parse_error::npos),
        physical_row_chars_passed_away_(0)
    {}

    primitive_parser(primitive_parser&&) = default;
    primitive_parser& operator=(primitive_parser&&) = default;

    template <class Tr>
    bool parse(std::basic_streambuf<Ch, Tr>& in)
    {
        auto release = [this](const Ch* buffer) {
            f_.release_buffer(buffer);
        };

        bool eof_reached = false;
        do {
            std::unique_ptr<Ch, decltype(release)> buffer(nullptr, release);
            std::size_t buffer_size;
            {
                auto allocated = f_.get_buffer();   // throw
                buffer.reset(allocated.first);
                buffer_size = allocated.second;
                if (buffer_size < 1) {
                    throw std::out_of_range(
                        "Specified buffer length is shorter than one");
                }
            }

            std::streamsize loaded_size = 0;
            do {
                const auto length = in.sgetn(buffer.get() + loaded_size,
                    buffer_size - loaded_size);
                if (length == 0) {
                    eof_reached = true;
                    break;
                }
                loaded_size += length;
            } while (buffer_size - loaded_size > 0);

            f_.start_buffer(buffer.get());
            if (!parse_partial(buffer.get(),
                    buffer.get() + loaded_size, eof_reached)) {
                return false;
            }
            f_.end_buffer(buffer.get() + loaded_size);
        } while (!eof_reached);

        return true;
    }

private:
    bool parse_partial(const Ch* begin, const Ch* end, bool eof_reached)
    {
        try {
            p_ = begin;
            physical_row_or_buffer_begin_ = begin;
            set_first_last();
            while (p_ < end) {
                with_handler([this](const auto& h) { h.normal(*this, *p_); });
                ++p_;
            }
            with_handler([this](const auto& h) { h.underflow(*this); });
            if (eof_reached) {
                set_first_last();
                with_handler([this](const auto& h) { h.eof(*this); });
                if (record_started_) {
                    end_record();
                }
            }
            physical_row_chars_passed_away_ +=
                p_ - physical_row_or_buffer_begin_;
            return true;
        } catch (csv_error& e) {
            e.set_physical_position(
                physical_row_index_,
                (p_ - physical_row_or_buffer_begin_)
                    + physical_row_chars_passed_away_);
            throw;
        } catch (const parse_aborted&) {
            return false;
        }
    }

private:
    void new_physical_row()
    {
        if (physical_row_index_ == parse_error::npos) {
            physical_row_index_ = 0;
        } else {
            ++physical_row_index_;
        }
        physical_row_or_buffer_begin_ = p_;
        physical_row_chars_passed_away_ = 0;
    }

    void change_state(state s)
    {
        s_ = s;
    }

    void set_first_last()
    {
        field_start_ = p_;
        field_end_ = p_;
    }

    void update_last()
    {
        field_end_ = p_ + 1;
    }

    void update()
    {
        if (!record_started_) {
            f_.start_record(field_start_);
            record_started_ = true;
        }
        if (field_start_ < field_end_) {
            if (!f_.update(field_start_, field_end_)) {
                throw parse_aborted();
            }
        }
    }

    void finalize()
    {
        if (!record_started_) {
            f_.start_record(field_start_);
            record_started_ = true;
        }
        if (!f_.finalize(field_start_, field_end_)) {
            throw parse_aborted();
        }
    }

    void force_start_record()
    {
        f_.start_record(p_);
        record_started_ = true;
    }

    void end_record()
    {
        if (!f_.end_record(p_)) {
            throw parse_aborted();
        }
        record_started_ = false;
    }

    void empty_physical_row()
    {
        assert(!record_started_);
        if (!f_.empty_physical_row(p_)) {
            throw parse_aborted();
        }
    }

private:
    template <class F>
    void with_handler(F f)
    {
        switch (s_) {
        case state::left_of_value:
            f(handler<state::left_of_value>());
            break;
        case state::in_value:
            f(handler<state::in_value>());
            break;
        case state::right_of_open_quote:
            f(handler<state::right_of_open_quote>());
            break;
        case state::in_quoted_value:
            f(handler<state::in_quoted_value>());
            break;
        case state::in_quoted_value_after_quote:
            f(handler<state::in_quoted_value_after_quote>());
            break;
        case state::after_cr:
            f(handler<state::after_cr>());
            break;
        case state::after_lf:
            f(handler<state::after_lf>());
            break;
        default:
            assert(false);
            break;
        }
    }
};

template <class Ch, class Sink>
primitive_parser<Ch, Sink> make_primitive_parser(Sink&& sink)
{
    return primitive_parser<Ch, Sink>(std::move(sink));
}

} // end namespace detail

template <class Ch, class Tr, class Sink>
bool parse(std::basic_streambuf<Ch, Tr>& in, Sink sink,
    std::size_t buffer_size = 0)
{
    return detail::make_primitive_parser<Ch>(
        detail::make_full_fledged<Ch>(
            std::move(sink), buffer_size)).parse(in);
}

template <class Ch, class Sink>
struct empty_physical_row_aware_sink : Sink
{
    explicit empty_physical_row_aware_sink(Sink&& sink) :
        Sink(std::move(sink))
    {}

    bool empty_physical_row(const Ch* where)
    {
        this->start_record(where);
        return this->end_record(where);
    }
};

template <class Ch, class Sink>
auto make_empty_physical_row_aware(Sink&& sink)
{
    return empty_physical_row_aware_sink<Ch,
        std::remove_reference_t<Sink>>(std::forward<Sink>(sink));
}

}}

#endif
