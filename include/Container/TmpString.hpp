#ifndef hpp_TmpString_hpp
#define hpp_TmpString_hpp

// We need read-only strings
#include "../Strings/ROString.hpp"
// We need the ring buffer declaration too
#include "RingBuffer.hpp"


namespace Container
{
    /** A TmpString is a read-only string whose storage is dynamically allocated from a RingBuffer.
        Deallocation isn't managed by the class itself, but by the ring buffer instance.
        It's typically used to avoid heap allocation and memory fragmentation.
        Typical example is for a network "client" that need to persist its current state
        while it's receiving a request (and flushing its network's buffers).
        The client owns the ring buffer and release all of the temporary allocations at once in a single move */
    template <std::size_t N>
    static bool persistString(ROString & stringToPersist, Container::FixedSize<N> & buffer)
    {
        const char * t = buffer.saveString(stringToPersist.getData(), stringToPersist.getLength());
        if (!t) return false;
        ROString tmp(t, stringToPersist.getLength());
        stringToPersist.swapWith(tmp);
        return true;
    }

    /** A basic size limited buffer with content tracking */
    template <std::size_t N>
    struct TrackedBuffer
    {
        TrackedBuffer(char (&buffer)[N]) : buffer(buffer), used(0) {}
        bool save(const char * data, const std::size_t length) { if (used + length > N) return false; memcpy(&buffer[used], data, length); used += length; return true; }
        char (&buffer)[N];
        std::size_t used;
        static constexpr std::size_t size = N;
    };
}

#endif
