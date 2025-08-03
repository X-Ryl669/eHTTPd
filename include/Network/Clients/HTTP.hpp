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

#if BuildClient == 1


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
        Code sendRequest(Request & request)
        {
            // Parse the given URL to check for supported features
            ROString url = request.url;
            ROString scheme = url.splitFrom("://");
            if (scheme != "http" && scheme != "https") return ClientRequestError;
#if UseTLSClient == 0
            if (scheme == "https") return ClientRequestError;
#endif
            ROString credentials = url.splitFrom("@");
            ROString authority = url.splitFrom("/");
            ROString qdn = authority.upToLast(":");
            uint16 port = scheme == "http" ? 80 : 443;
            if (qdn.getLength() != authority.getLength())
                port = (uint32)authority.fromLast(":");

            if (credentials)
                // Not supported yet
                return ClientRequestError;

            // Parse the request URI scheme to know what kind of socket to build
            BaseSocket * socket =
#if UseTLSClient != 0
                scheme == "https" ? new MBTLSSocket :
#endif
                new BaseSocket;

            // Prepare the request line now
            ROString uri = request.url.midString(request.url.getLength() - url.getLength() - 1, url.getLength() + 1);
            RWString reqLine = RWString(Refl::toString(request.method)) + " " + uri + " HTTP/1.1\r\n";
            // Prepare the host line too
            RWString host = "Host:" + qdn + "\r\n";

            // Connect to the server
            char * host = (char*)alloca(qdn.getLength() + 1);
            memcpy(host, qdn.getData(), qdn.getLength());
            host[qdn.getLength()] = 0;
            Error err = Success;
            if constexpr (requires(request.cert;)) {
                err = socket->connect(host, port, 5000, &request.cert);
            } else {
                err = socket->connect(host, port, 5000);
            }
            if (err != Success) {
                SLog(Error, "Connect error: %d", (int)err);
                return ClientRequestError;
            }

            // Send the request now
            if (socket->send(reqLine.getData(), reqLine.getLength()) != reqLine.getLength())
                return Unavailable;
            if (socket->send(host.getData(), host.getLength()) != host.getLength())
                return Unavailable;
            // Send the headers
            uint8 buffer[1024];
            Container::TrackedBuffer trackedBuffer{buffer, sizeof(buffer)};
            if (!request.sendHeaders(*socket, trackedBuffer))
                return Unavailable;

            // Check if we have some content to send
            auto && stream = clientAnswer.getInputStream(*socket);
            std::size_t contentLength = 0;
            if constexpr (!std::is_same_v<std::decay_t<decltype(stream)>, std::nullptr_t>)
            {
                // Send the content length header
                contentLength = stream.getSize();
                if (contentLength)
                {
                    if (!sendSize(*socket, contentLength)) return ClientRequestError;

                    // Send the content now
                    while (true)
                    {
                        std::size_t p = stream.read(buffer, ArrSz(buffer));
                        if (!p) break;

                        socket.send((const char*)buffer, p);
                    }
                }
            } else {
                // Send the end of request and start receiving the answer
                if (socket->send(EOM, 2) != 2) return Unavailable;
            }

            // Receive HTTP server answer now


        }
    };
}

#endif


#endif
