#include <stdio.h>
#include <type_traits>
#include <stdarg.h>

// Declare the log function the HTTP server is using (here, we simply forward to fprintf)
#define SLog logger
static void logger(auto level, const char * msg, ...)
{
    static char levelStr[] = { 'D', 'I', 'W', 'E' };
    fprintf(stderr, "[%c] ", levelStr[(int)level]);
    va_list vaargs;
    va_start(vaargs, msg);
    vfprintf(stderr, msg, vaargs);
    va_end(vaargs);
    fprintf(stderr, "\n");
}
#include "Network/Clients/HTTP.hpp"
#include "Container/CTVector.hpp"
#include "Strings/RWString.hpp"

// High recommented to do so to avoid long names everywhere
using namespace Protocol::HTTP;
using namespace Network::Clients::HTTP;
using namespace CompileTime::Literals; // For ""_hash function

int help(const char * name)
{
    printf("Usage: %s [url] -m METHOD -h HEADER -f CONTENT_FILE -v VERBOSITY\n", name);
    printf("       URL should be escaped or enclosed in double quotes if it contains shell specific characters\n");
    printf("       METHOD is like GET, POST, PUT, DELETE, OPTIONS\n");
    printf("       HEADER are in the form \"Content-Type: application/json\"\n");
    printf("       CONTENT_FILE is a path to a file to send with the request (only valid for POST or PUT method)\n");
    printf("       VERBOSITY is set the level of verbosity for the operation, from 0 (default) to 3 (most verbose)\n");
    return 0;
}

void join(const std::vector<ROString> & v, const char * c, RWString & s) {
    for (std::vector<ROString>::const_iterator p = v.begin(); p != v.end(); ++p) {
        s += *p;
        if (p != v.end() - 1) s += c;
    }
}

int main(int argc, const char * argv[])
{
    // Parse the given arguments
    ROString              url;
    Method                method = Method::GET;
    std::vector<ROString> headers;
    const char*           filePath = nullptr;
    int                   verbosity = 0;
    if (argc < 2) return help(argv[0]);
    url = argv[1];

    for (int a = 2; a < argc; a += 2)
    {
        const uint32 h = CompileTime::constHash(argv[a]);
        bool failed = a+1 >= argc;
        if (failed) return help(argv[0]);

        switch(h) {
        case "-m"_hash:
            method = Refl::fromString<Method>(argv[a+1]).orElse(Method::Invalid);
            if (method == Method::Invalid) return help(argv[0]);
            break;
        case "-f"_hash: filePath = argv[a+1]; break;
        case "-h"_hash: headers.push_back(argv[a+1]); break;
        case "-v"_hash:
            verbosity = (int)ROString(argv[a+1]);
            if ((unsigned)verbosity > 3) return help(argv[0]);
            break;
        default: return help(argv[0]);
        }
    }


    // Create the client request now
    RWString headersBuffer;
    join(headers, "\r\n", headersBuffer);
    // By default, we return the request on standard output
    Streams::FileOutput out(STDOUT_FILENO);

    auto request = filePath ? RequestWithTypedStream<Streams::FileInput, Streams::FileOutput>(filePath, getMIMEFromExtension(ROString(filePath).fromLast(".")), out, method, url, headersBuffer)
                            : Request(out, method, url, headersBuffer);

    Client client;
    Code code = verbosity == 0 ? client.sendRequest<0>(request) : verbosity == 1 ? client.sendRequest<1>(request) : client.sendRequest<2>(request);
    if (code >= Code::MultipleChoices)
        fprintf(stderr, "Error: %d - %s\n", (int)code, Refl::toString(code));

    return 0;
}