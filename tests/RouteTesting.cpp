#include <stdio.h>
#include <type_traits>
#include <stdarg.h>

// Declare the log function the HTTP server is using
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
#include "Network/Servers/Route.hpp"

#include "Container/CTVector.hpp"

using namespace Protocol::HTTP;
using namespace Network::Servers::HTTP;
using namespace CompileTime::Literals; // For ""_hash function

auto Color = [](Client & client, const auto & headers)
{
    // You can do a runtime method testing like this or a compile time testing while registering the route for a post method only
    if (client.reqLine.method == Method::POST) {
        HashFormPost<"some_param"_hash, "value"_hash> form;

        if (!client.fetchContent(headers, form))
        {
            client.closeWithError(Code::BadRequest);
            return true;
        }

        printf("Found param some_param = %.*s, value = %.*s\n",
                form.getValue<"some_param"_hash>().getLength(), form.getValue<"some_param"_hash>().getData(),
                form.getValue<"value"_hash>().getLength(), form.getValue<"value"_hash>().getData() );
        client.reply(Code::Ok, form.getValue<"some_param"_hash>());
        return true;
    } else
    {
        client.reply(Code::Ok, "GET Color");
        return true;
    }
};

auto LongAnswer = [](Client & client, const auto & headers)
{
    ROString longText = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

    // Example to get an expected header from the client
    auto lang = headers.template getHeader<Headers::AcceptLanguage>();
    Language expected = lang.getValueElement(0);  // Shortcut method. You can use lang.getValueElementsCount to get the number of elements in a list header

    // Or you can use the bare object hierarchy here too. The "parsed" member is always present, the first "value" can either be single or an array. In the latter case, you can access the final objet via "value"
    printf("Value lang: %s\n", Refl::toString(lang.parsed.value[0].value));
    CaptureAnswer answer{
        Code::Ok,
        // Using initializer list here for each given type if any of them requires multiple value, else you can use the value directly
        HeaderSet<Headers::ContentType, Headers::ContentLanguage>{ { MIMEType::text_plain }, { expected, Language::fr } },
        [&]() { return longText.splitFrom(" ", true); }  // Give a word by word answer, this will be called as many times as there are words in the answer
    };
    // Another possibility to set the header
//    answer.template setHeader<Headers::ContentType>(MIMEType::text_plain);
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
        Route<LongAnswer, Method::GET, "/long", Headers::Date, Headers::AcceptLanguage >{},
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