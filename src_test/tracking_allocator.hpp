/**
 * These codes are licensed under the Unlicense.
 * http://unlicense.org
 */

#ifndef FURFURYLIC_F2499770_B34D_40F0_9D61_D86483E76760
#define FURFURYLIC_F2499770_B34D_40F0_9D61_D86483E76760

#include <cassert>
#include <memory>
#include <utility>
#include <unordered_map>

namespace furfurylic { namespace test {

template <class BaseAllocator>
class tracking_allocator :
    public BaseAllocator
{
    std::vector<std::pair<char*, char*>>* allocated_;
    typename std::allocator_traits<BaseAllocator>::size_type* total_;

    template <class U>
    friend class tracking_allocator;

public:
    using base_traits = typename std::allocator_traits<BaseAllocator>;

    template <class U>
    struct rebind
    {
        using other =
            tracking_allocator<typename base_traits::template rebind_alloc<U>>;
    };

    // C++14 standard does not mandate default-constructibility of allocators
    // but some implementations require it
    tracking_allocator() :
        allocated_(nullptr), total_(nullptr)
    {}

    // C++14 standard says no constructors of an allocator shall exit via an
    // exception, so users must supply spaces by themselves for tracking info
    tracking_allocator(
        std::vector<std::pair<char*, char*>>& allocated,
        typename base_traits::size_type& total,
        const BaseAllocator& base = BaseAllocator()) :
        BaseAllocator(base), allocated_(&allocated), total_(&total)
    {}

    explicit tracking_allocator(
        std::vector<std::pair<char*, char*>>& allocated,
        const BaseAllocator& base = BaseAllocator()) :
        BaseAllocator(base), allocated_(&allocated), total_(nullptr)
    {}

    tracking_allocator(const tracking_allocator& other) noexcept = default;

    template <class U>
    explicit tracking_allocator(const tracking_allocator<U>& other) noexcept :
        BaseAllocator(other),
        allocated_(other.allocated_), total_(other.total_)
    {}

    auto allocate(typename base_traits::size_type n)
    {
        allocated_->emplace_back();                             // throw
        try {
            const auto p = base_traits::allocate(*this, n);      // throw
            char* const f = static_cast<char*>(true_addressof(p));
            const auto l = f + n * sizeof(typename base_traits::value_type);
            allocated_->back() = std::make_pair(f, l);
            if (total_) {
                *total_ += l - f;
            }
            return p;
        } catch (...) {
            allocated_->pop_back();
            throw;
        }
    }

    void deallocate(
        typename base_traits::pointer p,
        typename base_traits::size_type n) noexcept
    {
        const auto i = std::find_if(
            allocated_->cbegin(), allocated_->cend(),
            [p](auto be) {
                return be.first == tracking_allocator::true_addressof(p);
                    // "tracking_allocator::" is to satiate GCC 6.3.1
            });
        assert(i != allocated_->cend());
        allocated_->erase(i);
        BaseAllocator::deallocate(p, n);
    }

    template <class OtherBaseAllocator>
    bool operator==(
        const tracking_allocator<OtherBaseAllocator>& other) const noexcept
    {
        return (static_cast<const BaseAllocator&>(*this) == other)
            && (allocated_ == other.allocated_);
    }

    bool tracks(const void* p) const noexcept
    {
        for (const auto& be : *allocated_) {
            if ((be.first <= p) && (p < be.second)) {
                return true;
            }
        }
        return false;
    }

    typename base_traits::size_type total() const noexcept
    {
        return total_ ? *total_ : static_cast<decltype(*total_ + 0)>(-1);
    }

private:
    static void* true_addressof(typename base_traits::pointer p) noexcept
    {
        return static_cast<void*>(std::addressof(*p));
    }
};

template <class LeftBaseAllocator, class RightBaseAllocator>
bool operator!=(
    const tracking_allocator<LeftBaseAllocator>& left,
    const tracking_allocator<RightBaseAllocator>& right) noexcept
{
    return !(left == right);
}

}}

#endif