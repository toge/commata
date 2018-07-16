/**
 * These codes are licensed under the Unlicense.
 * http://unlicense.org
 */

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

#include <algorithm>
#include <cstring>
#include <deque>
#include <list>
#include <iomanip>
#include <locale>
#include <deque>
#include <scoped_allocator>
#include <string>
#include <sstream>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <furfurylic/commata/stored_table.hpp>
#include <furfurylic/commata/parse_csv.hpp>

#include "BaseTest.hpp"
#include "tracking_allocator.hpp"

using namespace furfurylic::commata;
using namespace furfurylic::test;

static_assert(noexcept(stored_value(nullptr, nullptr)), "");
static_assert(std::is_nothrow_copy_constructible<stored_value>::value, "");
static_assert(std::is_nothrow_copy_assignable<stored_value>::value, "");
static_assert(noexcept(std::declval<stored_value&>().begin()), "");
static_assert(noexcept(std::declval<stored_value&>().end()), "");
static_assert(noexcept(std::declval<stored_value&>().rbegin()), "");
static_assert(noexcept(std::declval<stored_value&>().rend()), "");
static_assert(noexcept(std::declval<stored_value&>().cbegin()), "");
static_assert(noexcept(std::declval<stored_value&>().cend()), "");
static_assert(noexcept(std::declval<stored_value&>().crbegin()), "");
static_assert(noexcept(std::declval<stored_value&>().crend()), "");
static_assert(noexcept(std::declval<const stored_value&>().begin()), "");
static_assert(noexcept(std::declval<const stored_value&>().end()), "");
static_assert(noexcept(std::declval<const stored_value&>().rbegin()), "");
static_assert(noexcept(std::declval<const stored_value&>().rend()), "");
static_assert(noexcept(std::declval<stored_value&>().c_str()), "");
static_assert(noexcept(std::declval<const stored_value&>().c_str()), "");
static_assert(noexcept(std::declval<stored_value&>().data()), "");
static_assert(noexcept(std::declval<const stored_value&>().data()), "");
static_assert(noexcept(std::declval<stored_value&>().size()), "");
static_assert(noexcept(std::declval<stored_value&>().length()), "");
static_assert(noexcept(std::declval<stored_value&>().empty()), "");
static_assert(noexcept(std::declval<stored_value&>().clear()), "");
static_assert(noexcept(
    std::declval<stored_value&>().swap(std::declval<stored_value&>())), "");
static_assert(noexcept(
    swap(std::declval<stored_value&>(), std::declval<stored_value&>())), "");

template <class Ch>
struct TestStoredValue : BaseTest
{};

typedef testing::Types<char, wchar_t> Chs;

TYPED_TEST_CASE(TestStoredValue, Chs);

TYPED_TEST(TestStoredValue, Iterators)
{
    using char_t = TypeParam;
    using string_t = std::basic_string<char_t>;
    using value_t = basic_stored_value<char_t>;

    const auto ch = char_helper<char_t>::ch;
    const auto str = char_helper<char_t>::str;
    const auto str0 = char_helper<char_t>::str0;

    auto s = str0("strings");   // s.back() == '\0'
    value_t v(&s[0], &s[s.size() - 1]);
    const auto& cv = v;

    // Read from explicitly-const iterators
    {
        string_t copied(v.cbegin(), v.cend());
        ASSERT_EQ(str("strings"), copied);
    }
    {
        string_t copied(v.crbegin(), v.crend());
        ASSERT_EQ(str("sgnirts"), copied);
    }

    // Write through non-const iterators
    v.begin()[3] = ch('a');
    v.rbegin()[0] = ch('e');
    ASSERT_EQ(str0("strange"), s);

    // Read from implicitly-const iterators
    {
        string_t copied(cv.begin(), cv.end());
        ASSERT_EQ(str("strange"), copied);
    }
    {
        string_t copied(cv.rbegin(), cv.rend());
        ASSERT_EQ(str("egnarts"), copied);
    }
}

TYPED_TEST(TestStoredValue, Empty)
{
    using char_t = TypeParam;
    using value_t = basic_stored_value<char_t>;

    const auto str0 = char_helper<char_t>::str0;

    auto s1 = str0("");     // s1.front()|s1.back() == '\0'
    value_t v(&s1[0], &s1[0]);
    const auto& cv = v;

    ASSERT_TRUE(v.empty());
    ASSERT_EQ(0U, v.size());
    ASSERT_EQ(0U, v.length());
    ASSERT_TRUE(v.begin() == v.end());
    ASSERT_TRUE(v.cbegin() == v.cend());
    ASSERT_TRUE(v.rbegin() == v.rend());
    ASSERT_TRUE(v.crbegin() == v.crend());
    ASSERT_TRUE(cv.begin() == cv.end());
    ASSERT_TRUE(cv.rbegin() == cv.rend());
}

TYPED_TEST(TestStoredValue, Relations)
{
    using char_t = TypeParam;
    using string_t = std::basic_string<char_t>;
    using value_t = basic_stored_value<char_t>;

    const auto str = char_helper<char_t>::str;

    std::vector<std::pair<const char*, const char*>> pairs = {
        { "plastic", "elastic" },       // same length, differ at front
        { "Maria", "Mario" },           // same length, differ at back
        { "galactic", "galactica" },    // have same prefix, lengths differ
        { "running", "run" },           // have same prefix, lengths differ
        { "identical", "identical" },   // identical
        { "", "empty" }                 // empty
    };

    for (const auto& p : pairs) {
        string_t s1 = str(p.first);
        string_t s2 = str(p.second);
        string_t s01 = s1 + char_t();
        string_t s02 = s2 + char_t();
        value_t v1(&s01[0], &s01[s01.size() - 1]);
        value_t v2(&s02[0], &s02[s02.size() - 1]);

        ASSERT_EQ(s1 == s2, v1 == v2) << s1 << " == " << s2;
        ASSERT_EQ(s1 != s2, v1 != v2) << s1 << " != " << s2;
        ASSERT_EQ(s1 < s2, v1 < v2) << s1 << " < " << s2;
        ASSERT_EQ(s1 > s2, v1 > v2) << s1 << " > " << s2;
        ASSERT_EQ(s1 <= s2, v1 <= v2) << s1 << " <= " << s2;
        ASSERT_EQ(s1 >= s2, v1 >= v2) << s1 << " >= " << s2;
        ASSERT_EQ(s2 < s1, v2 < v1) << s2 << " < " << s1;
        ASSERT_EQ(s2 > s1, v2 > v1) << s2 << " > " << s1;
        ASSERT_EQ(s2 <= s1, v2 <= v1) << s2 << " <= " << s1;
        ASSERT_EQ(s2 >= s1, v2 >= v1) << s2 << " >= " << s1;

        ASSERT_EQ(s1 == s2, v1 == s2) << s1 << " == " << s2;
        ASSERT_EQ(s1 != s2, v1 != s2) << s1 << " != " << s2;
        ASSERT_EQ(s1 < s2, v1 < s2) << s1 << " < " << s2;
        ASSERT_EQ(s1 > s2, v1 > s2) << s1 << " > " << s2;
        ASSERT_EQ(s1 <= s2, v1 <= s2) << s1 << " <= " << s2;
        ASSERT_EQ(s1 >= s2, v1 >= s2) << s1 << " >= " << s2;
        ASSERT_EQ(s2 < s1, v2 < s1) << s2 << " < " << s1;
        ASSERT_EQ(s2 > s1, v2 > s1) << s2 << " > " << s1;
        ASSERT_EQ(s2 <= s1, v2 <= s1) << s2 << " <= " << s1;
        ASSERT_EQ(s2 >= s1, v2 >= s1) << s2 << " >= " << s1;

        ASSERT_EQ(s1 == s2, s1 == v2) << s1 << " == " << s2;
        ASSERT_EQ(s1 != s2, s1 != v2) << s1 << " != " << s2;
        ASSERT_EQ(s1 < s2, s1 < v2) << s1 << " < " << s2;
        ASSERT_EQ(s1 > s2, s1 > v2) << s1 << " > " << s2;
        ASSERT_EQ(s1 <= s2, s1 <= v2) << s1 << " <= " << s2;
        ASSERT_EQ(s1 >= s2, s1 >= v2) << s1 << " >= " << s2;
        ASSERT_EQ(s2 < s1, s2 < v1) << s2 << " < " << s1;
        ASSERT_EQ(s2 > s1, s2 > v1) << s2 << " > " << s1;
        ASSERT_EQ(s2 <= s1, s2 <= v1) << s2 << " <= " << s1;
        ASSERT_EQ(s2 >= s1, s2 >= v1) << s2 << " >= " << s1;

        ASSERT_EQ(s1 == s2, v1 == s2.c_str()) << s1 << " == " << s2;
        ASSERT_EQ(s1 != s2, v1 != s2.c_str()) << s1 << " != " << s2;
        ASSERT_EQ(s1 < s2, v1 < s2.c_str()) << s1 << " < " << s2;
        ASSERT_EQ(s1 > s2, v1 > s2.c_str()) << s1 << " > " << s2;
        ASSERT_EQ(s1 <= s2, v1 <= s2.c_str()) << s1 << " <= " << s2;
        ASSERT_EQ(s1 >= s2, v1 >= s2.c_str()) << s1 << " >= " << s2;
        ASSERT_EQ(s2 < s1, v2 < s1.c_str()) << s2 << " < " << s1;
        ASSERT_EQ(s2 > s1, v2 > s1.c_str()) << s2 << " > " << s1;
        ASSERT_EQ(s2 <= s1, v2 <= s1.c_str()) << s2 << " <= " << s1;
        ASSERT_EQ(s2 >= s1, v2 >= s1.c_str()) << s2 << " >= " << s1;

        ASSERT_EQ(s1 == s2, s1.c_str() == v2) << s1 << " == " << s2;
        ASSERT_EQ(s1 != s2, s1.c_str() != v2) << s1 << " != " << s2;
        ASSERT_EQ(s1 < s2, s1.c_str() < v2) << s1 << " < " << s2;
        ASSERT_EQ(s1 > s2, s1.c_str() > v2) << s1 << " > " << s2;
        ASSERT_EQ(s1 <= s2, s1.c_str() <= v2) << s1 << " <= " << s2;
        ASSERT_EQ(s1 >= s2, s1.c_str() >= v2) << s1 << " >= " << s2;
        ASSERT_EQ(s2 < s1, s2.c_str() < v1) << s2 << " < " << s1;
        ASSERT_EQ(s2 > s1, s2.c_str() > v1) << s2 << " > " << s1;
        ASSERT_EQ(s2 <= s1, s2.c_str() <= v1) << s2 << " <= " << s1;
        ASSERT_EQ(s2 >= s1, s2.c_str() >= v1) << s2 << " >= " << s1;
    }
}

TYPED_TEST(TestStoredValue, Sizes)
{
    using char_t = TypeParam;
    using value_t = basic_stored_value<char_t>;

    const auto str0 = char_helper<char_t>::str0;

    auto s = str0("obscura");   // s.back() == '\0'
    value_t v(&s[0], &s[s.size() - 1]);
    const auto& cv = v;

    ASSERT_EQ(s.size() - 1, cv.size());
    ASSERT_EQ(s.length() - 1, cv.size());
    ASSERT_FALSE(cv.empty());
}

TYPED_TEST(TestStoredValue, RelationsSpecial)
{
    using char_t = TypeParam;
    using string_t = std::basic_string<char_t>;
    using value_t = basic_stored_value<char_t>;

    const auto ch = char_helper<char_t>::ch;

    string_t s0 = { ch('a'), ch('b'), ch('c'), ch('\0'),
                    ch('d'), ch('e'), ch('f') };
    string_t s = s0 + ch('\0');
    ASSERT_EQ(8U, s.size()) << "Test's precondition";
    value_t v = value_t(&s[0], &s[0] + s.size() - 1);
    ASSERT_EQ(7U, v.size()) << "Test's precondition";

    ASSERT_TRUE(v == s0);
    ASSERT_FALSE(v == s0.c_str());  // "abc\0def" vs "abc"
    ASSERT_TRUE(v > s0.c_str());    // ditto
}

TYPED_TEST(TestStoredValue, FrontBack)
{
    using char_t = TypeParam;
    using value_t = basic_stored_value<char_t>;

    const auto ch = char_helper<char_t>::ch;
    const auto str = char_helper<char_t>::str;
    const auto str0 = char_helper<char_t>::str0;

    auto s = str0("mars");  // s.back() == '\0'
    value_t v(&s[0], &s[s.size() - 1]);
    const auto& cv = v;

    ASSERT_EQ(s.size() - 1, cv.size());
    ASSERT_EQ(s.length() - 1, cv.size());
    ASSERT_FALSE(cv.empty());

    ASSERT_EQ(ch('m'), v.front());
    ASSERT_EQ(ch('m'), cv.front());
    ASSERT_EQ(ch('s'), v.back());
    ASSERT_EQ(ch('s'), cv.back());

    v.front() = 'c';
    v.back() = 'e';

    ASSERT_EQ(str("care"), cv);
}

TYPED_TEST(TestStoredValue, Pop)
{
    using char_t = TypeParam;
    using value_t = basic_stored_value<char_t>;

    const auto str = char_helper<char_t>::str;
    const auto str0 = char_helper<char_t>::str0;

    auto s = str0("hamburger");     // s.back() == '\0'
    value_t v(&s[0], &s[s.size() - 1]);

    v.pop_front();  // "amburger"
    v.pop_front();  // "mburger"
    v.pop_front();  // "burger"
    v.pop_front();  // "urger"
    v.pop_back();   // "urge"
    ASSERT_EQ(str("urge"), v);
}

TYPED_TEST(TestStoredValue, Erase)
{
    using char_t = TypeParam;
    using value_t = basic_stored_value<char_t>;

    const auto ch = char_helper<char_t>::ch;
    const auto str = char_helper<char_t>::str;
    const auto str0 = char_helper<char_t>::str0;

    auto s = str0("hamburger");     // s.back() == '\0'
    value_t v(&s[0], &s[s.size() - 1]);

    ASSERT_EQ(
        ch('a'),
        *v.erase(v.cbegin()));                      // "amburger"
    ASSERT_EQ(str("amburger"), v);

    ASSERT_EQ(
        ch('e'),
        *v.erase(v.cbegin() + 3, v.cbegin() + 6));  // "amber"
    ASSERT_EQ(str("amber"), v);

    ASSERT_EQ(
        ch('r'),
        *v.erase(v.cbegin() + 1, v.cbegin() + 4));  // "ar"
    ASSERT_EQ(str("ar"), v);

    const auto e = v.erase(v.cend() - 1);           // "r"
    ASSERT_EQ(v.cend(), e);
    ASSERT_EQ(str("a"), v);

    v.clear();
    ASSERT_EQ(str(""), v);
    ASSERT_TRUE(v.empty());
}

TYPED_TEST(TestStoredValue, EraseByIndex)
{
    using char_t = TypeParam;
    using value_t = basic_stored_value<char_t>;

    const auto str = char_helper<char_t>::str;
    const auto str0 = char_helper<char_t>::str0;

    auto s = str0("latter");        // s.back() == '\0'
    value_t v(&s[0], &s[s.size() - 1]);

    ASSERT_THROW(v.erase(6), std::out_of_range);

    ASSERT_EQ(
        &v,
        &v.erase(2, 1));            // "later"
    ASSERT_EQ(str("later"), v);

    ASSERT_EQ(
        &v,
        &v.erase(4));               // "late"
    ASSERT_EQ(str("late"), v);
}

TYPED_TEST(TestStoredValue, IndexAccess)
{
    using char_t = TypeParam;
    using value_t = basic_stored_value<char_t>;

    const auto ch = char_helper<char_t>::ch;
    const auto str = char_helper<char_t>::str;
    const auto str0 = char_helper<char_t>::str0;

    auto s = str0("string");    // s.back() == '\0'
    value_t v(&s[0], &s[s.size() - 1]);
    const auto& cv = v;

    ASSERT_EQ(ch('s'), v[0]);
    ASSERT_EQ(ch('t'), cv[1]);
    ASSERT_EQ(ch('\0'), cv[v.size()]);  // OK

    v.at(3) = ch('o');
    ASSERT_EQ(str("strong"), cv);
}

TYPED_TEST(TestStoredValue, At)
{
    using char_t = TypeParam;
    using value_t = basic_stored_value<char_t>;

    const auto ch = char_helper<char_t>::ch;
    const auto str = char_helper<char_t>::str;
    const auto str0 = char_helper<char_t>::str0;

    auto s = str0("strings");   // s.back() == '\0'
    value_t v(&s[0], &s[s.size() - 1]);
    const auto& cv = v;

    ASSERT_EQ(ch('s'), v.at(0));
    ASSERT_EQ(ch('t'), cv.at(1));
    ASSERT_EQ(ch('s'), cv.at(v.size() - 1));
    ASSERT_THROW(v.at(v.size()), std::out_of_range);
    ASSERT_THROW(
        cv.at(static_cast<typename value_t::size_type>(-1)),
        std::out_of_range);

    v.at(3) = ch('a');
    v.at(6) = ch('e');
    ASSERT_EQ(str("strange"), cv);
}

TYPED_TEST(TestStoredValue, Data)
{
    using char_t = TypeParam;
    using value_t = basic_stored_value<char_t>;

    const auto str = char_helper<char_t>::str;
    const auto str0 = char_helper<char_t>::str0;

    auto s = str0("string");    // s.back() == '\0'
    value_t v(&s[0], &s[s.size() - 1]);
    const auto& cv = v;

    ASSERT_EQ(cv.begin(), cv.c_str());
    v.c_str()[3] = 'o';
    ASSERT_EQ(str("strong"), cv);

    ASSERT_EQ(cv.begin(), cv.data());
    v.data()[1] = 'w';
    v.pop_front();
    ASSERT_EQ(str("wrong"), cv);
}

TYPED_TEST(TestStoredValue, Swap)
{
    using char_t = TypeParam;
    using value_t = basic_stored_value<char_t>;

    const auto str0 = char_helper<char_t>::str0;

    auto s1 = str0("swap");
    auto s2 = str0("wasp");
    value_t v1(&s1[0], &s1[s1.size() - 1]);
    value_t v2(&s2[0], &s2[s2.size() - 1]);

    const auto b1 = v1.cbegin();
    const auto b2 = v2.cbegin();

    v1.swap(v2);
    ASSERT_EQ(b1, v2.cbegin());
    ASSERT_EQ(b2, v1.cbegin());

    swap(v1, v2);
    ASSERT_EQ(b1, v1.cbegin());
    ASSERT_EQ(b2, v2.cbegin());
}

TYPED_TEST(TestStoredValue, Write)
{
    using namespace std;
    using char_t = TypeParam;
    using value_t = basic_stored_value<char_t>;
    using string_t = basic_string<char_t>;
    using stream_t = basic_stringstream<char_t>;

    const auto ch = char_helper<char_t>::ch;
    const auto str = char_helper<char_t>::str;

    string_t s = str("write");
    string_t s0 = s + char_t();
    const value_t v(&s0[0], &s0[s0.size() - 1]);

    stream_t o1;
    o1 << setfill(ch('_')) << setw(10) << s
       << setfill(ch('*')) << left << setw(8) << s
       << setfill(ch('+')) << setw(4) << s
       << 10;
    stream_t o2;
    o2 << setfill(ch('_')) << setw(10) << v
       << setfill(ch('*')) << left << setw(8) << v
       << setfill(ch('+')) << setw(4) << v
       << 10;
    ASSERT_EQ(o1.str(), o2.str());
}

namespace privy {

using store_t = detail::table_store<char, std::allocator<char>>;

static_assert(std::is_default_constructible<store_t>::value, "");
static_assert(std::is_nothrow_move_constructible<store_t>::value, "");
static_assert(std::is_nothrow_move_assignable<store_t>::value, "");
static_assert(noexcept(std::declval<store_t&>().secure_any(0)), "");
static_assert(noexcept(std::declval<store_t&>().clear()), "");
static_assert(noexcept(
    std::declval<store_t&>().merge(std::declval<store_t>())), "");
static_assert(noexcept(
    std::declval<store_t&>().swap(std::declval<store_t&>())), "");
static_assert(noexcept(
    swap(std::declval<store_t&>(), std::declval<store_t&>())), "");

}

class TestTableStore : public furfurylic::test::BaseTest
{};

TEST_F(TestTableStore, Basics)
{
    using store_t = detail::table_store<char, std::allocator<char>>;

    store_t store;

    // Add one buffer
    char* const buffer1 = store.get_allocator().allocate(10);
    store.add_buffer(buffer1, 10);
    ASSERT_EQ(store_t::security{ buffer1 }, store.get_security());

    // Secure on the first buffer
    ASSERT_EQ(buffer1, store.secure_any(4));
    ASSERT_EQ(store_t::security{ buffer1 + 4 }, store.get_security());
    ASSERT_EQ(buffer1 + 4, store.secure_any(6));
    store.secure_current_upto(buffer1 + 4);
    ASSERT_EQ(nullptr, store.secure_any(7));

    // Add another buffer and secure on it
    char* const buffer2 = store.get_allocator().allocate(15);
    store.add_buffer(buffer2, 15);
    ASSERT_EQ(buffer2, store.secure_any(7));

    store.clear();
    {
        store_t::security expected = { buffer2, buffer1 };
        ASSERT_EQ(expected, store.get_security());
    }
}

TEST_F(TestTableStore, Merge)
{
    using store_t = detail::table_store<wchar_t, std::allocator<wchar_t>>;

    store_t store1;
    wchar_t* const buffer1 = store1.get_allocator().allocate(10);
    store1.add_buffer(buffer1, 10);
    store1.secure_any(6);
    ASSERT_EQ(nullptr, store1.secure_any(10));
    ASSERT_EQ(store_t::security{ buffer1 + 6 }, store1.get_security());

    store_t store2;
    wchar_t* const buffer2 = store2.get_allocator().allocate(15);
    store2.add_buffer(buffer2, 15);
    store2.secure_any(4);

    store1.merge(std::move(store2));
    {
        store_t::security expected = { buffer1 + 6, buffer2 + 4 };
        ASSERT_EQ(expected, store1.get_security());
    }
    ASSERT_EQ(buffer2 + 4, store1.secure_any(10));
    {
        store_t::security expected = { buffer1 + 6, buffer2 + 14 };
        ASSERT_EQ(expected, store1.get_security());
    }

    {
        store_t::security s = { buffer1 + 6, buffer2 + 10 };
        store1.set_security(s);
        ASSERT_EQ(s, store1.get_security());
    }
}

TEST_F(TestTableStore, Swap)
{
    using store_t = detail::table_store<char, std::allocator<char>>;

    store_t store1;
    char* const buffer11 = store1.get_allocator().allocate(3);
    char* const buffer12 = store1.get_allocator().allocate(3);
    std::strcpy(buffer11, "AB");
    std::strcpy(buffer12, "ab");
    store1.add_buffer(buffer11, 3);
    store1.add_buffer(buffer12, 3);

    store_t store2;
    char* const buffer21 = store2.get_allocator().allocate(3);
    char* const buffer22 = store2.get_allocator().allocate(3);
    std::strcpy(buffer21, "XY");
    std::strcpy(buffer22, "xy");
    store2.add_buffer(buffer21, 3);
    store2.add_buffer(buffer22, 3);

    store_t::security expected1 = { buffer12, buffer11 };
    store_t::security expected2 = { buffer22, buffer21 };

    store1.swap(store2);
    ASSERT_EQ(expected2, store1.get_security());
    ASSERT_EQ(expected1, store2.get_security());

    swap(store1, store2);
    ASSERT_EQ(expected1, store1.get_security());
    ASSERT_EQ(expected2, store2.get_security());
}

static_assert(detail::is_std_vector<std::vector<int>>::value, "");
static_assert(!detail::is_std_vector<std::deque<int>>::value, "");
static_assert(detail::is_std_deque<std::deque<std::string>>::value, "");
static_assert(!detail::is_std_deque<std::list<std::vector<int>>>::value, "");
static_assert(detail::is_std_list<std::list<double>>::value, "");
static_assert(!detail::is_std_list<std::vector<std::deque<int>>>::value, "");

static_assert(std::is_default_constructible<stored_table>::value, "");
static_assert(noexcept(std::declval<stored_table&>().content()), "");
static_assert(noexcept(std::declval<const stored_table&>().content()), "");

struct TestStoredTable : furfurylic::test::BaseTest
{};

TEST_F(TestStoredTable, RewriteValue)
{
    wstored_table table(10U);

    // First record
    table.content().emplace_back();
    table[0].resize(2);

    // Consumes 5 chars
    table.rewrite_value(table[0][0], L"star");
    ASSERT_EQ(L"star", table[0][0]);
    const auto v = table[0][0];

    // In-place contraction is OK
    table.rewrite_value(table[0][0], L"sun");
    ASSERT_EQ(L"sun", table[0][0]);
    ASSERT_TRUE(
        (v.cbegin() <= table[0][0].cbegin())
     && (table[0][0].cend() <= v.cend()));

    // Expansion to 5 chars is fulfilled by comsuming next spaces
    std::wstring moon(L"moon");
    table[0][0] = table.import_value(moon.c_str(), moon.c_str() + 4);
    ASSERT_EQ(L"moon", table[0][0]);
    ASSERT_EQ(v.cbegin() + 5, table[0][0].c_str());

    // Consume another buffer by 5 chars
    table.rewrite_value(table[0][1], table[0][0]);
    ASSERT_EQ(L"moon", table[0][1]);
    ASSERT_TRUE(
        (table[0][0].cend() < v.cbegin())
     || (v.cend() <= table[0][0].cbegin()));
}

TEST_F(TestStoredTable, Copy)
{
    stored_table table1(10U);
    table1.content().emplace_back(2U);
    table1.content().emplace_back(1U);
    table1.rewrite_value(table1[0][0], "sky");              // 4 chars
    table1.rewrite_value(table1[0][1], "anaesthesia");      // 11 chars
    table1.rewrite_value(table1[1][0], "catalogue");        // 9 chars
    table1[1][0].erase(
        table1[1][0].cbegin() + 3, table1[1][0].cend());    // "cat"

    // Copy ctor

    stored_table table2(table1);

    ASSERT_EQ(2U, table2.size());
    ASSERT_EQ(2U, table2[0].size());
    ASSERT_EQ(1U, table2[1].size());
    ASSERT_EQ("sky", table2[0][0]);
    ASSERT_EQ("anaesthesia", table2[0][1]);
    ASSERT_EQ("cat", table2[1][0]);

    // In table2, "cat" is placed in the first buffer,
    // just after "sky"
    ASSERT_EQ(table2[0][0].cend() + 1, table2[1][0].cbegin());

    // Shrink to fit

    table1.shrink_to_fit();

    // Compacted just like table2
    ASSERT_EQ(table1[0][0].cend() + 1, table1[1][0].cbegin());

    // Copy assignment

    table2.content().pop_front();

    table1 = table2;
    table2.clear();

    ASSERT_EQ(1U, table1.size());
    ASSERT_EQ(1U, table1[0].size());
    ASSERT_EQ("cat", table1[0][0]);
}

TEST_F(TestStoredTable, MergeLists)
{
    basic_stored_table<std::list<std::vector<stored_value>>> table1(10U);
    table1.content().emplace_back();
    table1.content().back().emplace_back();
    table1.rewrite_value(table1.content().back().back(), "apples");

    basic_stored_table<std::list<std::vector<stored_value>>> table2(10U);
    table2.content().emplace_back();
    table2.content().back().emplace_back();
    table2.rewrite_value(table2.content().back().back(), "oranges");

    const auto field100 = &table1.content().front().front();
    const auto field200 = &table2.content().front().front();

    const auto table3 = std::move(table1) + std::move(table2);
    ASSERT_EQ(2U, table3.size());
    ASSERT_EQ(1U, table3.content().front().size());
    ASSERT_EQ("apples", table3.content().front().front());
    ASSERT_EQ(1U, table3.content().back().size());
    ASSERT_EQ("oranges", table3.content().back().front());

    // merger of lists shall be done by splicing,
    // so addesses of csv_values shall not be modified
    ASSERT_EQ(field100, &table3.content().front().front());
    ASSERT_EQ(field200, &table3.content().back().front());
}

template <class ContentLR>
struct TestStoredTableMerge : furfurylic::test::BaseTest
{};

typedef testing::Types<
    std::pair<
        std::vector<std::vector<stored_value>>,
        std::deque<std::vector<stored_value>>>,
    std::pair<
        std::deque<std::deque<stored_value>>,
        std::deque<std::vector<stored_value>>>,
    std::pair<
        std::list<std::deque<stored_value>>,
        std::deque<std::vector<stored_value>>>,
    std::pair<
        std::list<std::vector<stored_value>>,
        std::deque<std::vector<stored_value>>>> ContentLRs;

TYPED_TEST_CASE(TestStoredTableMerge, ContentLRs);

TYPED_TEST(TestStoredTableMerge, Merge)
{
    basic_stored_table<typename TypeParam::first_type> table1(20U);
    table1.content().emplace_back();
    table1.content().begin()->resize(3);
    table1.rewrite_value((*table1.content().begin())[0], "Lorem");
    table1.rewrite_value((*table1.content().begin())[1], "ipsum");
    table1.rewrite_value((*table1.content().begin())[2], "dolor");

    basic_stored_table<typename TypeParam::second_type> table2(25U);
    table2.content().resize(2);
    table2.content().begin() ->resize(2);
    table2.content().rbegin()->resize(1);
    table2.rewrite_value((*table2.content().begin()) [0], "sit");
    table2.rewrite_value((*table2.content().begin()) [1], "amet,");
    table2.rewrite_value((*table2.content().rbegin())[0], "consectetur");

    table1 += std::move(table2);
    ASSERT_EQ(3U, table1.size());
    ASSERT_EQ("Lorem"      , (*table1.content().cbegin())           [0]);
    ASSERT_EQ("ipsum"      , (*table1.content().cbegin())           [1]);
    ASSERT_EQ("dolor"      , (*table1.content().cbegin())           [2]);
    ASSERT_EQ("sit"        , (*std::next(table1.content().cbegin()))[0]);
    ASSERT_EQ("amet,"      , (*std::next(table1.content().cbegin()))[1]);
    ASSERT_EQ("consectetur", (*table1.content().crbegin())          [0]);
}

struct TestStoredTableAllocator : BaseTest
{};

TEST_F(TestStoredTableAllocator, Basics)
{
    using AA = tracking_allocator<std::allocator<char>>;
    using A = std::scoped_allocator_adaptor<AA>;

    using Record = std::vector<
        stored_value,
        typename std::allocator_traits<A>::
            template rebind_alloc<stored_value>>;
    using Content = std::deque<
        Record,
        typename std::allocator_traits<A>::template rebind_alloc<Record>>;

    std::vector<std::pair<char*, char*>> allocated1;
    AA a(allocated1);
    basic_stored_table<Content, A> table(std::allocator_arg, a, 1024);
    {
        const char* s = "Col1,Col2\n"
                        "aaa,bbb,ccc\n"
                        "AAA,BBB,CCC\n";
        std::stringbuf in(s);
        try {
            parse_csv(&in, make_stored_table_builder(table));
        } catch (const text_error& e) {
            FAIL() << e.info();
        }
    }

    ASSERT_TRUE(a == table.content().get_allocator());
    ASSERT_TRUE(a == table.content().front().get_allocator());
    ASSERT_TRUE(a.tracks(&table.content().front()));
    ASSERT_TRUE(a.tracks(&table.content().front().front()));
    ASSERT_TRUE(a.tracks(&table.content().front().front().front()));

    std::vector<std::pair<char*, char*>> allocated2;
    AA b(allocated2);
    basic_stored_table<Content, A> table2(std::allocator_arg, b);
    {
        const char* s = "Col1,Col2\n"
                        "xxx,yyy\n";
        std::stringbuf in(s);
        try {
            parse_csv(&in, make_stored_table_builder(table2));
        } catch (const text_error& e) {
            FAIL() << e.info();
        }
        table2.content().pop_front();
    }

    table += std::move(table2); // Not move but copy because of allocators

    ASSERT_EQ(1U, table2.size());
    ASSERT_TRUE(b.tracks(&table2[0]));
    ASSERT_TRUE(b.tracks(&table2[0].front()));
    ASSERT_TRUE(b.tracks(&table2[0].front().front()));

    table2.clear();

    ASSERT_EQ(4U, table.size());
    ASSERT_TRUE(a.tracks(&table[3]));
    ASSERT_TRUE(a.tracks(&table[3].front()));
    ASSERT_TRUE(a.tracks(&table[3].front().front()));
}

static_assert(std::is_nothrow_move_constructible<
    stored_table_builder<wstored_table::content_type,
        std::allocator<wchar_t>>>::value, "");
static_assert(std::is_nothrow_move_constructible<
    stored_table_builder<stored_table::content_type,
        std::allocator<char>, true>>::value, "");

struct TestStoredTableBuilder :
    furfurylic::test::BaseTestWithParam<std::size_t>
{};

TEST_P(TestStoredTableBuilder, Basics)
{
    const char* s = "\r\n\n"
                    "\"key_a\",key_b,value_a,value_b\n"
                    "ka1,\"kb\"\"01\"\"\",va1,\n"
                    "ka2,\"\",\"\"\"va2\"\"\",vb2\n"
                    "\"k\"\"a\"\"1\",\"kb\"\"13\"\"\",\"vb\n3\"";
    std::stringbuf in(s);
    stored_table table(GetParam());
    try {
        parse_csv(&in, make_stored_table_builder(table));
    } catch (const text_error& e) {
        FAIL() << e.info();
    }

    ASSERT_EQ(4U, table.size());
    ASSERT_EQ(4U, table[0].size());
    ASSERT_EQ("key_a",    table[0][0]);
    ASSERT_EQ("key_b",    table[0][1]);
    ASSERT_EQ("value_a",  table[0][2]);
    ASSERT_EQ("value_b",  table[0][3]);
    ASSERT_EQ(4U, table[1].size());
    ASSERT_EQ("ka1",      table[1][0]);
    ASSERT_EQ("kb\"01\"", table[1][1]);
    ASSERT_EQ("va1",      table[1][2]);
    ASSERT_EQ("",         table[1][3]);
    ASSERT_EQ(4U, table[2].size());
    ASSERT_EQ("ka2",      table[2][0]);
    ASSERT_EQ("",         table[2][1]);
    ASSERT_EQ("\"va2\"",  table[2][2]);
    ASSERT_EQ("vb2",      table[2][3]);
    ASSERT_EQ(4U, table.size());
    ASSERT_EQ(3U, table[3].size());
    ASSERT_EQ("k\"a\"1",  table[3][0]);
    ASSERT_EQ("kb\"13\"", table[3][1]);
    ASSERT_EQ("vb\n3",    table[3][2]);
}

TEST_P(TestStoredTableBuilder, EmptyRowAware)
{
    const char* s = "\r1,2,3,4\na,b\r\n\nx,y,z\r\n\"\"";
    std::stringbuf in(s);
    stored_table table(GetParam());
    try {
        parse_csv(&in, make_empty_physical_row_aware(
            make_stored_table_builder(table)));
    } catch (const text_error& e) {
        FAIL() << e.info();
    }

    ASSERT_EQ(6U, table.size());
    ASSERT_EQ(0U, table[0].size());
    ASSERT_EQ(4U, table[1].size());
    ASSERT_EQ("1", table[1][0]);
    ASSERT_EQ("2", table[1][1]);
    ASSERT_EQ("3", table[1][2]);
    ASSERT_EQ("4", table[1][3]);
    ASSERT_EQ(2U, table[2].size());
    ASSERT_EQ("a", table[2][0]);
    ASSERT_EQ("b", table[2][1]);
    ASSERT_EQ(0U, table[3].size());
    ASSERT_EQ(3U, table[4].size());
    ASSERT_EQ("x", table[4][0]);
    ASSERT_EQ("y", table[4][1]);
    ASSERT_EQ("z", table[4][2]);
    ASSERT_EQ(1U, table[5].size());
    ASSERT_EQ("", table[5][0]);
}

TEST_P(TestStoredTableBuilder, Transpose)
{
    const char* s = "Col1,Col2\n"
                    "aaa,bbb,ccc\n"
                    "AAA,BBB,CCC\n";
    std::stringbuf in(s);
    stored_table table(GetParam());
    try {
        parse_csv(&in, make_transposed_stored_table_builder(table));
    } catch (const text_error& e) {
        FAIL() << e.info();
    }

    ASSERT_EQ(3U, table.size());
    ASSERT_EQ(3U, table[0].size());
    ASSERT_EQ("Col1", table[0][0]);
    ASSERT_EQ("aaa", table[0][1]);
    ASSERT_EQ("AAA", table[0][2]);
    ASSERT_EQ(3U, table[1].size());
    ASSERT_EQ("Col2", table[1][0]);
    ASSERT_EQ("bbb", table[1][1]);
    ASSERT_EQ("BBB", table[1][2]);
    ASSERT_EQ(3U, table[2].size());
    ASSERT_EQ("", table[2][0]);
    ASSERT_EQ("ccc", table[2][1]);
    ASSERT_EQ("CCC", table[2][2]);

    const char* t = "AAa,BBb";
    std::stringbuf in2(t);
    try {
        parse_csv(&in2, make_transposed_stored_table_builder(table));
    } catch (const text_error& e) {
        FAIL() << e.info();
    }

    ASSERT_EQ(3U, table.size());
    ASSERT_EQ(4U, table[0].size());
    ASSERT_EQ("AAa", table[0][3]);
    ASSERT_EQ(4U, table[1].size());
    ASSERT_EQ("BBb", table[1][3]);
    ASSERT_EQ(4U, table[2].size());
    ASSERT_EQ("", table[2][3]);
}

INSTANTIATE_TEST_CASE_P(,
    TestStoredTableBuilder, testing::Values(2, 11, 1024));
