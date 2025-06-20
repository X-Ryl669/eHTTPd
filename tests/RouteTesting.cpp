#include <stdio.h>
#include <type_traits>
#include <stdarg.h>

// We need request lines too for testing
#define SLog logger
static void logger(auto level, const char * msg, ...)
{
    static char levelStr[] = { 'D', 'I', 'W', 'E' };
    printf("[%c] ", levelStr[(int)level]);
    va_list vaargs;
    va_start(vaargs, msg);
    vprintf(msg, vaargs);
    va_end(vaargs);
    printf("\n");
}
#include "Network/Servers/HTTP.hpp"

#include "Container/CTVector.hpp"

using namespace Protocol::HTTP;
using namespace Network::Servers::HTTP;

auto Color = [](Client & client, const auto & headers)
{
    client.reply(Code::Ok, "Bah, it works");
    return true;
};




int main()
{
    constexpr Router<
        Route<Color, MethodsMask{ Method::GET, Method::POST }, "/Color", Headers::ContentLength, Headers::Date, Headers::ContentDisposition>{}
    > router;

    Server<router, 4> server;

    // Simulate client here
    if (Network::Error ret = server.create(8080); ret.isError())
    {
        fprintf(stderr, "Error while creating server: %d:%s\n", (int)ret, Refl::toString((Network::Errors)ret));
        return 1;
    }

    while (true)
    {
        if (Network::Error ret = server.loop(); ret.isError())
        {
            fprintf(stderr, "Error in server loop: %d:%s\n", (int)ret, Refl::toString((Network::Errors)ret));
            return 1;
        }
    }

    printf("OK\n");
    return 0;
}