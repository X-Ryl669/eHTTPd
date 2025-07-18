#ifndef hpp_AutoEnum_hpp
#define hpp_AutoEnum_hpp

#ifdef __cplusplus

// We need various std functions
#include <algorithm>
#include <functional>
#include <type_traits>
#include <concepts>
#include <utility>
#include <limits>
// We need ROString for value to enum matching
#include "../Strings/ROString.hpp"

// The minimal negative value for enumeration to search for
#define MinNegativeValueForEnum      16
// The maximal positive value for enumeration to look for
#define MaxPositiveValueForEnum      112

/** Useful concept used in many part of the library */
template <typename T> concept Enum = std::is_enum_v<T>;

namespace Refl
{
    namespace Details { template <auto str> struct Wrapper { static constexpr auto v = str; }; template <typename ... T> constexpr bool always_false = false;  }

    constexpr const char * find(const char * str, char l, std::size_t off = 0, char avoid = 0)
    {
        std::size_t p = 0; for(; str[p] && str[p] != l; p++)
            if (str[p] == avoid) return "";
        return str[p] ? &str[p+off] : nullptr;
    }
    constexpr const char * rfind(const char * str, char l, std::size_t off = 0, char avoid = 0)
    {
        std::size_t p = 0; for(; str[p]; p++) {}
        for(;p && str[p] != l; p--)
            if (str[p] == avoid) return "";
        return p ? &str[p+off] : nullptr;
    }

    constexpr bool strequal(const char * s1, const char * s2)
    {
        for(std::size_t index = 0;; index++)
        {
            if (s1[index] != s2[index]) return false;
            if (!s1[index] && !s2[index]) return true;
            if (!s1[index] || !s2[index]) return false;
        }
    }


    #ifdef __clang__
        // Clang can detect invalid enum value via SFINAE but choke on constexpr conversion out of the allowed enum values
        template <typename Enum, int V, Enum X = static_cast<Enum>(V)>
        constexpr bool enum_exists(int) { return true; }

        template <typename Enum, int V>
        constexpr bool enum_exists(...) { return false; }
    #endif

    template<typename enm, auto $>
    requires std::is_enum_v<enm>
    constexpr const char * enum_value_exist()
    {
        constexpr const char * str = __PRETTY_FUNCTION__;
        constexpr const char * p = find(str, '$');
        constexpr const char * n = rfind(p, ':', 1, ')');
        if (n == nullptr) return find(p, '=', 2, ']');
        return n;
    }

    template<typename enm, auto $>
    requires std::is_enum_v<enm>
    constexpr auto enum_raw_name_only()
    {
    #ifdef __clang__
        if constexpr (enum_exists<enm, $>(0)) {
    #endif
        constexpr const char * n = enum_value_exist<enm, static_cast<enm>($)>();
        if (*n == 0) return "";
        constexpr const char * e = *n == 0 ? n : find(n, ']');
        constexpr auto range = e - n;
        constexpr auto arr = [&n]<std::size_t ... i>(std::index_sequence<i ...>){ return std::array<char, range + 1>{*(n+i) ...}; }(std::make_index_sequence<range>{});
        return &Details::Wrapper<arr>::v[0];
    #ifdef __clang__
        } else return "";
    #endif
    }

    template<typename enm, auto $>
    requires std::is_enum_v<enm>
    constexpr auto enum_raw_name_only_str()
    {
    #ifdef __clang__
        if constexpr (enum_exists<enm, $>(0)) {
    #endif
        constexpr const char * n = enum_value_exist<enm, static_cast<enm>($)>();
        if constexpr (*n == 0) return CompileTime::str("");
        constexpr const char * e = *n == 0 ? n : find(n, ']');
        constexpr auto range = e - n;
        constexpr auto arr = [&n]<std::size_t ... i>(std::index_sequence<i ...>){ return std::array<char, range + 1>{*(n+i) ...}; }(std::make_index_sequence<range>{});
        return CompileTime::str(Details::Wrapper<arr>::v);
    #ifdef __clang__
        } else return CompileTime::str("");
    #endif
    }

    template<typename enm, std::size_t neg_range = MinNegativeValueForEnum, std::size_t range = 128>
    requires std::is_enum_v<enm>
    constexpr auto enum_value_names() {
        constexpr auto map = []<std::size_t ... i>(std::index_sequence<i ...>) { return std::array<const char*, range+1+neg_range>{enum_raw_name_only<enm, (int)i-(int)neg_range>()...}; }(std::make_index_sequence<range+1+neg_range>{});
        return map;
    };

    template <typename enm, std::size_t i>
    requires std::is_enum_v<enm>
    constexpr std::size_t find_max_value()
    {
        constexpr bool exists = *enum_value_exist<enm, (enm)(i+1)>() != 0;
        if constexpr (exists) return find_max_value<enm, i+1>();
        else return i;
    }

    template <typename enm, int i>
    requires std::is_enum_v<enm>
    constexpr int find_min_value()
    {
        constexpr bool exists = *enum_value_exist<enm, (enm)(i-1)>() != 0;
        if constexpr (exists) return find_min_value<enm, i-1>();
        else return i;
    }


    /** Get the enumeration name as string.
        This is valid for any sparse enum whose value fall in [-MinNegativeValueForEnum ; MaxPositiveValueForEnum]
        @warning This is usually slow and produce a larger binary size, since the compiler is building an array of the given range size for storing the name for each values */
    template<typename enm>
    requires std::is_enum_v<enm>
    constexpr const char* sparse_value_name(enm const e) { return ((int)e < -MinNegativeValueForEnum || (int)e > MaxPositiveValueForEnum) ? "" : enum_value_names<enm>()[static_cast<std::size_t>(e + MinNegativeValueForEnum)]; }


    /** Get the enumeration name as string for any value.
        This is valid if the enumeration isn't sparse (from the minimum value to its maximum value, it's monotonically increasing and contiguous)
        The function automatically search for the minimum and maximum of the enum values.
        This provides the fastest and most compact form since the produced name array only contains values that exists
        @param min_starts_from      If provided, this start decreasing search for the min value from this
        @param max_starts_from      If provided, this start increasing search for the max value from this */
    template<typename enm, int min_starts_from = 0, std::size_t max_starts_from = 0>
    requires std::is_enum_v<enm>
    constexpr const char* enum_value_name(enm const e) {
        constexpr int min_val = find_min_value<enm, min_starts_from>();
        constexpr std::size_t max_val = find_max_value<enm, max_starts_from>();
        return ((int)e < (int)min_val || (int)e > (int)max_val) ? "" : enum_value_names<enm, (std::size_t)-min_val, max_val>()[static_cast<std::size_t>(e - min_val)];
    };

    /** Get the enumeration value for the given textual form, or the fallback value if not found
        @param min_starts_from      If provided, this start decreasing search for the min value from this
        @param max_starts_from      If provided, this start increasing search for the max value from this */
    template<typename enm, int min_starts_from = 0, std::size_t max_starts_from = 0>
    requires std::is_enum_v<enm>
    constexpr enm from_enum_value(const char * value, enm const orElse) {
        constexpr int min_val = find_min_value<enm, min_starts_from>();
        constexpr std::size_t max_val = find_max_value<enm, max_starts_from>();
        for (int i = min_val; i <= (int)max_val; i++)
            if (strequal(value, enum_value_names<enm, (std::size_t)-min_val, max_val>()[static_cast<std::size_t>(i - min_val)]))
                return (enm)i;
        return orElse;
    }


    /** Get the enumeration value for the given textual form, or the fallback value if not found
        @param min_starts_from      If provided, this start decreasing search for the min value from this
        @param max_starts_from      If provided, this start increasing search for the max value from this */
    template<typename enm, int min_starts_from = 0, std::size_t max_starts_from = 0>
    requires std::is_enum_v<enm>
    constexpr enm from_enum_value(ROString value, enm const orElse) {
        constexpr int min_val = find_min_value<enm, min_starts_from>();
        constexpr std::size_t max_val = find_max_value<enm, max_starts_from>();
        for (int i = min_val; i <= (int)max_val; i++)
            if (value == enum_value_names<enm, (std::size_t)-min_val, max_val>()[static_cast<std::size_t>(i - min_val)])
                return (enm)i;
        return orElse;
    }

    /** Compile time I=>J map with no runtime ressource solution below:

        Associating a value to a enum can be done with no resources like this:
        @code
            enum Something { This, Is, A, Test };
            enum SomethingValue { This_3, Is_1, A_4, Test_1 };
            static_assert(find_value_for_key<SomethingValue>(This)  == 3 && find_value_for_key<SomethingValue>(Is) == 1)
        @endcode
    */
    // Parse number at compile time
    constexpr unsigned parse_value(const char * str) {
        const char * p = find(str, '_', 1);
        if (!p || !p[0]) return 0;

        unsigned val = 0;
        for (std::size_t index = 0; p[index]; ++index) {
            char ch = p[index];
            if (ch < '0' || ch > '9') break;
            val = val * 10 + (ch - '0');
        }

        return val;
    }

    /** Check if the given string starts with the given key until a specific delimiter */
    constexpr bool starts_with(const char * first, const char * key, const char limit = '_')
    {
        for(std::size_t index = 0;; index++)
        {
            if (first[index] != key[index]) return first[index] == limit;
            if (!key[index] || !first[index]) return false;
        }
        return false;
    }
    /** Query the given map enumeration for the main enum key to extract the value stored in the name itself */
    template <typename enm>
    requires std::is_enum_v<enm>
    constexpr unsigned find_value_for_key(const char * str)
    {
        constexpr auto names = enum_value_names<enm, 0, find_max_value<enm, 0>()>();
        for (const char * i : names)
        {
            if (starts_with(i, str)) return parse_value(i);
        }
        return 0;
    }


    // The functions below are useful for enumerations that are contiguous, starting with 0, sorted alphabetically
    // Enumeration with value -1 is used as an error code, since it isn't reflected
    // In that case, they are giving the smallest binary size for reflection, and the highest runtime cost for conversion from string to enumeration

    /** This kind of method provides the smallest enum to string in the final binary size so they are preferred instead of hand made version and reusing Refl code every where */

    // This is to avoid C++23 here: static constexpr is not allowed in C++20 in function, only in struct
    namespace Details { template <Enum E, std::size_t ... i > struct Reflect { static constexpr std::array<const char*, sizeof...(i)> values = {Refl::enum_raw_name_only<E, (int)i>()...}; }; }

    template <Enum E> constexpr bool isCaseSensitive = true;
    template <Enum E> constexpr bool isSorted = false;
    template <Enum E> constexpr bool useHash = false;

    template <Enum E>
    constexpr inline auto &_supports()
    {
        constexpr auto maxV = Refl::find_max_value<E, 0>();
        return *[]<std::size_t ... i>(std::index_sequence<i...>) { return &Details::Reflect<E, i...>::values; }(std::make_index_sequence<maxV+1>{});
    }

    namespace Details { template <Enum E, std::size_t ... i > struct ReflectHashes {
        static constexpr std::array<const unsigned, sizeof...(i)> values = {
            (isCaseSensitive<E> ? CompileTime::constHash(_supports<E>()[i]) : CompileTime::constHashCI(_supports<E>()[i]))...
        };};}
    template <Enum E>
    constexpr inline auto &_allHashes()
    {
        constexpr auto maxV = Refl::find_max_value<E, 0>();
        return *[]<std::size_t ... i>(std::index_sequence<i...>) { return &Details::ReflectHashes<E, i...>::values; }(std::make_index_sequence<maxV+1>{});
    }
    template <Enum E>
    inline constexpr const char * toString(E m)
    {
        if constexpr (useHash<E>) {
            static_assert(Details::always_false<E> && "If you're using hashes to store the enum, it's only useful to save binary size (storing hash vs whole string). But as soon as you use toString, the whole string is saved and the hashes are just wasted space");
            return nullptr;
        } else {
            constexpr auto & sup = _supports<E>();
            return (unsigned)m < sup.size() ? sup[(size_t)m] : "";
        }
    }
    template <Enum E>
    struct Opt
    {
        Opt(E e) : value((Type)e) {}
        Opt() : value((Type)-1) {}

        explicit operator bool() const { return value != (Type)-1; }
        const bool inline isValid() const { return value != (Type)-1; }
        const E orElse(E other) const { return isValid() ? (E)value : other; }
        const E get() const { return (E)value; }
    private:
        typedef std::underlying_type_t<E> Type;
        Type value;
    };
    /** Convert a read-only string view to an enum or -1 if not found. Use Opt::orElse or Opt::isValid to check if the value is valid or not.
        This method is using a dichotomic search in the enumeration string value. The enumeration must not have a negative value (should start from 0) and be sorted alphabetically */
    template <Enum E>
    inline constexpr Opt<E> fromString(const ROString & string)
    {
        if constexpr (useHash<E>) {
            // Try O(N) search in the table of hash (sorry, we aren't sorting the table yet)
            // Storing the hashes should take less binary space than the whole string so it's a size vs performance optimization here
            // This doesn't make sense if you're using the toString for this enum, since toString involves storing the full strings here, so don't set useHash<E> = true in that case
            constexpr auto & sup = _allHashes<E>();
            unsigned h = 0;
            if constexpr (isCaseSensitive<E>) h = CompileTime::constHash(string.getData(), string.getLength());
            else h = CompileTime::constHashCI(string.getData(), string.getLength());

            for (auto i = 0; i < sup.size(); i++)
                if (h == sup[i]) return Opt<E>{(E)i};
        }
        else if constexpr (isSorted<E>) {
            // A dichotomic search into an enum name to value not performing O(log N) search here
            constexpr auto & sup = _supports<E>();
            const char * p = string.getData();
            size_t l = 0, r = sup.size() - 1;
            while(l <= r)
            {
                size_t m = l + (r - l) / 2;
                // Check if any transform is needed for this enumeration type
                if constexpr (isCaseSensitive<E>) {
                    int c = string.compare(sup[m]);
                    if (c == 0) return Opt<E>{(E)m};
                    if (c > 0) l = m + 1;
                    else if (m) r = m - 1;
                    else break;
                } else {
                    int c = string.compareCaseless(sup[m]);
                    if (c == 0) return Opt<E>{(E)m};
                    if (c > 0) l = m + 1;
                    else if (m) r = m - 1;
                    else break;
                }
            }
        } else {
            // Enum isn't sorted, so let's use a plain old O(N) search here
            constexpr auto & sup = _supports<E>();
            for (auto i = 0; i < sup.size(); i++)
            {
                if constexpr (isCaseSensitive<E>) {
                    if (!string.compare(sup[i])) return Opt<E>{(E)i};
                } else {
                    if (!string.compareCaseless(sup[i])) return Opt<E>{(E)i};
                }
            }
        }
        return Opt<E>{};
    }




}




#endif
#endif
