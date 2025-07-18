#ifndef hpp_Types_hpp
#define hpp_Types_hpp

#include <string.h>
#include <stdint.h>
#include <bitset>
// For enable_if and is_enum
#include <type_traits>
// For numeric_limits
#include <limits>

// Uncomment the line below to tell the software about enabled JTAG (this will disable features depending on JTAG's pin 12-15)
#ifdef JTAGEnabled
    /** Check whether the given pin number is a JTAG pin */
    inline bool isJTAG(int pin) { return pin >= 12 && pin <= 15; }
    #pragma message("Building JTAG version")
#endif

// Define usual types size (this needs to be fixed on every platform)
typedef uint64_t uint64;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t  uint8;
typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;
typedef int64_t  int64;

// Declare useful helpers functions
/** Compute the minimum between the 2 given parameters */
template <typename T>
static inline T min(T a, T b) { return a < b ? a : b; }
/** Compute the maximum between the 2 given parameters */
template <typename T>
static inline T max(T a, T b) { return a > b ? a : b; }
/** Clamp the first parameter in range specified by the second and third parameter
    @param a    The value to clamp
    @param m    The minimum of the range to clamp into
    @param M    The maximum of the range to clamp into
    @return a if a is > m and < M, m if it's <= m and M if it's >= M */
template <typename T>
static inline T clamp(T a, T m, T M) { return a < m ? m : (a > M ? M : a); }
/** Useful lookup in a static array */
template <typename T, size_t N >
inline bool isInArray(const T & a, T (&arr)[N]) { for(size_t i = 0; i < N; i++) if (arr[i] == a) return true; return false; }


// The deleters helpers
/** Delete and zero the pointer */
template <typename T>
inline void delete0(T*& t) { delete t; t = 0; }
/** Delete array and zero the pointer */
template <typename T>
inline void deleteA0(T*& t) { delete[] t; t = 0; }
/** Free and zero the pointer */
template <typename T>
inline void free0(T*& t) { free(t); t = 0; }


/** Divide by a (power of 2) - 1 with no actual division
    Using this trick, let: (2^n - 1) = (a - b), and since (a-b)(a+b) = a^2 - b^2
     x / (2^n - 1) = x * (2^n + 1) / [(2^n + 1)*(2^n - 1)] = (x*2^n + x) / (2^(2n) - 1) ~= (x<<n + x) >> (2*n)
    Since 2^(2n) - 1 ~= 2^2n, we commit a smaller error than if we divided by shifting right initially by n.
    So, for example, for x in [0-65536] range, and dividing by 255, this approximation gives no error
    See https://godbolt.org/z/ss7xra9e4 for benchmark, it's a tad faster than a simple division (or multiplication by reciprocal and shift)
    @param x        The number to divide by (2^power - 1)
    @param power    The power that used to compute 2^power - 1, for example, it's 8 for 255 */
inline uint32 divPowerOfTwoMinus1(uint32 x, uint8 power)
{
    uint32 y = x+1;
    uint32 num = ((y << power) + y);
    return num >> (2*power);
}

/** Simple macro to return the number of element in a static array */
#define ArrSz(X)    (sizeof(X) / sizeof(X[0]))

template <typename T>
static inline void Zero(T & t) { memset(&t, 0, sizeof(t)); }


template<typename Enum, bool IsEnum = std::is_enum<Enum>::value>
struct Bitflag;

template<typename Enum>
struct Bitflag<Enum, true>
{
    typedef typename std::conditional< (std::numeric_limits<typename std::underlying_type<Enum>::type>::digits > 32), uint64, uint32>::type Underlying;

    constexpr Bitflag() = default;
    constexpr Bitflag(Enum value) : bits(static_cast<Underlying>(value)) {}
    constexpr Bitflag(const Bitflag& other) : bits(other.bits) {}
    constexpr Bitflag& operator=(Enum value) { bits = static_cast<Underlying>(value); return *this; }
    constexpr Bitflag& operator=(const Bitflag & value) { bits = value.bits; return *this; }

    constexpr Bitflag operator|(Enum value) const { Bitflag result = *this; result.bits |= static_cast<Underlying>(value); return result; }
    constexpr Bitflag operator&(Enum value) const { Bitflag result = *this; result.bits &= static_cast<Underlying>(value); return result; }
    constexpr Bitflag operator^(Enum value) const { Bitflag result = *this; result.bits ^= static_cast<Underlying>(value); return result; }
    constexpr Bitflag operator~() const { Bitflag result = *this; result.bits = ~result.bits; return result; }

    constexpr Bitflag& operator|=(Enum value) { bits |= static_cast<Underlying>(value); return *this; }
    constexpr Bitflag& operator&=(Enum value) { bits &= static_cast<Underlying>(value); return *this; }
    constexpr Bitflag& operator^=(Enum value) { bits ^= static_cast<Underlying>(value); return *this; }

    constexpr bool any() const { return bits > 0; }
    constexpr bool all() const { return bits == (Underlying)-1; }
    constexpr bool none() const { return bits == 0; }
    constexpr explicit operator bool() const { return any(); }

    constexpr bool test(Enum value) const { return (bits & static_cast<Underlying>(value)) > 0; }
    constexpr void set(Enum value) { bits |= static_cast<Underlying>(value); }
    constexpr void unset(Enum value) { bits &= ~static_cast<Underlying>(value); }

    constexpr operator Underlying() const { return bits; }

private:
    Underlying bits;
};

template<typename Enum>
constexpr typename std::enable_if<std::is_enum<Enum>::value, Bitflag<Enum>>::type operator|(Enum left, Enum right)
{
    return Bitflag<Enum>(left) | right;
}
template<typename Enum>
constexpr typename std::enable_if<std::is_enum<Enum>::value, Bitflag<Enum>>::type operator&(Enum left, Enum right)
{
    return Bitflag<Enum>(left) & right;
}
template<typename Enum>
constexpr typename std::enable_if_t<std::is_enum<Enum>::value, Bitflag<Enum>>::type operator^(Enum left, Enum right)
{
    return Bitflag<Enum>(left) ^ right;
}

/** Compute, without branching, the same as:
    @code
        if (enable) reg |= mask; else reg &= ~mask;
    @endcode */
template <typename T>
void setResetBits(T & reg, const bool enable, const T mask)
{
    reg = (reg & ~mask) | (-(!!enable) & mask);
}
/** Compute, without branching, the same as:
    @code
        if (enable) reg |= (1<<bit); else reg &= ~(1<<bit);
    @endcode */
template <typename T>
void setResetBit(T & reg, const bool enable, const uint8 bit) { return setResetBits(reg, enable, ((T)1)<<bit); }


inline uint16 EndianSwap(uint16 value) { return (value >> 8) | (value << 8) ; }
inline uint32 EndianSwap(uint32 value) { value = ((value << 8) & 0xFF00FF00 ) | ((value >> 8) & 0xFF00FF ); return (value >> 16) | (value << 16) ; }
inline uint64 EndianSwap(uint64 value)
{
    value = ((value << 8) & 0xFF00FF00FF00FF00ULL ) | ((value >> 8) & 0x00FF00FF00FF00FFULL );
    value = ((value << 16) & 0xFFFF0000FFFF0000ULL ) | ((value >> 16) & 0x0000FFFF0000FFFFULL );
    return (value >> 32) | (value << 32);
}

/** A cross platform bitfield class that should be used in union like this:
    @code
    union
    {
        T whatever;
        BitField<T, 0, 1> firstBit;
        BitField<T, 7, 1> lastBit;
        BitField<T, 2, 2> someBits;
    };
    @endcode */
template <typename T, int Offset, int Bits>
struct BitField
{
    /** This is public to avoid undefined behavior while used in union */
    T value;

    static_assert(Offset + Bits <= (int) sizeof(T) * 8, "Member exceeds bitfield boundaries");
    static_assert(Bits < (int) sizeof(T) * 8, "Can't fill entire bitfield with one member");

    /** Base constants are typed to T so we skip type conversion everywhere */
    static const T Maximum = (T(1) << Bits) - 1;
    static const T Mask = Maximum << Offset;

    /** Main access operator, use like any other member */
    inline operator T() const { return (value >> Offset) & Maximum; }
    /** Assign operator */
    inline BitField & operator = (T v) { value = (value & ~Mask) | (v << Offset); return *this; }
};



#endif
