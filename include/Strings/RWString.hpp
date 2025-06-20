#ifndef hpp_CPP_RWString_CPP_hpp
#define hpp_CPP_RWString_CPP_hpp

#include "ROString.hpp"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <utility>

/** Our Read write string class that's doing allocation (compared to ROString that's only a view on an existing buffer)
    This is using malloc/free/realloc underneath to try to be as efficient as possible.
    In general to avoid heap fragmentation, RWString should be short lived */
class RWString
{
private:
    size_t length;
    char * buffer;

    /** Append the given buffer to our buffer by reallocing */
    void append(const char * other, size_t l)
    {
        if (!realloc(length + l + 1)) return;
        memcpy(&buffer[length], other, l + 1);
        length += l;
    }

    /** Realloc the buffer to the given size */
    bool realloc(size_t newSize)
    {
        void * t = ::realloc(buffer, newSize);
        if (!t) { ::free(buffer); length = 0; }
        buffer = (char*)t;
        return t != 0;
    }

public:
    /** Construct a RWString from the given (optional) buffer and (optional) size.
        The string is copied so it's safe to use with temporary buffer.
        @param buffer   A pointer to a UTF-8 encoded buffer
        @param size     If provided, use this size instead of trying to find a the zero in the given buffer */
    RWString(const char * buffer = 0, const int size = -1)
        : length(size < 0 ? (buffer ? strlen(buffer) : 0) : (size_t)size), buffer((char*)malloc(length + 1))
    {
        if (buffer) memcpy(this->buffer, buffer, length+1);
        else *this->buffer = 0;
    }

    /** Construct a string from a buffer and a string len */
    RWString(const char * buffer, const size_t size)
        : length(size), buffer((char*)malloc(length + 1))
    {
        if (buffer) memcpy(this->buffer, buffer, length+1);
        else *this->buffer = 0;
    }

    /** Build a string from a compile-time based array.
        @warning Try to avoid using this as much as possible since this generate an instance of this method for each possible length */
    template <size_t N>
    RWString(const char (&data)[N]) : length(N-1), buffer((char*)malloc(N))
    {
        memcpy(buffer, data, N);
    }
    /** Copy constructor */
    RWString(const RWString & other) : length(other.length), buffer((char*)malloc(other.length+1)) { memcpy(buffer, other.buffer, other.length+1); }
    /** Move constructor */
    RWString(RWString && other) : length(other.length), buffer(other.buffer) { other.length = 0; other.buffer = 0; }
    /** Conversion constructor from a ROString */
    RWString(const ROString & other) : length(other.length), buffer((char*)malloc(other.length+1)) { memcpy(buffer, other.data, other.length); buffer[length] = 0; }

    /** Get the string length in bytes */
    size_t getLength() const { return length; }

    /** Useful equal operator */
    RWString & operator = (const char* other)
    {
        if (!other) { length = 0; if (realloc(1)) *buffer = 0; return *this; }
        length = strlen(other);
        if (realloc(length+1)) memcpy(buffer, other, length+1);
        return *this;
    }
    /** Copy operator */
    RWString & operator = (const RWString & other)
    {
        if (&other != this)
        {
            if (realloc(other.length+1))
            {
                length = other.length;
                memcpy(buffer, other.buffer, length+1);
            }
        }
        return *this;
    }
    /** Another copy operator for ROStrings */
    RWString & operator = (const ROString & other)
    {
        if (realloc(other.length+1))
        {
            length = other.length;
            buffer[length] = 0;
            memcpy(buffer, other.data, length);
        }
        return *this;
    }
    /** Destructor */
    ~RWString() { free(buffer); length = 0; }
    /** Allocate the given size in bytes for this string and return a pointer on the buffer.
        @param sizeInBytes      The size to allocate in bytes */
    char * allocate(const size_t sizeInBytes) { if (realloc(sizeInBytes)) length = (sizeInBytes - 1); return buffer; }
    /** Limit the string length to the given value */
    RWString & limitTo(const size_t len) { if (len < length) length = len; return *this; }
    /** Test if this string is empty */
    explicit operator bool() const { return length && buffer != 0; }
    /** This is just to mimic ROString access-to-member function */
    inline const char * getData() const { return buffer; }
    /** Convert this string to a C-style string implicitely.
        @warning This is only safe is you have stored a zero terminated string inside this class beforehand */
    operator const char * () const { return buffer; }
    /** Get a view on this string */
    operator ROString () const { return ROString(buffer, (int)length); }
    /** Another smaller toROString convertion */
    ROString toRO() { return ROString(buffer, (int)length); }
    /** Test string equality */
    bool operator == (const RWString & other) const { return length == other.length && memcmp(buffer, other.buffer, length) == 0; }
    /** Test string inequality */
    bool operator != (const RWString & other) const { return length != other.length || memcmp(buffer, other.buffer, length); }
    /** Test string equality */
    bool operator == (const char* other) const { return strcmp(buffer, other) == 0; }
    /** Test string inequality */
    bool operator != (const char* other) const { return strcmp(buffer, other); }
    /** Array access operator */
    char & operator[] (const int index) { static char bad = 0; return index <= (int)length ? buffer[index] : bad; }
    /** An empty string */
    static const RWString & empty() { static RWString empty; return empty; }
    /** Get a pointer on the internal buffer */
    inline char * map() { return buffer; }
    /** Swap this string with another one */
    void swapWith(RWString & other) { char * t = buffer; buffer = other.buffer; other.buffer = t; size_t l = length; length = other.length; other.length = l; }

    /** Concatenation operator */
    RWString & operator += (const char* other)
    {
        if (!other) return *this;
        append(other, strlen(other));
        return *this;
    }
    /** Concatenation operator */
    RWString & operator += (const char other)
    {
        append(&other, 1);
        return *this;
    }
    /** Concatenation operator */
    RWString & operator += (const RWString & other)
    {
        if (!other) return *this;
        append(other.buffer, other.length);
        return *this;
    }
    /** Concatenation operator */
    RWString & operator += (const ROString & other)
    {
        if (!other) return *this;
        append(other.data, other.length);
        return *this;
    }
    /** Concatenation operator */
    RWString operator + (const RWString & other) const { RWString c(*this); c += other; return c; }
    /** Concatenation operator */
    RWString operator + (const char*  other) const     { RWString c(*this); c += other; return c; }
    /** Concatenation operator */
    RWString operator + (const char   other) const     { RWString c(*this); c += other; return c; }
    /** Format a string using printf like format.
        @warning This isn't typesafe formatting here, so make sure about the argument passed in
        @warning This consumes 512 bytes of stack space, so make sure the task has such space available */
    static RWString format(const char * format, ...)
    {
        va_list argp;
        va_start(argp, format);
        char buf[512];
        // We use vasprintf extension to avoid dual parsing of the format string to find out the required length
        int err = vsnprintf(buf, sizeof(buf), format, argp);
        va_end(argp);
        if (err <= 0) return RWString();
        if (err >= (int)sizeof(buf)) err = (int)(sizeof(buf) - 1);
        buf[err] = 0;
        return RWString(buf, (size_t)err);
    }

    /** Copy a string into a fixed sized array */
    template <size_t N>
    bool copyInto(char (&_data)[N]) const { return ROString(buffer, length).copyInto(_data); }
    /** Copy a string into a fixed sized array */
    template <size_t N>
    bool copyInto(uint8 (&_data)[N]) const { return ROString(buffer, length).copyInto(_data); }

    /** Capture the given pointer that was allocated with malloc */
    inline RWString & capture(char * buf, const size_t len) { free(buffer); buffer = buf; length = len; return *this; }
    /** Hexdump the given buffer to a new string
        @param buffer   The buffer to dump
        @param len      The length of the buffer in byte
        @param sep      The separator char to use between bytes */
    static RWString hexDump(const uint8 * buffer, const size_t len, char sep = '\0')
    {
        // Optimized for code size, not speed
        size_t size = 2 + (sep != 0);
        RWString ret(0, len * size - (sep != 0));
        const char hexD[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };
        for (size_t i = 0; i < len; i++)
        {
            ret.buffer[i * size + 0 ] = hexD[buffer[i]>>4];
            ret.buffer[i * size + 1 ] = hexD[buffer[i] & 0xF];
            if (sep) ret.buffer[i * size + 2] = sep;
        }
        ret.buffer[len * size - (sep != 0)] = 0;
        return ret;
    }
};

// Useful method to concatenate read only strings too
inline RWString operator + (const ROString & lhs, const ROString & rhs) { RWString c(lhs); c += rhs; return c; }

inline char* intToStr(int value, char* result, int base)
{
    // check that the base if valid
    if (base < 2 || base > 36) { *result = '\0'; return result; }

    char* ptr = result, *ptr1 = result, tmp_char;
    int tmp_value;

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
    } while ( value );

    // Apply negative sign
    if (tmp_value < 0) *ptr++ = '-';
    *ptr-- = '\0';
    while(ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
    return result;
}

#endif
