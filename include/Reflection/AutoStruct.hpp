#ifndef hpp_AutoStruct_hpp
#define hpp_AutoStruct_hpp

#include <cstddef>
#include <functional>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

#include "AutoName.hpp"

namespace Refl
{
	// Workaround for DR 2518 for C++20 compiler that' don't implement it
	template <class...>	struct AlwaysFalse : std::false_type {};
	template <class... T> constexpr bool always_false_v = AlwaysFalse<T...>::value;

    inline namespace Declval
    {
        #ifdef __clang__
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wundefined-var-template"
        #endif

        template <class T> extern T not_exists; // NOLINT

        template <class T> consteval T declval() { return std::forward<T>(not_exists<std::remove_reference_t<T>>); }

        #ifdef __clang__
        #pragma clang diagnostic pop
        #endif
    }

    /** The basic idea here to provide a minimum of reflection to simple aggregate types is:
        1. An aggregate type (like `struct A { int a; float b; };` ) can be constructed like this: `A a{0, 0};`
        2. Equivalently, it can't be constructed like `A a{0}` or `A a{0, 0, 0}`
        3. So it's possible to deduce the number of fields in the aggregate by trying to construct with any number of argument until the right number that'll work

        Then, once we know the number of arguments, we can extract their type similarly (try to construct with some type until works)
        To extract the fields' name, we are using the following trick:
        1. Build a new type that's taking pointers to the original struct's type, like this: `struct APtr { int A::* pa, float A::* pb; };` and construct it like
           `APtr ap = {&A::a, &A::b};`
        Then pass this object to a template function and ask the compiler for the pretty function name. The compiler is kind enough to track the pointer original name `A::a` so
        strip the class name and you get the field's name.

        There are few caveat to this marvelous

    */

    namespace Size
    {
        // This is used to compute an identity type for something that's not constructible, it force the compiler to convert the type and as such, lowers its match score to accept better construction
        struct ubiq
        {
            std::size_t ignore;
            template <class Type> constexpr operator Type &() const noexcept { return declval<Type &>(); }
        };

        // Same thing for the getting the base type of a type (will only exist for converting to the base of T and not T itself)
        template <class T> struct ubiq_base
        {
            std::size_t ignore;
            template <class Type>
                requires(std::is_base_of_v<std::remove_cvref_t<Type>, std::remove_cv_t<T>>
                        && !std::is_same_v<std::remove_cvref_t<Type>, std::remove_cv_t<T>>)
            constexpr operator Type &() const noexcept { return declval<Type &>(); }
        };

    #ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wmissing-field-initializers"
    #endif

        // For any array type T, construct a (T*)[N] type
        template <class T, std::size_t... I, class = typename std::enable_if<std::is_copy_constructible<T>::value>::type>
        consteval typename std::add_pointer<decltype(T{ubiq{I}...})>::type enable_if_constructible(std::index_sequence<I...>) noexcept;

    #ifdef __clang__
    #pragma clang diagnostic pop
    #endif

        // Samething for building a (T*)[N] for base type
        template <class T, std::size_t... B, std::size_t... I>
        consteval typename std::add_pointer<decltype(T(ubiq_base<T>{B}..., ubiq{I}...))>::type enable_if_constructible_base(std::index_sequence<B...>, std::index_sequence<I...>) noexcept;

        // Dichotomic search for the number of fields in the struct
        template <template <class, std::size_t, class...> typename, class T, std::size_t Begin, std::size_t Middle> requires(Begin == Middle)
        consteval std::size_t detect_fields_count(int) noexcept
        {
            return Begin;
        }

        template <template <class, std::size_t, class...> typename, class T, std::size_t Begin, std::size_t Middle> requires(Begin < Middle)
        consteval std::size_t detect_fields_count(std::size_t) noexcept;

        template <template <class, std::size_t, class...> typename EnableIf, class T, std::size_t Begin, std::size_t Middle> requires(Begin < Middle && !std::is_void_v<EnableIf<T, Middle>>)
        consteval auto detect_fields_count(int) noexcept
        {
            return detect_fields_count<EnableIf, T, Middle, Middle + (Middle - Begin + 1) / 2>(0);
        }

        template <template <class, std::size_t, class...> typename EnableIf, class T, std::size_t Begin, std::size_t Middle> requires(Begin < Middle)
        consteval std::size_t detect_fields_count(std::size_t) noexcept
        {
            return detect_fields_count<EnableIf, T, Begin, Begin + (Middle - Begin) / 2>(0);
        }

        // Will exists only if T[N] is constructible
        template <class T, std::size_t N, class = decltype(enable_if_constructible<T>(std::make_index_sequence<N>()))>
        using if_constructible_t = std::size_t;

        // Check the number of base class in the given type
        template <std::size_t M> struct FixMax
        {
            template <class T, std::size_t N, class = decltype(enable_if_constructible_base<T>(std::make_index_sequence<N>(), std::make_index_sequence<std::min(M - N, M)>()))> requires(N <= M)
            using if_constructible_base_t = std::size_t;
        };

        // Get the number of element to aggregate initialize T, by splitting those for the bases of T and those of the members of T
        template <class T> consteval std::pair<std::size_t, std::size_t> aggregate_count() noexcept
        {
            using type = std::remove_cvref_t<T>;
            if constexpr (std::is_empty_v<type> || std::is_polymorphic_v<type> || !std::is_aggregate_v<type> || std::is_scalar_v<type>) {
                return {};
            }
            else {
                constexpr auto fields = detect_fields_count<if_constructible_t, T, 0, 19>(0);

                constexpr auto bases = detect_fields_count<FixMax<fields>::template if_constructible_base_t, T, 0, fields>(0);
                return {bases, fields - bases};
            }
        }


        /*  From reflect-cpp:
            We infer the number of fields using by figuring out how many fields
            we need to construct it. This is done by implementing the constructible
            concept, see below.

            However, there is a problem with C arrays. Suppose you have a struct
            like this:

            struct A{
                int arr[3];
            };

            Then, the struct can be initialized like this:

            const auto a = A{1, 2, 3};

            This is a problem, because a naive logic would believe that A
            has three fields, when in fact it has only one.

            That is why we use the constructible concept to get the maximum
            possible number of fields and then try to subdivide them into arrays
            in order to figure out which of these fields is in fact an array.

            Basically, for every field there is, we try to squeeze as many variables into
            the potential array as we can without missing variables in subsequent fields.
            This is the purpose of get_nested_array_size().
            */
        template <class Derived>
        struct any_empty_base {
            any_empty_base(std::size_t);
            template <class Base>
            requires(std::is_empty_v<std::remove_cvref_t<Base>> && std::is_base_of_v<std::remove_cvref_t<Base>,	std::remove_cv_t<Derived>> && !std::is_same_v<std::remove_cvref_t<Base>, std::remove_cv_t<Derived>>)
            constexpr operator Base&() const noexcept;
        };

        template <class Derived>
        struct any_base {
            any_base(std::size_t);
            template <class Base>
            requires(std::is_base_of_v<std::remove_cvref_t<Base>, std::remove_cv_t<Derived>> && !std::is_same_v<std::remove_cvref_t<Base>, std::remove_cv_t<Derived>>)
            constexpr operator Base&() const noexcept;
        };
        // A wrapper that's accepting to convert to anything
        struct any {
            any(std::size_t);
            template <typename T> constexpr operator T() const noexcept;
        };

        template <typename T>
        struct CountFieldsHelper {
            // Check if T is constructible with n "any" parameters
            template <std::size_t n>
            static consteval bool constructible() {
                return []<std::size_t... is>(std::index_sequence<is...>) { return requires { T{any(is)...}; }; }
                (std::make_index_sequence<n>());
            }

            // Check if T is constructible with l+nested+r "any" parameters and nested is sub-constructed
            template <std::size_t l, std::size_t nested, std::size_t r>
            static consteval bool constructible_with_nested() {
                return []<std::size_t... i, std::size_t... j, std::size_t... k>(
                    std::index_sequence<i...>, std::index_sequence<j...>,
                    std::index_sequence<k...>) { return requires { T{any(i)..., {any(j)...}, any(k)...}; }; }
                (std::make_index_sequence<l>(), std::make_index_sequence<nested>(),	std::make_index_sequence<r>());
            }

            // Returns the maximum number of arguments useable via aggregate initialization (recursively)
            template <std::size_t n = 0, bool check_bitfield = false>
            static consteval std::size_t count_max_args_in_agg_init() {
                if constexpr (!check_bitfield)
                {	// This method of counting might failed for variant, so let's try a failsafe in that case
                    if (n >= static_cast<std::size_t>(sizeof(T)))
                        return aggregate_count<T>().second;
                }
                if constexpr (constructible<n>() && !constructible<n + 1>()) {
                    return n;
                } else {
                    return count_max_args_in_agg_init<n + 1, check_bitfield>();
                }
            }
            // Get the number of arguments in a nested array aggregate initialization starting at index (recursively)
            template <std::size_t index, std::size_t size, std::size_t rest>
            static consteval std::size_t get_nested_array_size() {
                if constexpr (size < 1) {
                    return 1;
                } else if constexpr (constructible_with_nested<index, size, rest>() && !constructible_with_nested<index, size, rest + 1>()) {
                    return size;
                } else {
                    return get_nested_array_size<index, size - 1, rest + 1>();
                }
            }

            // Find the sole non-empty base index in initialization
            template <std::size_t max_args, std::size_t index = 0>
            static consteval std::size_t find_the_sole_non_empty_base_index() {
                static_assert(index < max_args);
                constexpr auto check = []<std::size_t... l, std::size_t... r>(std::index_sequence<l...>, std::index_sequence<r...>) {
                    return requires { T{any_empty_base<T>(l)..., any_base<T>(0), any_empty_base<T>(r)...}; };
                };

                if constexpr (check(std::make_index_sequence<index>(), std::make_index_sequence<max_args - index - 1>())) {
                    return index;
                } else {
                    return find_the_sole_non_empty_base_index<max_args, index + 1>();
                }
            }

            template <std::size_t arg_index, std::size_t size = 0>
            static consteval std::size_t get_nested_base_field_count() {
                static_assert(size <= sizeof(T));
                if constexpr (constructible_with_nested<arg_index, size, 0>() && !constructible_with_nested<arg_index, size + 1, 0>()) {
                    return size;
                } else {
                    return get_nested_base_field_count<arg_index, size + 1>();
                }
            }

            template <std::size_t n, std::size_t max_arg_num>
            static consteval bool has_n_base_param() {
                constexpr auto right_len = max_arg_num>=n ? max_arg_num-n : 0;
                return []<std::size_t... l, std::size_t... r>(std::index_sequence<l...>, std::index_sequence<r...>) {
                    return requires { T{any_base<T>(l)..., any(r)...}; };
                }(std::make_index_sequence<n>(), std::make_index_sequence<right_len>());
            }

            template <std::size_t max_arg_num, std::size_t index = 0>
            static consteval std::size_t base_param_num() {
                if constexpr (!has_n_base_param<index + 1, max_arg_num>()) {
                    return index;
                } else {
                    return base_param_num<max_arg_num, index + 1>();
                }
            }

            template <std::size_t index, std::size_t max>
            static consteval std::size_t constructible_no_brace_elision() {
                static_assert(index <= max);
                if constexpr (index == max)
                    return 0;
                else
                    return 1 + constructible_no_brace_elision<index + get_nested_array_size<index, max - index, 0>(), max>();
            }

            static consteval std::size_t count_fields() {
                constexpr std::size_t max_agg_args = count_max_args_in_agg_init();
                constexpr std::size_t no_brace_ellison_args = constructible_no_brace_elision<0, max_agg_args>();
                constexpr std::size_t base_args = base_param_num<no_brace_ellison_args>();
                // Empty struct
                if constexpr (no_brace_ellison_args == 0 && base_args == 0)
                    return 0;
                else if constexpr (base_args == no_brace_ellison_args)
                    // Special case when the derived class is empty.
                    // In such cases the filed number is the fields in base class.
                    // Note that there should be only one base class in this case.
                    return get_nested_base_field_count<find_the_sole_non_empty_base_index<max_agg_args>()>();
                else
                    return no_brace_ellison_args - base_args;
            }
            static consteval std::size_t count_fields_in_bitfield() {
                constexpr std::size_t max_agg_args = count_max_args_in_agg_init<0, true>();
                constexpr std::size_t no_brace_ellison_args = constructible_no_brace_elision<0, max_agg_args>();
                constexpr std::size_t base_args = base_param_num<no_brace_ellison_args>();
                if constexpr (no_brace_ellison_args == 0 && base_args == 0) {
                    // Empty struct
                    return 0;
                } else if constexpr (base_args == no_brace_ellison_args) {
                    // Special case when the derived class is empty.
                    // In such cases the filed number is the fields in base class.
                    // Note that there should be only one base class in this case.
                    return get_nested_base_field_count<find_the_sole_non_empty_base_index<max_agg_args>()>();
                } else {
                    return no_brace_ellison_args - base_args;
                }
            }
        };

        template <class T>
        constexpr std::size_t num_fields = CountFieldsHelper<T>::count_fields();
        template <class T>
        constexpr std::size_t num_fields_in_bitfield = CountFieldsHelper<T>::count_fields_in_bitfield();

    }

namespace Loophole
{

#if !defined(__clang__) && defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-template-friend"
#endif

template <class T, std::size_t N> struct tag
{
	friend auto loophole(tag<T, N>);
};

#if !defined(__clang__) && defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

template <class T, class U, std::size_t N, bool B> struct fn_def
{
	friend auto loophole(tag<T, N>)
	{
		return static_cast<std::add_pointer_t<U>>(nullptr);
	}
};
template <class T, class U, std::size_t N>
struct fn_def<T, U, N, true>
{};

template <class T, std::size_t N, bool base = false>
struct loophole_ubiq
{
	template <class U, std::size_t M> static std::size_t ins(...);
	template <class U,
	    std::size_t M,
	    std::size_t = sizeof(loophole(tag<T, M>{}))>
	static char ins(int);

	template <class U,
	    std::size_t = sizeof(
	        fn_def<T, U, N, sizeof(ins<U, N>(0)) == sizeof(char)>)>
	    requires(!base
	             || (std::is_base_of_v<std::remove_cvref_t<U>,
	                     std::remove_cv_t<T>>
	                 && !std::is_same_v<std::remove_cvref_t<U>,
	                     std::remove_cv_t<T>>))
	// NOLINTNEXTLINE
	constexpr operator U &&() const && noexcept
	{
		return declval<U &&>();
	}
};

template <class T,
    class U =
        std::make_index_sequence<Size::aggregate_count<T>().first>,
    class V =
        std::make_index_sequence<Size::aggregate_count<T>().second>>
struct loophole_type_list;

template <typename T, std::size_t... B, std::size_t... Ix>
struct loophole_type_list<T,
    std::index_sequence<B...>,
    std::index_sequence<Ix...>> :
    std::tuple<decltype(T{loophole_ubiq<T, B, true>{}...,
                            loophole_ubiq<T, sizeof...(B) + Ix>{}...},
        0)>
{
	using type = std::tuple<std::tuple<std::remove_reference_t<
	                            decltype(*loophole(tag<T, B>{}))>...>,
	    std::tuple<std::remove_reference_t<decltype(*loophole(
	        tag<T, sizeof...(B) + Ix>{}))>...>>;
};

template <class T>
using aggregate_types_t = typename Loophole::loophole_type_list<
    std::remove_cvref_t<T>>::type;
}

    template <class T>
    using bases_t =
        std::tuple_element_t<0, Loophole::aggregate_types_t<T>>;

    template <class T>
    using members_t =
        std::tuple_element_t<1, Loophole::aggregate_types_t<T>>;

    template <std::size_t Ix>
    using index_t = std::integral_constant<std::size_t, Ix>;

    namespace Impl
    {

    template <class T,
        class U = std::remove_cv_t<T>,
        class Members = members_t<U>,
        std::size_t = std::tuple_size_v<Members>>
    constexpr inline bool same_as_decomposed = false;

    template <class T, class U, class Members>
    constexpr inline bool same_as_decomposed<T, U, Members, 1> =
        std::is_same_v<std::remove_cv_t<std::tuple_element_t<0, Members>>,
            U>;
    }

    enum class structure_bindable { no, through_base, through_members };

    template <class T,
        class = bases_t<T>,
        bool = Impl::same_as_decomposed<T>,
        bool = !std::is_empty_v<T>,
        size_t = std::tuple_size_v<members_t<T>>>
    constexpr inline structure_bindable is_structure_bindable_v =
        structure_bindable::no;

    template <class T, size_t members, class... Base>
    constexpr inline structure_bindable is_structure_bindable_v<T,
        std::tuple<Base...>,
        false,
        true,
        members> =
        static_cast<structure_bindable>(
            (std::is_empty_v<Base> && ...) * 2);

    template <class T, class... Base>
        requires(sizeof...(Base) > 0)
    constexpr inline structure_bindable
        is_structure_bindable_v<T, std::tuple<Base...>, false, true, 0> =
            static_cast<structure_bindable>(
                ((2
                    * (1 - std::is_empty_v<Base>)-static_cast<bool>(
                        is_structure_bindable_v<Base>))
                    + ... + 0)
                <= 1);

    template <class T,
        bool = static_cast<bool>(is_structure_bindable_v<T>),
        class Bases = bases_t<T>>
    constexpr inline std::size_t structure_binding_size_v{};

    template <class T, class... Base>
    constexpr inline std::size_t
        structure_binding_size_v<T, true, std::tuple<Base...>> = Size::num_fields<std::decay_t<T>>;

    namespace Detail
    {
        template <std::size_t Ix>
        using index_t = std::integral_constant<std::size_t, Ix>;
        template <typename T>
        concept Aggregate = std::is_aggregate_v<T>;
        template <typename T>
        concept NotAggregate = !std::is_aggregate_v<T>;
        template <typename T>
        concept CArray = std::is_array_v<T>;

        template <typename T>
        concept AggregateArray = requires {
            requires Refl::Size::num_fields_in_bitfield<std::decay_t<T>> != Refl::Size::aggregate_count<std::decay_t<T>>().second;
        };

        template <typename T, typename Enable = void>
        struct isaggregatearray : std::false_type {};

        template <AggregateArray T>
        struct isaggregatearray<T> : std::true_type {};

        template <typename T> constexpr bool isaggregatearray_v = isaggregatearray<T>::value;
    }

    #ifndef __clang_analyzer__
    namespace Members
    {
        // Convert a type to a tuple of reference to its fields based on the number of fields it owns
        template <class T> constexpr inline auto get_members(T &&t, index_t<1>) noexcept { auto &[_0] = t; return std::forward_as_tuple(_0); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<2>) noexcept { auto &[_0, _1] = t; return std::forward_as_tuple(_0, _1); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<3>) noexcept { auto &[_0, _1, _2] = t; return std::forward_as_tuple(_0, _1, _2); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<4>) noexcept { auto &[_0, _1, _2, _3] = t; return std::forward_as_tuple(_0, _1, _2, _3); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<5>) noexcept { auto &[_0, _1, _2, _3, _4] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<6>) noexcept { auto &[_0, _1, _2, _3, _4, _5] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<7>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<8>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6, _7] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6, _7); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<9>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6, _7, _8] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6, _7, _8); }

        template <class T> constexpr inline auto get_members(T &&t, index_t<10>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6, _7, _8, _9] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<11>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<12>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<13>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<14>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<15>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<16>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<17>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<18>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<19>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<20>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<21>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<22>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<23>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<24>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23); }
        template <class T> constexpr inline auto get_members(T &&t, index_t<25>) noexcept { auto &[_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24] = t; return std::forward_as_tuple(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24); }

        template <class T, std::size_t I>
        constexpr inline auto get_members(T &&, index_t<I>) noexcept
        {
            static_assert(I == 0, "Not implemented");
        }

        template <class T> constexpr inline auto get_members(T &&t) noexcept
        {
            if constexpr(Detail::isaggregatearray_v<T>)
                // Don't count the field via destructured binding if one of them is an array
                return get_members(t, index_t<Refl::Size::num_fields<std::decay_t<T>>>{});
            else
                return get_members(t, index_t<std::tuple_size_v<members_t<T>>>());
        }

        template <class From, class = void>
        constexpr inline bool specified_members = false;

        template <class From>
        constexpr inline bool specified_members<From, std::enable_if_t<std::tuple_size_v<decltype(From::members())> != 0 && (std::tuple_size_v<members_t<From>> != 0 || !std::is_aggregate_v<From>)>> = []
        {
            static_assert(!std::is_aggregate_v<From> || std::tuple_size_v<decltype(From::members())> == std::tuple_size_v<members_t<From>>, "Not specified all member");
            return true;
        }();

        template <class From, std::size_t Ix, bool through_memptrs = specified_members<From>>
        struct MemberFunctor;

        template <class From, std::size_t Ix> requires(!std::is_reference_v<From> && !std::is_const_v<From>)
        struct MemberFunctor<From, Ix, true>
        {
            template <class F = From>
            constexpr static inline decltype(auto) get(const F &t)
            {
                constexpr auto G = std::get<Ix>(F::members());
                return std::invoke(G, t);
            }

            template <class F = From>
            using type = std::remove_cvref_t<decltype(get(std::declval<F>()))>;

            template <class F = From>
            consteval static inline ROString name()
            {
                return Refl::Name::in_data_name<Refl::Name::name<decltype(std::get<Ix>(F::members())), std::get<Ix>(F::members())>()>;
            }
        };

        template <class From, std::size_t Ix> requires(!std::is_reference_v<From> && !std::is_const_v<From> && is_structure_bindable_v<From> == structure_bindable::through_members)
        struct MemberFunctor<From, Ix, false>
        {
            constexpr static inline decltype(auto) get(const From &t)
            {
                return std::get<Ix>(get_members(t));
            }

            template <class = void>
            using type = std::remove_cvref_t<decltype(get(std::declval<From>()))>;

            template <class = void>
            consteval static inline ROString name()
            {
                return Refl::Name::in_data_name<Refl::Name::name<void, Name::Wrapper{&std::get<Ix>(get_members(declval<From &>()))}>()>;
            }
        };

        template <class T, std::size_t... Ix>
        consteval auto get_members_by_bind(std::index_sequence<Ix...>)
        {
            return std::tuple<MemberFunctor<T, Ix>...>{};
        }

        template <class T, std::size_t... Ix>
        consteval auto get_members_by_memptrs(std::index_sequence<Ix...>)
        {
            if constexpr ((std::is_same_v<std::remove_cvref_t<decltype(std::get<Ix>(T::members()))>, std::remove_cvref_t<decltype(std::ignore)>> || ...)) {
                using X = decltype(std::tuple_cat(
                    std::declval<std::conditional_t<
                        std::is_same_v<std::remove_cvref_t<decltype(std::get<
                                        Ix>(T::members()))>,
                            std::remove_cvref_t<decltype(std::ignore)>>,
                        std::tuple<>,
                        std::tuple<index_t<Ix>>>>()...));

                constexpr auto sequence = std::apply(
                    []<std::size_t... Ixs>(index_t<Ixs>...)
                    {
                        return std::index_sequence<Ixs...>{};
                    },
                    X{});
                return get_members_by_memptrs<T>(sequence);
            }
            else {
                return std::tuple<MemberFunctor<T, Ix>...>{};
            }
        }

        template <class T>
            requires(std::tuple_size_v<decltype(T::members())> != 0
                    && (std::tuple_size_v<members_t<T>> != 0
                        || !std::is_aggregate_v<T>))
        consteval auto get_member_functors(std::nullptr_t)
        {
            static_assert(!std::is_aggregate_v<T>
                            || std::tuple_size_v<decltype(T::members())>
                                    == std::tuple_size_v<members_t<T>>,
                "Not specified all member");
            return get_members_by_memptrs<T>(std::make_index_sequence<
                std::tuple_size_v<decltype(T::members())>>{});
        }

        template <class T>
            requires(is_structure_bindable_v<T>
                    == structure_bindable::through_members)
        consteval auto get_member_functors(void *)
        {
            return get_members_by_bind<T>(
                std::make_index_sequence<structure_binding_size_v<T>>{});
        }

        template <class T>
            requires(is_structure_bindable_v<T>
                        != structure_bindable::through_members
                    && std::tuple_size_v<members_t<T>> == 0
                    && 0 < std::tuple_size_v<bases_t<T>>)
        consteval auto get_member_functors(void *)
        {
            return std::tuple{};
        }

        template <class T>
            requires(is_structure_bindable_v<T> == structure_bindable::no
                    && std::tuple_size_v<members_t<T>> == 0
                    && std::tuple_size_v<bases_t<T>> == 0
                    && !std::is_empty_v<T>)
        consteval auto get_member_functors(void *)
        {
            static_assert(!std::is_polymorphic_v<T>);
            static_assert(std::is_aggregate_v<T>);
            return std::tuple{};
        }
    }

    namespace Functors
    {
    namespace Composite
    {
    template <class T, class U>
    constexpr inline auto operator>>(U &&arg, const T &)
        -> decltype(T::get(std::forward<U &&>(arg)))
    {
        return T::get(std::forward<U &&>(arg));
    }
    }

    template <class T, class U, class... MemberFunctors>
    constexpr static inline auto FuncPtr = +[](const T &t) -> U &
    {
        using Composite::operator>>;
        auto &v = (t >> ... >> MemberFunctors{});
        return const_cast<std::remove_cvref_t<decltype(v)> &>( // NOLINT
            v);
    };

    template <class... MemberFunctors>
    constexpr static inline std::initializer_list<ROString>
        name_list{MemberFunctors::name()...};

    template <class T, class Visitor> struct Applier
    {
        template <class U, class... MemberFunctors>
            requires(!std::is_same_v<T, U>
                    && std::is_invocable_v<Visitor &,
                        U &(*)(const T &),
                        const std::initializer_list<ROString> &>)
        constexpr static inline __attribute__((always_inline)) void apply(
            Visitor &v) noexcept
        {
            v(FuncPtr<T, U, MemberFunctors...>,
                name_list<MemberFunctors...>);
        }

        template <class U, class... MemberFunctors>
            requires(!std::is_same_v<T, U>
                    && std::is_invocable_v<Visitor &, U &(*)(const T &)>
                    && !std::is_invocable_v<Visitor &,
                        U &(*)(const T &),
                        const std::initializer_list<ROString> &>)
        constexpr static inline __attribute__((always_inline)) void apply(
            Visitor &v) noexcept
        {
            v(FuncPtr<T, U, MemberFunctors...>);
        }

        template <class U, class... MemberFunctors>
        constexpr static inline __attribute__((always_inline)) void apply(
            [[maybe_unused]] Visitor &v) noexcept
        {
            if constexpr (!std::is_empty_v<U>) {
                if constexpr (std::tuple_size_v<bases_t<U>> > 0) {
                    std::invoke(
                        [&v]<class... Bases>(std::tuple<Bases...> *)
                        {
                            (Functors::Applier<T,
                                Visitor>::template apply<Bases,
                                MemberFunctors...>(v),
                                ...);
                        },
                        std::add_pointer_t<bases_t<U>>{});
                }

                if constexpr (constexpr auto members =
                                Members::get_member_functors<U>(
                                    nullptr);
                            std::tuple_size_v<decltype(members)> > 0) {
                    std::apply(
                        [&v]<class... MF>(MF...)
                        {
                            (Functors::Applier<T, Visitor>::
                                    template apply<
                                        typename MF::template type<>,
                                        MemberFunctors...,
                                        MF>(v),
                                ...);
                        },
                        members);
                }
            }
        }
    };

    template <class Visitor,
        class Tuple,
        class = std::make_index_sequence<std::tuple_size_v<Tuple>>>
    struct GetterVisitor;

    template <class Visitor, class... Ts, std::size_t... Ix>
    struct GetterVisitor<Visitor,
        std::tuple<Ts...>,
        std::index_sequence<Ix...>>
    {
        Visitor &visitor;
        std::tuple<Ts...> ts;

        constexpr inline GetterVisitor(Visitor &p_visitor,
            std::tuple<Ts...> t) :
            visitor(p_visitor),
            ts(std::move(t))
        {}

        template <class Getter>
        constexpr inline std::invoke_result_t<Visitor &,
            std::invoke_result_t<Getter, Ts>...>
        operator()(Getter &&getter) const noexcept
        {
            return std::invoke(visitor,
                std::invoke(getter, std::get<Ix>(ts))...);
        }

        template <class Getter>
        constexpr inline std::invoke_result_t<Visitor &,
            std::invoke_result_t<Getter, Ts>...,
            const std::initializer_list<ROString> &>
        operator()(Getter &&getter,
            const std::initializer_list<ROString> &sv = {})
            const noexcept
        {
            return std::invoke(visitor,
                std::invoke(getter, std::get<Ix>(ts))...,
                sv);
        }
    };

    }
    #endif

    template <class T, class Visitor>
    constexpr inline __attribute__((always_inline)) auto visit(
        [[maybe_unused]] Visitor &&visitor)
    {
    #ifndef __clang_analyzer__
        return Functors::Applier<T, Visitor>::template apply<T>(visitor);
    #endif
    }

    template <class Visitor, class T, class... Ts>
        requires((std::is_same_v<std::remove_cvref_t<T>,
                    std::remove_cvref_t<Ts>>
                && ...))
    constexpr inline void visit([[maybe_unused]] Visitor &&visitor,
        [[maybe_unused]] T &visitable,
        [[maybe_unused]] Ts &&...ts)
    {
    #ifndef __clang_analyzer__
        using TT = std::remove_cvref_t<T>;
        visit<TT>(
            Functors::GetterVisitor<Visitor, std::tuple<T &, Ts &...>>{
                visitor,
                {visitable, ts...}});
    #endif
    }

}

#endif
