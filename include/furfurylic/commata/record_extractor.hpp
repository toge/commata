/**
 * These codes are licensed under the Unlicense.
 * http://unlicense.org
 */

#ifndef FURFURYLIC_GUARD_D53E08F9_CF1C_4762_BF77_1A6FB68C6A96
#define FURFURYLIC_GUARD_D53E08F9_CF1C_4762_BF77_1A6FB68C6A96

#include <ios>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <streambuf>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "csv_error.hpp"
#include "key_chars.hpp"

namespace furfurylic {
namespace commata {

namespace detail {

template <class T>
class field_name_of_impl
{
    const char* prefix_;
    const T* t_;

public:
    field_name_of_impl(const char* prefix, const T& t) :
        prefix_(prefix), t_(&t)
    {}

    friend std::ostream& operator<<(
        std::ostream& o, const field_name_of_impl& t)
    {
        return o << t.prefix_ << *t.t_;
    }
};

auto field_name_of(...)
{
    return "";
}

template <class T>
auto field_name_of(const char* prefix, const T& t)
  -> decltype(std::declval<std::ostream&>() << t,
              field_name_of_impl<T>(prefix, t))
{
    return field_name_of_impl<T>(prefix, t);
}

template <class F, class = void>
class member_like_base
{
    F f_;

public:
    template <class G>
    member_like_base(G&& f) :
        f_(std::forward<G>(f))
    {}

    F& get()
    {
        return f_;
    }

    const F& get() const
    {
        return f_;
    }
};

template <class F>
class member_like_base<F, std::enable_if_t<!std::is_final<F>::value>> :
    F
{
public:
    template <class G>
    member_like_base(G&& f) :
        F(std::forward<G>(f))
    {}

    F& get()
    {
        return *this;
    }

    const F& get() const
    {
        return *this;
    }
};

template <class FieldNamePred>
struct field_name_pred_base :
    member_like_base<FieldNamePred>
{
    template <class G>
    field_name_pred_base(G&& f) :
        member_like_base<FieldNamePred>(std::forward<G>(f))
    {}
};

template <class FieldValuePred>
struct field_value_pred_base :
    member_like_base<FieldValuePred>
{
    template <class G>
    field_value_pred_base(G&& f) :
        member_like_base<FieldValuePred>(std::forward<G>(f))
    {}
};

}

class record_extraction_error :
    public csv_error
{
public:
    using csv_error::csv_error;
};

namespace detail {

template <class Ch>
struct hollow_field_name_pred
{
    bool operator()(const Ch*, const Ch*) const
    {
        return true;
    }
};

}

template <class FieldNamePred, class FieldValuePred,
    class Ch, class Tr = std::char_traits<Ch>>
class record_extractor :
    detail::field_name_pred_base<FieldNamePred>,
    detail::field_value_pred_base<FieldValuePred>
{
    enum class record_mode : std::int_fast8_t
    {
        unknown,
        include,
        exclude
    };

    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    record_mode header_mode_;
    record_mode record_mode_;

    std::size_t record_num_to_include_;
    std::size_t target_field_index_;

    std::size_t field_index_;
    const Ch* record_begin_;
    std::basic_streambuf<Ch, Tr>* out_;
    std::vector<Ch> field_buffer_;
    std::vector<Ch> record_buffer_;

    using field_name_pred_t = detail::field_name_pred_base<FieldNamePred>;
    using field_value_pred_t = detail::field_value_pred_base<FieldValuePred>;

public:
    using char_type = Ch;

    record_extractor(
        std::basic_streambuf<Ch, Tr>* out,
        FieldNamePred field_name_pred, FieldValuePred field_value_pred,
        bool includes_header, std::size_t max_record_num) :
        field_name_pred_t(std::move(field_name_pred)),
        field_value_pred_t(std::move(field_value_pred)),
        header_mode_(includes_header ?
            record_mode::include : record_mode::exclude),
        record_mode_(record_mode::exclude),
        record_num_to_include_(max_record_num), target_field_index_(npos),
        field_index_(0), out_(out)
    {}

    record_extractor(
        std::basic_streambuf<Ch, Tr>* out,
        std::size_t target_field_index, FieldValuePred field_value_pred,
        bool includes_header, std::size_t max_record_num) :
        record_extractor(out,
            detail::hollow_field_name_pred<Ch>(),
            std::move(field_value_pred),
            includes_header, max_record_num)
    {
        if (target_field_index >= npos) {
            std::ostringstream what;
            what << "Target field index "
                 << target_field_index << " is too large";
            throw std::out_of_range(what.str());
        }
        target_field_index_ = target_field_index;
    }

    record_extractor(record_extractor&&) = default;
    record_extractor& operator=(record_extractor&&) = default;

    void start_buffer(const Ch* buffer_begin)
    {
        record_begin_ = buffer_begin;
    }

    void end_buffer(const Ch* buffer_end)
    {
        switch (record_mode_) {
        case record_mode::include:
            out_->sputn(record_begin_, buffer_end - record_begin_);
            break;
        case record_mode::unknown:
            record_buffer_.insert(
                record_buffer_.cend(), record_begin_, buffer_end);
            break;
        default:
            break;
        }
    }

    void start_record(const Ch* record_begin)
    {
        record_begin_ = record_begin;
        record_mode_ = header_yet() ? header_mode_ : record_mode::unknown;
        field_index_ = 0;
        record_buffer_.clear();
    }

    bool update(const Ch* first, const Ch* last)
    {
        if ((header_yet() && (target_field_index_ == npos))
         || (field_index_ == target_field_index_)) {
            field_buffer_.insert(field_buffer_.cend(), first, last);
        }
        return true;
    }

    bool finalize(const Ch* first, const Ch* last)
    {
        if (header_yet()) {
            if ((target_field_index_ == npos)
             && with_field_buffer_appended(first, last,
                    [this](const Ch* first, const Ch* last) {
                        return field_name_pred_t::get()(first, last);
                    })) {
                target_field_index_ = field_index_;
            }
            ++field_index_;
            if (field_index_ >= npos) {
                throw no_matching_field();
            }
        } else {
            if ((record_mode_ == record_mode::unknown)
             && (field_index_ == target_field_index_)) {
                if (with_field_buffer_appended(first, last,
                        [this](const Ch* first, const Ch* last) {
                            return field_value_pred_t::get()(first, last);
                        })) {
                    record_mode_ = record_mode::include;
                    if (!record_buffer_.empty()) {
                        out_->sputn(
                            record_buffer_.data(), record_buffer_.size());
                        record_buffer_.clear();
                    }
                } else {
                    record_mode_ = record_mode::exclude;
                }
            }
            ++field_index_;
            if (field_index_ >= npos) {
                record_mode_ = record_mode::exclude;
            }
        }
        return true;
    }

    bool end_record(const Ch* record_end)
    {
        if (header_yet()) {
            if (target_field_index_ == npos) {
                throw no_matching_field();
            }
            flush_record_if_include(record_end);
            if (record_num_to_include_ == 0) {
                return false;
            }
            header_mode_ = record_mode::unknown;
        } else if (flush_record_if_include(record_end)) {
            if (record_num_to_include_ == 1) {
                return false;
            }
            --record_num_to_include_;
        }
        return true;
    }

private:
    bool header_yet() const
    {
        return header_mode_ != record_mode::unknown;
    }

    template <class F>
    auto with_field_buffer_appended(const Ch* first, const Ch* last, F f)
    {
        if (field_buffer_.empty()) {
            return f(first, last);
        } else {
            field_buffer_.insert(field_buffer_.cend(), first, last);
            const auto r = f(
                field_buffer_.data(),
                field_buffer_.data() + field_buffer_.size());
            field_buffer_.clear();
            return r;
        }
    }

    record_extraction_error no_matching_field() const
    {
        std::ostringstream what;
        what << "No matching field"
             << field_name_of(" for ", field_name_pred_t::get());
        return record_extraction_error(what.str());
    }

    bool flush_record_if_include(const Ch* record_end)
    {
        switch (record_mode_) {
        case record_mode::include:
            if (!record_buffer_.empty()) {
                out_->sputn(record_buffer_.data(), record_buffer_.size());
            }
            out_->sputn(record_begin_, record_end - record_begin_);
            out_->sputc(detail::key_chars<Ch>::LF);
            record_mode_ = record_mode::exclude;    // to prevent end_buffer
                                                    // from doing anything
            return true;
        case record_mode::exclude:
            return false;
        case record_mode::unknown:
            assert(!header_yet());
            record_mode_ = record_mode::exclude;    // no such a far field
            return false;
        default:
            assert(false);
            return false;
        }
    }
};

namespace detail {

template <class Ch, class Tr>
class string_eq
{
    std::basic_string<Ch, Tr> s_;

public:
    explicit string_eq(std::basic_string<Ch, Tr>&& s) :
        s_(std::move(s))
    {}

    bool operator()(const Ch* begin, const Ch* end) const
    {
        const auto rlen = static_cast<decltype(s_.size())>(end - begin);
        return (s_.size() == rlen)
            && (Tr::compare(s_.data(), begin, rlen) == 0);
    }

    friend std::basic_ostream<Ch, Tr>& operator<<(
        std::basic_ostream<Ch, Tr>& os, const string_eq& o)
    {
        return os << o.s_;
    }
};

template <class Ch, class Tr, class T>
auto forward_as_string_pred(T&& t)
 -> std::enable_if_t<
        std::is_constructible<std::basic_string<Ch, Tr>, T&&>::value,
        string_eq<Ch, Tr>>
{
    return string_eq<Ch, Tr>(std::forward<T>(t));
}

template <class Ch, class Tr, class T>
auto forward_as_string_pred(T&& t)
 -> std::enable_if_t<
        !std::is_constructible<std::basic_string<Ch, Tr>, T&&>::value,
        T&&>
{
    return std::forward<T>(t);
}

template <class Ch, class Tr, class T>
using string_pred =
    typename std::conditional<
        std::is_constructible<std::basic_string<Ch, Tr>, T&&>::value,
        string_eq<Ch, Tr>,
        typename std::remove_reference<T>::type>::type;

template <class FieldNamePredF, class FieldValuePredF, class Ch, class Tr>
using record_extractor_from =
    record_extractor<
        string_pred<Ch, Tr, FieldNamePredF>,
        string_pred<Ch, Tr, FieldValuePredF>,
        Ch, Tr>;

} // end namespace detail

template <class FieldNamePredF, class FieldValuePredF, class Ch, class Tr>
auto make_record_extractor(
    std::basic_streambuf<Ch, Tr>* out,
    FieldNamePredF&& field_name_pred,
    FieldValuePredF&& field_value_pred,
    bool includes_header = true,
    std::size_t max_record_num = static_cast<std::size_t>(-1))
 -> std::enable_if_t<
        !std::is_integral<FieldNamePredF>::value,
        detail::record_extractor_from<FieldNamePredF, FieldValuePredF, Ch, Tr>>
{
    return detail::record_extractor_from<
            FieldNamePredF, FieldValuePredF, Ch, Tr>(
        out,
        detail::forward_as_string_pred<Ch, Tr>(
            std::forward<FieldNamePredF>(field_name_pred)),
        detail::forward_as_string_pred<Ch, Tr>(
            std::forward<FieldValuePredF>(field_value_pred)),
        includes_header, max_record_num);
}

template <class FieldNamePredF, class FieldValuePredF, class Ch, class Tr>
auto make_record_extractor(
    std::basic_ostream<Ch, Tr>& out,
    FieldNamePredF&& field_name_pred,
    FieldValuePredF&& field_value_pred,
    bool includes_header = true,
    std::size_t max_record_num = static_cast<std::size_t>(-1))
    ->std::enable_if_t<
    !std::is_integral<FieldNamePredF>::value,
    detail::record_extractor_from<FieldNamePredF, FieldValuePredF, Ch, Tr>>
{
    return make_record_extractor(out.rdbuf(),
        std::forward<FieldNamePredF>(field_name_pred),
        std::forward<FieldValuePredF>(field_value_pred),
        includes_header, max_record_num);
}

template <class FieldValuePredF, class Ch, class Tr>
auto make_record_extractor(
    std::basic_streambuf<Ch, Tr>* out,
    std::size_t target_field_index,
    FieldValuePredF&& field_value_pred,
    bool includes_header = true,
    std::size_t max_record_num = static_cast<std::size_t>(-1))
{
    return detail::record_extractor_from<
            detail::hollow_field_name_pred<Ch>, FieldValuePredF, Ch, Tr>(
        out,
        target_field_index,
        detail::forward_as_string_pred<Ch, Tr>(
            std::forward<FieldValuePredF>(field_value_pred)),
        includes_header, max_record_num);
}

template <class FieldValuePredF, class Ch, class Tr>
auto make_record_extractor(
    std::basic_ostream<Ch, Tr>& out,
    std::size_t target_field_index,
    FieldValuePredF&& field_value_pred,
    bool includes_header = true,
    std::size_t max_record_num = static_cast<std::size_t>(-1))
{
    return make_record_extractor(out.rdbuf(), target_field_index,
        std::forward<FieldValuePredF>(field_value_pred),
        includes_header, max_record_num);
}

}}

#endif
