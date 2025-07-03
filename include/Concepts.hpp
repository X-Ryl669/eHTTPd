#ifndef hpp_Concepts_hpp
#define hpp_Concepts_hpp

#include <initializer_list>
#include <type_traits>

/** A collection of useful concept that's simplifying the code a lot */
namespace Concepts
{
    template <typename>                     struct IsStdInitList : std::false_type { };
    template <typename T>                   struct IsStdInitList<std::initializer_list<T>> : std::true_type { };
    /** Check if the given type is an initializer list */
    template <typename T>                   constexpr bool is_stdinitlist_v = IsStdInitList<T>::value;
}

#endif
