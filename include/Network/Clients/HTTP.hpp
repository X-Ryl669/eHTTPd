#ifndef hpp_Client_HTTP_hpp
#define hpp_Client_HTTP_hpp


// We need headers array too
#include "Network/Common/HeadersArray.hpp"
#include "Network/Common/HTTPMessage.hpp"
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


#include <type_traits>

#if BuildClient == 1


#ifndef ClientBufferSize
  #define ClientBufferSize 1024
#endif

namespace Network::Clients::HTTP
{
    using namespace Protocol::HTTP;
    using namespace Network::Common::HTTP;

    template <Headers ... expectedHeaders>
    struct ExpectedAnswer : public CommonHeader<ExpectedAnswer<expectedHeaders...>, BaseSocket, expectedHeaders...>
    {

        ExpectedAnswer() {}
    };

    struct Request
    {
        Method method;
        ROString url;
        ROString additionalHeaders;

        Request(Method method, ROString url, ROString additionalHeaders = "") : method(method), url(url), additionalHeaders(additionalHeaders) {}
    };

    template <typename Stream>
    struct RequestWithTypedStream : public Request
    {
        auto getInputStream(BaseSocket & socket) { return stream; }
        const char* getStreamType(BaseSocket &)  { return Refl::toString(mime); }

        Stream stream;
        MIMEType mime;

        template<typename StreamArg, typename ... Args>
        RequestWithTypedStream(StreamArg arg, MIMEType mime, Args && ... args) : Request(std::forward<Args>(args)...), stream(arg), mime(mime) {}
    };

    template <typename Stream>
    struct RequestWithStream : public Request
    {
        auto getInputStream(BaseSocket & socket) { return stream; }

        Stream stream;

        template<typename StreamArg, typename ... Args>
        RequestWithStream(StreamArg arg, Args && ... args) : Request(std::forward<Args>(args)...), stream(arg) {}
    };

    template <typename Base>
    struct RequestWithExpectedServerCert : public Base
    {
        ROString cert;
        template <typename ... Args>
        RequestWithExpectedServerCert(ROString cert, Args && ... args) : Base(std::forward<Args>(args)...), cert(cert) {}
    };

    // Compile-time proxy wrapper used to enhance logging while capture communication
    template <int Level> struct SocketDumper
    {
        BaseSocket & socket;
        SocketDumper(BaseSocket & socket) : socket(socket) {}

        template <typename ... Args> inline auto connect(Args && ... args) { return socket.connect(std::forward<Args>(args)...); }
        template <typename ... Args> inline auto send(Args && ... args)    { return socket.send(std::forward<Args>(args)...); }
        template <typename ... Args> inline auto recv(Args && ... args)    { return socket.recv(std::forward<Args>(args)...); }
    };

    template <> struct SocketDumper<1>
    {
        BaseSocket & socket;
        SocketDumper(BaseSocket & socket) : socket(socket) {}

        template <typename ... Args> inline auto connect(Args && ... args) { auto r = socket.connect(std::forward<Args>(args)...); SLog(Level::Info, "Connect returned: %d", (int)r); return r; }
        inline auto send(const char * b, const uint32 l)                   { auto r = socket.send(b, l); SLog(Level::Info, "Send returned: %d/%u", (int)r, l); return r; }
        inline auto recv(char * b, const uint32 l, const uint32 m = 0)     { auto r = socket.recv(b, l, m); SLog(Level::Info, "Recv returned: %d/%u", (int)r, l); return r; }
    };

    template <> struct SocketDumper<2>
    {
        BaseSocket & socket;
        SocketDumper(BaseSocket & socket) : socket(socket) {}

        template <typename ... Args> auto connect(const char * h, uint16 p, Args && ... args) { auto r = socket.connect(h, p, std::forward<Args>(args)...); SLog(Level::Info, "Connect to %s:%hu returned: %d", h, p, (int)r); return r; }
        auto send(const char * b, const uint32 l)                                             { auto r = socket.send(b, l); SLog(Level::Info, "Send [%.*s] returned: %d/%u", l, b, (int)r, l); return r; }
        auto recv(char * b, const uint32 l, const uint32 m = 0)                               { auto r = socket.recv(b, l, m); SLog(Level::Info, "Recv returned: %d/%u [%.*s]", (int)r, l, (int)r > 0 ? (int)r : 0, b); return r; }
    };

    /** A native and simple HTTP client library reusing the code of the HTTP server to avoid duplicate in your binary.
     */

    struct Client
    {
        /** The current parsing status for this client.
            The state machine is like this:
            @code
            [ Invalid ] ==> Request line incomplete => [ ReqLine ]
            [ ReqLine ] ==> Request line complete => [ RecvHeaders ]
            [ RecvHeader ] ==> \r\n\r\n found ? => [ HeadersDone ] (else [NeedRefillHeaders], currently not implemented)
            [ HeadersDone ] ==> Content received? => [ ReqDone ]
            @endcode */
        enum ParsingStatus
        {
            Invalid = 0,
            ReqLine,
            RecvHeaders,
            NeedRefillHeaders, // Currently not implemented, used to trigger route's processing for emptying the recv buffer in case the request doesn't fill the available buffer
            HeadersDone,
            ReqDone,

        };

        template <int verbosity, typename Request>
        Code sendRequest(Request & request)
        {
            // Parse the given URL to check for supported features
            ROString url = request.url;
            ROString scheme = url.splitFrom("://");
            if (scheme != "http" && scheme != "https") return Code::ClientRequestError;
#if UseTLSClient == 0
            if (scheme == "https") return Code::ClientRequestError;
#endif
            ROString credentials = url.splitFrom("@");
            ROString authority = url.splitUpTo("/");
            ROString qdn = authority.upToLast(":");
            uint16 port = scheme == "http" ? 80 : 443;
            if (qdn.getLength() != authority.getLength())
                port = (uint32)(int)authority.fromLast(":");

            if (credentials)
                // Not supported yet
                return Code::ClientRequestError;

            // Parse the request URI scheme to know what kind of socket to build
            BaseSocket * _socket =
#if UseTLSClient != 0
                scheme == "https" ? new MBTLSSocket :
#endif
                new BaseSocket;

            SocketDumper<verbosity> socket(*_socket);

            // Prepare the request line now
            ROString uri = request.url.midString(request.url.getLength() - url.getLength() - (url[0] == '/'), url.getLength() + (url[0] == '/'));
            RWString reqLine = RWString(Refl::toString(request.method)) + " " + (uri ? uri : "/") + " HTTP/1.1\r\n";
            // Prepare the host line too
            RWString hostHeader = RWString("Host:") + qdn + "\r\n";

            // Connect to the server
            char * host = (char*)alloca(qdn.getLength() + 1);
            memcpy(host, qdn.getData(), qdn.getLength());
            host[qdn.getLength()] = 0;
            Error err = Success;
            if constexpr (requires{request.cert;}) {
                err = socket.connect(host, port, 5000, &request.cert);
            } else {
                err = socket.connect(host, port, 5000);
            }
            if (err != Success) {
                SLog(Level::Error, "Connect error: %d", (int)err);
                return Code::ClientRequestError;
            }

            // Send the request now
            if (socket.send(reqLine.getData(), reqLine.getLength()) != reqLine.getLength())
                return Code::Unavailable;
            if (socket.send(hostHeader.getData(), hostHeader.getLength()) != hostHeader.getLength())
                return Code::Unavailable;
            // Send the headers
            if (socket.send(request.additionalHeaders.getData(), request.additionalHeaders.getLength()) != request.additionalHeaders.getLength())
                return Code::Unavailable;

            // Check if we have some content to send
            if constexpr (requires{request.getInputStream(*socket);}) {
                auto && stream = request.getInputStream(*socket);
                // If we know the type of file to send, tell the server about it
                if constexpr (requires{request.getStreamType(*socket);}) {
                    hostHeader = "Content-Type:" + request.getStreamType(*socket) + "\r\n";
                    if (socket.send(hostHeader.getData(), hostHeader.getLength()) != hostHeader.getLength())
                        return Code::Unavailable;
                }
                // Send the content length header
                std::size_t contentLength = stream.getSize();
                if (contentLength)
                {
                    if (!sendSize(*socket, contentLength)) return Code::ClientRequestError;

#if MinimizeStackSize == 1
                    uint8 buffer[1024];
#endif
                    // Send the content now
                    while (true)
                    {
                        std::size_t p = stream.read(buffer, ArrSz(buffer));
                        if (!p) break;

                        socket.send((const char*)buffer, p);
                    }
                }
                else // TODO: Support chunked encoding for the request
                    return Code::ClientRequestError;
            } else {
                // Send the end of request and start receiving the answer
                if (socket.send(EOM, 2) != 2) return Code::Unavailable;
            }

            // Receive HTTP server answer now
            // Use the same logic as for the HTTP server here, that is, receive data from the server in a (ring buffer) and parse it
            // We are only interested in few headers:
            ExpectedAnswer<Headers::Location, Headers::ContentType, Headers::ContentLength, Headers::TransferEncoding, Headers::ContentEncoding, Headers::WWWAuthenticate> answer;
            Container::TranscientVault<ClientBufferSize> recvBuffer;
            ParsingStatus status = ReqLine;
            Code serverAnswer;
            bool endOfHeaders = false;

            // Main loop to receive data
            while(true)
            {
                Error err = socket.recv((const char*)recvBuffer.getTail(), recvBuffer.freeSize());
                if (err.isError()) return Code::InternalServerError;
                recvBuffer.stored(err.getCount());

                ROString buffer = recvBuffer.getView<ROString>();
                if (status < HeadersDone)
                {   // Need to make sure we've received a complete line to be able to make progress here
                    if (buffer.Find("\r\n") == buffer.getLength())
                    {
                        if (recvBuffer.freeSize()) continue; // Not enough data for progressing processing, let's try again
                        return Code::ClientRequestError; // Buffer is too small to store the given request
                    }
                }
                switch(status)
                {
                case ReqLine:
                {
                    // Check if we have enough
                    ROString protocol = buffer.splitFrom(" ");
                    if (protocol != "HTTP/1.1" && protocol != "HTTP/1.0") return Code::UnsupportedHTTPVersion;
                    ROString _code = buffer.splitFrom(" ");
                    int code = _code;
                    // Save server code now
                    if (code < 100 || code > 599) return Code::UnsupportedHTTPVersion;
                    serverAnswer = (Code)code;
                    buffer.splitFrom("\r\n");
                    status = RecvHeaders;
                }
                [[fallthrough]];
                case RecvHeaders:
                {
                    ROString header = buffer.splitFrom("\r\n");
                    if (!header) {
                        // Not enough data to parse a single header, let's refill
                        recvBuffer.drop(buffer.getData());
                        if (buffer.midString(0, 2) == "\r\n") {
                            status = HeadersDone;
                            recvBuffer.drop(2);
                        }
                        continue;
                    }
                    // Parse the header now

                }
                case NeedRefillHeaders:
                case HeadersDone:
                case ReqDone:
                default: return Code::ClientRequestError;
                }
            }


            return Code::Ok;
        }
    };
}

#endif


#endif
