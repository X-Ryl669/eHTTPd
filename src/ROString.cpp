#include "ROString.hpp"

size_t ROString::Find(const ROString & needle, size_t pos) const
{
    for (size_t j = 0; pos + j < length;)
    {
        if (needle.data[j] == data[pos + j])
        {
            j++;
            if (j == needle.length) return pos;
            continue;
        }
        pos++;
        j = 0;
    }
    return length;
}
size_t ROString::reverseFind(const ROString & needle, size_t pos) const
{
    if (needle.length > length) return length;
    size_t i = min(pos, (length - needle.length)); // If there is no space to find out the needle at the end, simply snap back
    for (size_t j = 0;;)
    {
        if (needle.data[j] == data[i + j])
        {
            j ++;
            if (j >= needle.length) return i;
        } else
        {
            if (i-- == 0) break;
            j = 0;
        }
    }
    return length;
}

size_t ROString::Count(const ROString & needle) const
{
    size_t pos = 0; size_t count = 0;
    while ((pos = Find(needle, pos)) != length) { count++; pos++; }
    return count;
}

ROString ROString::splitFrom(const ROString & find, const bool includeFind)
{
    const size_t pos = Find(find);
    if (pos == length)
    {
        if (includeFind)
        {
            ROString ret(*this);
            (void)Mutate(data + length, 0);
            return ret;
        }
        return ROString("", 0);
    }
    const size_t size = pos + find.length;
    ROString ret(data, (int)(includeFind ? size : pos));
    (void)Mutate(data + size, length - size);
    return ret;
}

ROString ROString::splitAt(int pos, int stripFromRet)
{
    if (stripFromRet > pos) stripFromRet = pos;
    if (pos < 0) return ROString();
    ROString ret(data, min(pos - stripFromRet, (int)length));
    if ((size_t)pos > length) (void)Mutate(data, 0);
    else (void)Mutate(data + pos, length - (size_t)pos);
    return ret;
}


ROString ROString::fromTo(const ROString & from, const ROString & to, const bool includeFind) const
{
    const size_t fromPos = Find(from);
    const size_t toPos = Find(to, fromPos + from.length);
    return ROString(fromPos >= length ? "" : &data[includeFind ? fromPos : fromPos + from.length],
    (int)(toPos < length ? (includeFind ? toPos + to.length - fromPos : toPos - fromPos - from.length)
    // If the "to" needle was not found, either we return the whole string (includeFind) or an empty string
                         : (includeFind ? length - fromPos : 0)));
}

// Get the string up to the first occurrence of the given string
ROString ROString::upToFirst(const ROString & find, const bool includeFind) const
{
    const size_t pos = Find(find);
    return ROString(pos == length && includeFind ? "" : data, (int)(includeFind ? (pos == length ? 0 : pos + find.length) : pos));
}
// Get the string up to the last occurrence of the given string
ROString ROString::upToLast(const ROString & find, const bool includeFind) const
{
    const size_t pos = reverseFind(find);
    return ROString(pos == length && includeFind ? "" : data, (int)(includeFind ? (pos == length ? 0 : pos + find.length) : pos));
}
// Get the string from the last occurrence of the given string.
ROString ROString::fromLast(const ROString & find, const bool includeFind) const
{
    const size_t pos = reverseFind(find);
    return ROString(pos == length ? (includeFind ? data : "") : &data[includeFind ? pos : pos + find.length],
    (int)(pos == length ? (includeFind ? length : 0) : (includeFind ? length - pos : length - pos - find.length)));
}
// Get the string from the first occurrence of the given string
ROString ROString::fromFirst(const ROString & find, const bool includeFind) const
{
    const size_t pos = Find(find);
    return ROString(pos == length ? (includeFind ? data : "") : &data[includeFind ? pos : pos + find.length],
    (int)(pos == length ? (includeFind ? length : 0)
                        : (includeFind ? length - pos
                                       : length - pos - find.length)));
}
// Get the string from the first occurrence of the given string
ROString ROString::dropUpTo(const ROString & find, const bool includeFind) const
{
    const size_t pos = Find(find);
    return ROString(pos == length ? data : &data[includeFind ? pos : pos + find.length],
    (int)(pos == length ? length : (includeFind ? length - pos
                                                : length - pos - find.length)));
}
// Get the substring up to the given needle if found, or the whole string if not, and split from here.
ROString ROString::splitUpTo(const ROString & find, const bool includeFind)
{
    const size_t pos = Find(find);
    if (pos == length)
    {
        ROString ret(*this);
        (void)Mutate(data + length, 0);
        return ret;
    }
    const size_t size = pos + find.length;
    ROString ret(data, (int)(includeFind ? size : pos));
    (void)Mutate(data+size, length - size);
    return ret;
}

inline void StrToWrapper(long & r, const char* str, char ** end, int base) { r = strtol(str, end, base); }
inline void StrToWrapper(double & r, const char* str, char ** end, int) { r = strtod(str, end); }
inline void StrToWrapper(unsigned long & r, const char* str, char ** end, int base) { r = strtoul(str, end, base); }

template <typename T>
T ParseWrapper(const char * data, const size_t length, int * consumed = 0, const int base = 0)
{
    char * str = 0;
    if (length < 64) str = (char*)alloca(length + 1);
    else             str = (char*)calloc(1, length + 1);

    memcpy(str, data, length); str[length] = 0;
    char * end = 0; T out = 0; StrToWrapper(out, str, &end, base);
    if (consumed) *consumed = (int)(end - str);
    if (length >= 64) free(str);
    return out;
}

int ROString::parseInt(const int base, int * consumed) const { return (int)ParseWrapper<long>(data, length, consumed, base); }
double ROString::parseDouble(int * consumed) const { return ParseWrapper<double>(data, length, consumed); }



/** The basic conversion operators */
ROString::operator int() const
{
    return parseInt();
}
/** The basic conversion operators */
ROString::operator size_t() const
{
    return (size_t)ParseWrapper<unsigned long>(data, length);
}
#ifndef __amd64__
/** The basic conversion operators */
ROString::operator uint32() const
{
    return (uint32)ParseWrapper<unsigned long>(data, length);
}
#endif
/** The basic conversion operators */
ROString::operator double() const
{
    return parseDouble();
}

