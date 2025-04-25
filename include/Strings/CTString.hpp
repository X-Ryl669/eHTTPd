#ifndef hpp_CT_String_hpp
#define hpp_CT_String_hpp


namespace CompileTime
{
    /** A Compile time string. This is used to store a char array in a way that the compiler can deal with */
    template <std::size_t N>
    struct str
    {
        static constexpr std::size_t size = N;
        char data[N] = {0};

        constexpr str(const char (&s)[N]) {
            for(std::size_t i = 0; i < N; i++)
            {
                if (!s[i]) break;
                data[i] = s[i];
            }
        }
        template <std::size_t M> constexpr str(const char (&s)[M], std::size_t offset) {
            for(std::size_t i = 0; i < N; i++)
            {
                if (!s[i+offset]) break;
                data[i] = s[i + offset];
            }
        }
        constexpr operator const char*() const { return data; }
    };

    /** Help the compiler deduce the type (with the number of bytes) from the given static array */
    template <std::size_t N> str(const char (&s)[N]) -> str<N>;

    // This is to link a template constexpr to a char array reference that's usable in parsing context
    // This is equivalent to template <typename Type, Type S> to be used as template <typename str<N>, str<N> value>
    template <const auto S>
    struct str_ref {
        constexpr static auto & instance = S;
        constexpr static auto & data = S.data;
    };



}

#endif
