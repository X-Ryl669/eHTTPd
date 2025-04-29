#include <stdio.h>

// We are testing method parsing and header parsing in this file, so let's include the required stuff
#include "Protocol/HTTP/Methods.hpp"

using namespace Protocol::HTTP;


template <typename A>
bool testEqual(const A a, const Refl::Opt<A> b)
{
    if (a != b.orElse((A)-1))
    {
        fprintf(stderr, "Failed test for %d, got %d\n", (int)a, (int)b.get());
        return false;
    }
    return true;
}

bool testEqual(const char * a, const char * b)
{
    if (ROString(a) != ROString(b))
    {
        fprintf(stderr, "Failed test for %s, got %s\n", a, b);
        return false;
    }
    return true;
}


int main()
{
    if (!testEqual("DELETE",    toString(DELETE))) return 1;
    if (!testEqual("GET",       toString(GET))) return 1;
    if (!testEqual("HEAD",      toString(HEAD))) return 1;
    if (!testEqual("POST",      toString(POST))) return 1;
    if (!testEqual("PUT",       toString(PUT))) return 1;
    if (!testEqual("OPTIONS",   toString(OPTIONS))) return 1;

    if (!testEqual(Protocol::HTTP::DELETE, fromString<Method>("DELETE"))) return 1;
    if (!testEqual(Protocol::HTTP::DELETE, fromString<Method>("delete"))) return 1;
    if (!testEqual(Protocol::HTTP::DELETE, fromString<Method>("DELETe"))) return 1;
    fprintf(stderr, "Expecting failure here: ");
    if (testEqual(Protocol::HTTP::DELETE, fromString<Method>("DELETEn"))) return 1;
    if (!testEqual(Protocol::HTTP::GET, fromString<Method>("GET"))) return 1;
    if (!testEqual(Protocol::HTTP::HEAD, fromString<Method>("HEAD"))) return 1;
    if (!testEqual(Protocol::HTTP::POST, fromString<Method>("POST"))) return 1;
    if (!testEqual(Protocol::HTTP::PUT, fromString<Method>("PUT"))) return 1;
    if (!testEqual(Protocol::HTTP::OPTIONS, fromString<Method>("OPTIONS"))) return 1;


    fprintf(stdout, "OK\n");
    return 0;

}
