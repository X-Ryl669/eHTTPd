#include <stdio.h>

// We are testing method parsing and header parsing in this file, so let's include the required stuff
#include "Protocol/HTTP/Methods.hpp"


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
    if (!testEqual("DELETE", Protocol::HTTP::toString(Protocol::HTTP::DELETE))) return 1;
    if (!testEqual("GET", Protocol::HTTP::toString(Protocol::HTTP::GET))) return 1;
    if (!testEqual("HEAD", Protocol::HTTP::toString(Protocol::HTTP::HEAD))) return 1;
    if (!testEqual("POST", Protocol::HTTP::toString(Protocol::HTTP::POST))) return 1;
    if (!testEqual("PUT", Protocol::HTTP::toString(Protocol::HTTP::PUT))) return 1;
    if (!testEqual("OPTIONS", Protocol::HTTP::toString(Protocol::HTTP::OPTIONS))) return 1;


    return 0;

}
