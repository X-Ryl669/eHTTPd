#include <stdio.h>
#include <type_traits>
#include <stdarg.h>

// Declare the log function the HTTP server is using (here, we simply forward to printf)
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

// High recommented to do so to avoid long names everywhere
using namespace Protocol::HTTP;
using namespace Network::Servers::HTTP;
using namespace CompileTime::Literals; // For ""_hash function

// Basic REST method, supporting both GET and POST. Acting differently on either method
auto Color = [](Client & client, const auto & headers)
{
    // You can do a runtime method testing like this or a compile time testing while registering the route for a post method only
    if (client.reqLine.method == Method::POST) {
        // We don't want to store "some_param" in the binary here, so instead store the hash of this string, much smaller
        // If the parameter is not known at compile time (unlikely), you can use FormPost instead.
        HashFormPost<"some_param"_hash, "value"_hash> form;

        if (!client.fetchContent(headers, form))
        {
            client.closeWithError(Code::BadRequest);
            return true;
        }

        printf("Found param some_param = %.*s, value = %.*s\n",
                form.getValue<"some_param"_hash>().getLength(), form.getValue<"some_param"_hash>().getData(),
                form.getValue<"value"_hash>().getLength(), form.getValue<"value"_hash>().getData() );

        // We can use the found parameter here (even if it's in the same buffer as the reply, it'll be copied in the vault only if required)
        client.reply(Code::Ok, form.getValue<"some_param"_hash>());
        return true;
    } else
    {
        // reply method is a shortcut for SimpleAnswer<MIMEType::text_plain>
        client.reply(Code::Ok, "GET Color");
        return true;
    }
};

// This example use a callback to get chunks of answer (simulate an answer that you need to gather from different, async sources) and will hence use a chunked transfer method
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
        [&]() { return longText.splitFrom(" ", true); }  // Give a word by word answer, this will be called as many times as there are words in the answer, sending a chunk each time
    };
    // Another possibility to set the header
    // answer.template setHeader<Headers::ContentType>(MIMEType::text_plain);
    return client.sendAnswer(answer);
};

// Example to receive a posted file from the client in a POST form
auto PostFile = [](Client & client, const auto & headers)
{
    Streams::FileOutput formFile("upload.out");
    if (!client.fetchContent(headers, formFile))
    {
        client.closeWithError(Code::BadRequest);
        return true;
    }
    client.reply(Code::Ok, "Done");
    return true;
};

// Example to show how you could write your file serving route (catch all)
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

    // Ok, make an FileAnswer that'll copy the file content to the send buffer repeatedly until done.
    // You might prefer to use memory mapping here, which is also possible, with a Stream::MemoryView instead
    // This server doesn't use linux's sendfile syscall, it's too specific to Linux.
    FileAnswer<Streams::FileInput> answer(buffer);
//    FileAnswer<Streams::MemoryView, Headers::ContentEncoding> answer((const char*)buffer, ROString("This is it"));
//    answer.setHeader<Headers::ContentEncoding>(Encoding::identity);
    return client.sendAnswer(answer);
};


#define UseMultiRoute

int main()
{
    // Declare your router with all the static routes
    constexpr Router<
#ifndef UseMultiRoute
        Route<Color, MethodsMask{ Method::GET, Method::POST }, "/Color", Headers::ContentLength, Headers::Date, Headers::ContentDisposition>{},
        Route<LongAnswer, Method::GET, "/long", Headers::Date, Headers::AcceptLanguage >{},
        Route<PostFile, Method::POST, "/postFile", Headers::ContentType >{},
#else
        SimilarRoutes<MethodsMask{ Method::GET, Method::POST },
                      MultiRoute<SubRoute<Color, "/Color"_hash>{},
                                 SubRoute<LongAnswer, "/long"_hash>{},
                                 SubRoute<PostFile, "/postFile"_hash>{}>{},
                      Headers::ContentType, Headers::Date, Headers::AcceptLanguage>{},
#endif
        DefaultRoute<CatchAll, Method::GET, Headers::Date >{}
    > router;

    // Create the server with how many client to expect simultaneously. Each client will use a TranscientBuffer of the size you've configured
    Server<router, 4> server;

    // Start the server on the given port
    if (Network::Error ret = server.create(8080); ret.isError())
    {
        fprintf(stderr, "Error while creating server: %d:%s\n", (int)ret, Refl::toString((Network::Errors)ret));
        return 1;
    }

    // The main server loop
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