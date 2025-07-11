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
        constexpr str(const std::array<char, N> & s) {
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
    template <std::size_t N> str(const std::array<char, N> & s) -> str<N>;

    // This is to link a template constexpr to a char array reference that's usable in parsing context
    // This is equivalent to template <typename Type, Type S> to be used as template <typename str<N>, str<N> value>
    template <const auto S>
    struct str_ref {
        constexpr static auto & instance = S;
        constexpr static auto & data = S.data;
    };


    constexpr char tolower(const char c) { return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c; }
    constexpr int strncasecmp(const char *s1, const char *s2, int n)
    {
        if (n && s1 != s2)
        {
            do {
                int d = tolower(*s1) - tolower(*s2);
                if (d || *s1 == '\0' || *s2 == '\0') return d;
                s1++;
                s2++;
            } while (--n);
        }
        return 0;
    }
    constexpr int strncmp(const char *s1, const char *s2, int n)
    {
        if (n && s1 != s2)
        {
            do {
                int d = *s1 - *s2;
                if (d || *s1 == '\0' || *s2 == '\0') return d;
                s1++;
                s2++;
            } while (--n);
        }
        return 0;
    }

    constexpr size_t strlen(const char *s1)
    {
        size_t i = 0;
        while(s1[i]) ++i;
        return i;
    }


    unsigned constexpr constHash(char const * input)
    {
        return *input ? static_cast<unsigned>(*input) + 257 * constHash( input + 1 ) : 5381;
    }
    unsigned constexpr constHash(char const * input, std::size_t len)
    {
        return len ? static_cast<unsigned>(*input) + 257 * constHash( input + 1, len - 1 ) : 5381;
    }

    unsigned constexpr constHashCI(char const * input)
    {
        return *input ? static_cast<unsigned>(tolower(*input)) + 257 * constHash( input + 1 ) : 5381;
    }
    unsigned constexpr constHashCI(char const * input, std::size_t len)
    {
        return len ? static_cast<unsigned>(tolower(*input)) + 257 * constHash( input + 1, len - 1 ) : 5381;
    }


    unsigned constexpr operator ""_hash( const char* str, size_t len )
    {
        return constHash( str );
    }

    namespace Literals
    {
        using ::CompileTime::operator ""_hash;
    }
}

#endif
