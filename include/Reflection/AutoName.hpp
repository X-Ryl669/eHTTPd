#ifndef hpp_AutoName_hpp
#define hpp_AutoName_hpp

#include <algorithm>
#include <array>
#include <functional>
#include <string_view>
#include "ROString.hpp"

namespace Refl
{
    namespace Name
    {
        template <class T> struct Wrapper { T t; };
        template <class T> Wrapper(T) -> Wrapper<T>;

        template <class Base, class Member>
        static Base getBase(Member Base::*);

        template <class E, auto v> consteval auto name()
        {
            constexpr std::string_view sv = __PRETTY_FUNCTION__;
            constexpr auto last = sv.find_last_not_of(" }])");
            constexpr auto val = sv.find_last_not_of(   "abcdefghijklmnopqrstuvwxyz"
                                                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                        "0123456789_", last);
            if constexpr (constexpr std::string_view res = sv.substr(val + 1, last - val);
                            res.length() > 0 && (res[0] < '0' || res[0] > '9'))
            {
                std::array<char, res.size()> arr{};
                for(size_t i = 0; i < res.size(); i++) arr[i] = res[i];
                return arr;
            }
            else return std::array<char, 0>{};
        }

        template <auto A>
        constexpr static inline ROString in_data_name{std::data(A), std::size(A)};

        template <typename T>
        constexpr auto type() {
            std::string_view name, prefix, suffix;
            name = __PRETTY_FUNCTION__;
            prefix = "constexpr auto Refl::Name::type() [with T = ";
            suffix = "]";
            name.remove_prefix(prefix.size());
            name.remove_suffix(suffix.size());
            return ROString(name.data(), name.size());
        }
        template <typename T> constexpr auto type(T &&) { return type<T>(); }

    }
}

#endif
