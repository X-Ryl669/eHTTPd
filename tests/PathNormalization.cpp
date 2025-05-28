#include <stdio.h>

#include "../src/Normalization.cpp"
#include "../include/Strings/RWString.hpp"


bool testEqual(const ROString & a, const char * b, size_t i)
{
    if (a != ROString(b))
    {
        fprintf(stderr, "Failed test for %s, got %.*s (%lu)\n", b, a.getLength(), a.getData(), i);
        return false;
    }
    return true;
}


int main()
{
    RWString pathsToNormalize[] = { "C:/windows", "../../C:/.././a/b/.../", "/some/strange/path", "/some/../a/strange/path/", "/some/bad/trick/./..", "./././././././././././././", "a/b/../../c/d/../e/../../f/g", "bother" };
    const char * expected[] = { "C:/windows", "/a/b/...", "/some/strange/path", "/a/strange/path", "/some/bad", "/", "/f/g", "bother" };

    for (size_t i = 0; i < sizeof(expected)/sizeof(*expected); i++)
    {
        ROString a = pathsToNormalize[i];
        if (!testEqual(Path::normalize(a, false), expected[i], i)) return 1;
    }


    RWString pathsToNormalize2[] = { "ab%20cd", "admin%2527--%20", "%62%61%64%2E%65%78%61%6D%70%6C%65%2E%63%6F%6D" };
    const char * expected2[] = { "ab cd", "admin%27-- ", "bad.example.com" };

    for (size_t i = 0; i < sizeof(expected2)/sizeof(*expected2); i++)
    {
        ROString a = pathsToNormalize2[i];
        if (!testEqual(Path::normalize(a, true), expected2[i], i)) return 1;
    }

    printf("OK\n");
    return 0;
}