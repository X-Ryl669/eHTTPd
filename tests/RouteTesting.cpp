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

auto LongAnswer = [](Client & client, const auto & headers)
{
    ROString longText = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";


    CaptureAnswer answer{
        Code::Ok,
        // Using initializer list here for each given type if any of them requires multiple value, else you can use the value directly
        HeaderSet<Headers::ContentType, Headers::ContentLanguage>{ { MIMEType::text_plain }, { Language::en, Language::fr } },
        [&]() { return longText.splitFrom(" ", true); }  // Give a word by word answer, this will be called as many times as there are words in the answer
    };
    // Another possibility to set the header
//    answer.template setHeader<Headers::ContentType>(Protocol::HTTP::MIMEType::text_plain);
    return client.sendAnswer(answer, true);
};

auto CatchAll = [](Client & client, const auto & headers)
{
    // Simple catch all callback, used for serving files
    // First, check the requested URI
    ROString path = client.getRequestedPath();
    // Need to make an absolute (local) path from this relative path
    // This is for demonstration only, the final path should be sanitized, and this is specific to POSIX platform
    char buffer[256];
    getcwd(buffer, sizeof(buffer));
    size_t cwdLen = strlen(buffer);
    if (cwdLen + path.getLength() + 1 >= sizeof(buffer)) return false;
    memcpy(buffer + cwdLen, path.getData(), path.getLength());
    buffer[cwdLen + path.getLength()] = 0;

    FileAnswer<Streams::FileInput> answer(buffer);
    return client.sendAnswer(answer, true);
};




int main()
{
    constexpr Router<
        Route<Color, MethodsMask{ Method::GET, Method::POST }, "/Color", Headers::ContentLength, Headers::Date, Headers::ContentDisposition>{},
        Route<LongAnswer, Method::GET, "/long", Headers::Date >{},
        DefaultRoute<CatchAll, Method::GET, Headers::Date >{}
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