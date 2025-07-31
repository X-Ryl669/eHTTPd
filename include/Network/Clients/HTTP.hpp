#ifndef hpp_Client_HTTP_hpp
#define hpp_Client_HTTP_hpp


// We need headers array too
#include "HeadersArray.hpp"
// We need the socket code too for clients and server
#include "Network/Socket.hpp"
// We need intToStr
#include "Strings/RWString.hpp"
// We need error code too
#include "Protocol/HTTP/Codes.hpp"
// We need compile time vectors here to cast some magical spells on types
#include "Container/CTVector.hpp"
#include "Container/RingBuffer.hpp"
// We need streams too
#include "Streams/Streams.hpp"
// We need forms too
#include "Forms.hpp"

#include <type_traits>


#ifndef ClientBufferSize
  #define ClientBufferSize 1024
#endif

namespace Network::Client::HTTP
{
    using namespace Protocol::HTTP;
    using namespace Network::Common::HTTP;

    template <Headers ... headersToSend>
    struct Request :  public CommonHeader<Request<headersToSend...>, Socket, headersToSend...>
    {
        Method method;
        ROString url;

        Request(Method method, ROString && url) : method(method), url(std::forward(url)) {}
    };

    /** A native and simple HTTP client library reusing the code of the HTTP server to avoid duplicate in your binary.
     */
    struct Client
    {
        template <typename Request>
        Code sendRequest(Request & request);
    };
}


#endif
