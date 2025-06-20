#ifndef hpp_CPP_HTTPServer_CPP_hpp
#define hpp_CPP_HTTPServer_CPP_hpp


// We need HTTP parsers here
#include "Parser.hpp"
// We need the socket code too for clients and server
#include "../Socket.hpp"
// We need intToStr
#include "../../Strings/RWString.hpp"
// We need error code too
#include "../../Protocol/HTTP/Codes.hpp"
// We need compile time vectors here to cast some magical spells on types
#include "../../Container/CTVector.hpp"
#include "../../Container/RingBuffer.hpp"
#include <type_traits>
// We need offsetof for making the container_of macro
#include <cstddef>

#ifndef ClientBufferSize
  #define ClientBufferSize 256
#endif

#ifdef UseTLSServer
  #define Socket MBTLSSocket
#else
  #define Socket BaseSocket
#endif


#define container_of(pointer, type, member)                                                        \
  (reinterpret_cast<type*>((reinterpret_cast<char*>(pointer) - offsetof(type, member))))


namespace Network::Servers::HTTP
{
    static constexpr const char HTTPAnswer[] = "HTTP/1.1 ";
    static constexpr const char EOM[] = "\r\n\r\n";
    static constexpr const char BadRequestAnswer[] = "HTTP/1.1 400 Bad request\r\n\r\n";
    static constexpr const char EntityTooLargeAnswer[] = "HTTP/1.1 413 Entity too large\r\n\r\n";
    static constexpr const char InternalServerErrorAnswer[] = "HTTP/1.1 500 Internal server error\r\n\r\n";
    static constexpr const char NotFoundAnswer[] = "HTTP/1.1 404 Not found\r\n\r\n";

    using namespace Protocol::HTTP;

    /** The current client parsing state */
    enum class ClientState
    {
        Error       = 0,
        Processing  = 1,
        NeedRefill  = 2,
        Done        = 3,
    };

    /** A client which is linked with a single session.
        There's a fixed possible number of clients while a server is started to avoid dynamic allocation (and memory fragmentation)
        Thus a client is identified by its index in the client array */
    struct Client
    {
        /** The client socket */
        Socket      socket;
        /** The current parsing status for this client.
            The state machine is like this:
            @code
            [ Invalid ] ==> Request line incomplete => [ ReqLine ]
            [ ReqLine ] ==> Request line complete => [ RecvHeaders ]
            [ RecvHeader ] ==> \r\n\r\n found ? => [ HeadersDone ]
            [ HeadersDone ] ==> Content received? => [ ReqDone ]
            @endcode */
        enum ParsingStatus
        {
            Invalid = 0,
            ReqLine,
            RecvHeaders,
            HeadersDone,
            ReqDone,

        } parsingStatus;

        /** The buffer where all the per-request data is saved */
        Container::FixedSize<ClientBufferSize> sessionBuffer;
        /** The current receiving buffer */
        char        recvBuffer[ClientBufferSize * 3];
        std::size_t recvLength;

        /** The content length for the answer */
        std::size_t answerLength;
        Code        replyCode;
        /** The current request as received and parsed by the server */
        Protocol::HTTP::RequestLine reqLine;

        bool sendStatus()
        {
            char buffer[5] = { };
            intToStr((int)replyCode, buffer, 10);
            buffer[3] = ' ';

            socket.send(HTTPAnswer, strlen(HTTPAnswer));
            socket.send(buffer, strlen(buffer));
            socket.send(Refl::toString(replyCode), strlen(Refl::toString(replyCode)));
            socket.send(EOM, 2);
            return true;
        }
        bool sendSize()
        {
            static char hdr[] = { ':' };
            char buffer[sizeof("18446744073709551615")] = { };
            socket.send(Refl::toString(Headers::ContentLength), strlen(Refl::toString(Headers::ContentLength)));
            socket.send(hdr, 1);
            intToStr((int)answerLength, buffer, 10);
            socket.send(buffer, strlen(buffer));
            return true;
        }
        bool reply(Code statusCode, const ROString & msg, bool close = false)
        {
            replyCode = statusCode;
            if (!sendStatus()) return false;
            SLog(Level::Info, "Client %s [%.*s](%u): %d%s", socket.address, reqLine.URI.absolutePath.getLength(), reqLine.URI.absolutePath.getData(), msg.getLength(), (int)replyCode, close ? " closed" : "");

            answerLength = msg.getLength();
            if (!sendSize()) return false;
            socket.send(EOM, strlen(EOM));
            if (answerLength) socket.send(msg.getData(), msg.getLength());
            if (close) reset();
            return true;
        }

        bool closeWithError(Code code) { return reply(code, "", true); }

        bool parse() {
            ROString buffer(recvBuffer, recvLength);
            switch (parsingStatus)
            {
            case Invalid:
            {   // Check if we had a complete request line
                parsingStatus = ReqLine;
            } // Intentionally no break here
            case ReqLine:
            {
                if (buffer.Find("\r\n") != buffer.getLength())
                {
                    // Potential request line found, let's parse it to check if it's full
                    if (ParsingError err = reqLine.parse(buffer); err != MoreData)
                        return closeWithError(Code::BadRequest);
                    // Got the request line, let's move to the headers now
                    // A classical HTTP server will continue parsing headers here
                    // But we aren't a classical HTTP server, there's no point in parsing header if there's no route to match for it
                    // So we'll stop here and the server will match the routes that'll take over the parsing from here.
                    parsingStatus = RecvHeaders;
                    if (!reqLine.URI.normalizePath()) return closeWithError(Code::BadRequest);
                    // Make sure we save the current normalized URI since it's used by the routes
                    if (!reqLine.persist(sessionBuffer)) return closeWithError(Code::InternalServerError);

                    // We don't need the request line anymore, let's drop it from the receive buffer
                    memmove(recvBuffer, buffer.getData(), buffer.getLength());
                    recvLength = buffer.getLength();
                    return true;
                } else {
                    // Check if we can ultimately receive a valid request?
                    return recvLength < ArrSz(recvBuffer) ? true : closeWithError(Code::EntityTooLarge);
                }
            }
            case RecvHeaders:
            case HeadersDone:
            case ReqDone: break;
            }
            return false;
        }
        /** Wait for the request line to come */
        bool waitForRequestLine() const { return true; } // TODO
        /** Check if we already had the request line, or wait for it if not */
        bool hadRequestLine() const { return parsingStatus >= ReqLine || waitForRequestLine(); }
        /** Check if the client is valid */
        bool isValid() const { return socket.isValid(); }


    protected:
        /** Reset this client state and buffer. This is called from the server's accept method before actually using the client */
        void reset() {
#ifdef ParanoidServer
            Zero(recvBuffer);
#endif
            recvLength = 0;
            sessionBuffer.reset();
            reqLine.reset();
            parsingStatus = Invalid;
            socket.reset();
            answerLength = 0;

        }

    };



    namespace Details
    {
        // This works when they are all the same type
        template<typename Result, typename... Ts>
        Result& runtime_get(std::size_t i, std::tuple<Ts...>& t)  {
            using Tuple = std::tuple<Ts...>;

            return [&]<std::size_t... Is>(std::index_sequence<Is...>)  {
                return std::array<std::reference_wrapper<Result>, sizeof...(Ts)>{ (Result&)std::get<Is>(t)... }[i];
            }(std::index_sequence_for<Ts...>());
        }

        // Convert a std::array of headers to a parametric typelist
        template <Headers E> struct MakeRequest { typedef Protocol::HTTP::RequestHeader<E> Type; };
    }


    // Useless vomit of useless garbage to get the inner content of a typelist
    template <auto headerArray, typename U> struct HeadersArray {};
    /** The headers array with a compile time defined heterogenous tuple of headers */
    template <auto headerArray, typename ... Header>
    struct HeadersArray<headerArray, Container::TypeList<Header...>>
    {
        std::tuple<Header...> headers;

        // Runtime version, slower O(N) at runtime, and takes more binary space since it must store an array of values here
        RequestHeaderBase * getHeader(const Headers h)
        {
            // Need to find the position in the tuple first
            std::size_t pos = 0;
            for(; pos < headerArray.size(); ++pos) if (headerArray[pos] == h) break;
            if (pos == headerArray.size()) return 0; // Not found, don't waste time searching for it

            return &Details::runtime_get<RequestHeaderBase>(pos, headers);
        }

        static constexpr std::size_t findHeaderPos(const Headers h) {
            std::size_t pos = 0;
            for(; pos < headerArray.size(); ++pos) if (headerArray[pos] == h) break;
            return pos;
        }
        // Compile time version, faster O(1) at runtime, and smaller, obviously
        template <Headers h>
        RequestHeader<h> & getHeader()
        {
            constexpr std::size_t pos = findHeaderPos(h);
            if constexpr (pos == headerArray.size())
            {   // If the compiler stops here, you're querying a header that doesn't exist...
                throw "Invalid header given for this type, it doesn't contain any";
//                    static RequestHeader<h> invalid;
//                    return invalid;
            }
            return std::get<pos>(headers);
        }

        // Runtime version to test if we are interested in a specific header (speed up O(M*N) search to O(m*N) search instead)
        Headers acceptHeader(const ROString & header)
        {
            // Only search for headers we are interested in
            for(std::size_t pos = 0; pos < headerArray.size(); ++pos)
                if (header == Refl::toString(headerArray[pos])) return headerArray[pos];

            return Headers::Invalid;
        }

        // Runtime version to accept header and parse the value in the expected element
        ParsingError acceptAndParse(const ROString & header, ROString & input)
        {
            ParsingError err = InvalidRequest;
            [&]<std::size_t... Is>(std::index_sequence<Is...>)  {
                return ((header == Refl::toString(headerArray[Is]) ? (err = std::get<Is>(headers).acceptValue(input), true) : false) || ...);
            }(std::make_index_sequence<sizeof...(Header)>{});
            return err;
        }
    };



    /** Convert the list of headers you're expecting to the matching HeadersArray the library is using */
    template <Headers ... allowedHeaders>
    struct ToHeaderArray {
        static constexpr size_t HeaderCount = sizeof...(allowedHeaders);
        static constexpr auto headersArray = Container::getUnique<std::array<Headers, HeaderCount>{allowedHeaders...}, std::array<Headers, 1> {Headers::Authorization}>();
        typedef HeadersArray<headersArray, decltype(Container::makeTypes<Details::MakeRequest, headersArray>())> Type;
    };


    /** The route callback template function expected signature is:
        @code
            bool ARouteCallback(Client &, const HeaderArray<...> & )
        @endcode
        Since the 2nd argument is hard to compute, it's better to either use a template lamda like this:
        @code
            auto ARouteCallback = [](Client & x, const auto& arg) {}
        @endcode
        If it's not possible, you'll have to get the type with ToHeaderArray, but it means you'll have to repeat yourself in the route declaration (like this):
        @code
            bool ARouteCallback(Client & client, const ToHeaderArray<Headers::ContentType, Headers::Date>:Type &)
        @endcode
        */
    template <typename Func>
    concept RouteCallback = requires (Func f, Client c) {
        // Make sure the signature matches
        f(c, HeadersArray<std::array<Headers, 1>{Headers::ContentType}, Container::TypeList<RequestHeader<Headers::ContentType>>>{}); // Who said we can't feed brainfuck to C++ compiler?
    };

    /** The log callback function that expected from the server. The signature is:
        @code
            void ALogFunction(Network::Level, static const char * message with printf like formatting, arguments...)
        @endcode
        By default a null-log function is used */
    template<typename Func>
    concept LogCallback = requires (Func f, Level l, const char * msg) {
        f(l, msg);
        f(l, msg, 1);
    };

#ifndef SLog
    // If no log function defined, let's define a no-op stub here
    template <typename ... Args>
    constexpr void noLog(Network::Level, const char*, Args && ...) {}
    #define SLog noLog
#endif

    /** This is use to share the common code for all template specialization to avoid code bloat */
    struct RouteHelper
    {
        /** Simple accepter that's checking both the method and the given route */
        static bool accept(Client & client, uint32 methodsMask, const char * route, const std::size_t routeLength)
        {
            if (((1<<(uint32)client.reqLine.method) & methodsMask)
                && client.reqLine.URI.absolutePath.midString(0, routeLength) == route) // TODO: Use match here instead of plain old string comparison to allow wildcards
                return true;
            return false;
        }
        /** An wildcard accepter that's only check the method, not the route */
        static bool accept(Client & client, uint32 methodsMask) { return ((1<<(uint32)client.reqLine.method) & methodsMask); }

        /** A generic header parser that's using the given lambda function for the specialized stuff (this limits the binary size) */
        template <typename Func>
        static ClientState parse(Client & client, Func && f)
        {
            // Parse the headers as much as we can
            ROString input(client.recvBuffer, client.recvLength), header;

            if (input.Find(EOM) == input.getLength())
            {
                // No valid header here, we need to refill the client
                // We can either put it back in the pool (and we'll be called back later when data is available)
                // However this means that the whole request must fit in the buffer, which isn't exactly what we want
                // Since it implies reparsing the whole buffer once more when it's ready (we can avoid this to only delay
                // parsing when we've found the end-of-header marker ("\r\n\r\n"))

                // Or we can persist what we've already parsed to the fixed buffer so it makes space for
                // the next batch of headers in the receive buffer. This implies saving the ExpectedHeaderArray
                // while we are returning from this function since it's a local variable
                // This implies adding serializing/deserializing code to the ExpectedHeaderArray instance to the fixed buffer.
                // So a compilation switch since this makes a larger binary

                // For now, let's implement the first option
                return ClientState::NeedRefill;
            }

            do
            {
                if (ParsingError err = GenericHeaderParser::parseHeader(input, header); err != MoreData)
                    break;

                RequestHeaderBase * reqHdr = 0;
                Headers h = f(header, reqHdr);
                if (h == Headers::Invalid)
                {   // We don't care about this header, let's skip it
                    if (GenericHeaderParser::skipValue(input) != MoreData)
                        break;
                }
                else
                {   // Ok, we are intested by this header, let's parse it
                    if (!reqHdr)
                    {
                        client.closeWithError(Code::InternalServerError);
                        return ClientState::Error;
                    }

                    if (ParsingError err = reqHdr->acceptValue(input); err != MoreData && err != EndOfRequest)
                    {   // Parsing error
                        client.closeWithError(Code::NotAcceptable);
                        return ClientState::Error;
                    }
                }
                // Done, parsing? let's call the callback
                if (input == "\r\n") return ClientState::Processing;

            } while(true);

            client.closeWithError(Code::BadRequest);
            return ClientState::Error;
        }
    };

    /** A HTTP route that's accepted by this server. You'll define a list of routes with those in Router object declaration */
    template <RouteCallback auto CallbackCRTP,  MethodsMask methods, CompileTime::str route, Headers ... allowedHeaders>
    struct Route final : public RouteHelper
    {
        typedef ToHeaderArray<allowedHeaders...>::Type ExpectedHeaderArray;
        /** Early and fast check to see if the current request by the client is worth continuing parsing the headers */
        static bool accept(Client & client) { return RouteHelper::accept(client, methods.mask, route.data, route.size); }

        /** Once a route is accepted for a client, let's compute the list of headers and parse them all */
        static ClientState parse(Client & client)
        {
            ExpectedHeaderArray headers;
            auto cb = [&](const ROString & header, RequestHeaderBase *& reqHdr) {
                Headers h = headers.acceptHeader(header);
                if (h != Headers::Invalid) reqHdr = headers.getHeader(h);
                return h;
            };

            ClientState state = RouteHelper::parse(client, cb);
            if (state == ClientState::Processing)
            {   // Ok, the header were accepted, let's start processing this route
                return CallbackCRTP(client, headers) ? ClientState::Done : ClientState::Error;
            }
            return state;
        }
    };

    // Generic catch all route used for file serving typically
    template <RouteCallback auto CallbackCRTP, MethodsMask methods, Headers ... allowedHeaders>
    struct Route<CallbackCRTP, methods, "", allowedHeaders...> final : public RouteHelper
    {
        typedef ToHeaderArray<allowedHeaders...>::Type ExpectedHeaderArray;
        /** Early and fast check to see if the current request by the client is worth continuing parsing the headers */
        static bool accept(Client & client) { return RouteHelper::accept(client, methods.mask); }

        /** Once a route is accepted for a client, let's compute the list of headers and parse them all */
        static ClientState parse(Client & client)
        {
            ExpectedHeaderArray headers;
            auto cb = [&](const ROString & header, RequestHeaderBase *& reqHdr) {
                Headers h = headers.acceptHeader(header);
                if (h != Headers::Invalid) reqHdr = headers.getHeader(h);
                return h;
            };

            ClientState state = RouteHelper::parse(client, cb);
            if (state == ClientState::Processing)
            {   // Ok, the header were accepted, let's start processing this route
                return CallbackCRTP(client, headers) ? ClientState::Done : ClientState::Error;
            }
            return state;
        }
    };

    /** Allow to compute the merge of all static routes in a single object */
    template <auto ... Routes>
    struct Router
    {
        static constexpr auto routes = std::make_tuple(Routes...);
        /** Accept a client and call the appropriate route accordingly */
        static ClientState process(Client & client) {
            // TODO: Read some data from the client to fetch, at least, the request line
            if (client.parsingStatus < Client::RecvHeaders) return ClientState::Error;

            // Usual trick to test all routes in a static type list
            ClientState ret = ClientState::Error;
            bool handled = [&]<std::size_t... Is>(std::index_sequence<Is...>)  {
                return ( (std::get<Is>(routes).accept(client) ? (ret = std::get<Is>(routes).parse(client), true) : false) || ... );
            }(std::make_index_sequence<sizeof...(Routes)>{});

            if (!handled) { client.closeWithError(Code::NotFound); return ClientState::Error; }
            return ret;
        }
    };


        /** The server itself.
        The server roles is to maintain a list of client's resources and perform the network activity work
        That is:
        1. Monitoring for network activity
        2. Fetching data and accepting connections
        3. Sending data back to clients
        4. Managing session/cookies between clients */
    template <auto Router/*, LogCallback auto logCB = noLog*/, std::size_t MaxClientCount = 4>
    struct Server
    {
        /** The main client array that's allocated upon construction and never desallocated */
        Client clientsArray[MaxClientCount] = {};
        /** The server's own socket */
        Socket server;
        /** The socket pool for passively monitoring sockets */
        SocketPool<MaxClientCount + 1> pool;
        /** The cookie jar for each session */
        //TODO

        Error closeClient(Client * client, Code errorCode = Code::Invalid)
        {
            // The client is in error, let's remove it from the pool anyway
            pool.remove(client->socket);
            // Default answer for invalid client
            if (errorCode != Code::Invalid) client->closeWithError(errorCode);
            return Success;
        }

        /** The main server loop */
        Error loop(uint32 timeoutMs = 20)
        {
            if (pool.selectActive(timeoutMs) == Success)
            {   // At least, one socket made progress, so deal with it

                // Deal with client socket first
                Socket * socket;
                while ((socket = pool.getReadableSocket(1))) // Start from 1 since socket 0 if for the server
                {
                    // Got a client for a socket, so need to fill the client buffer and let it progress parsing
                    Client * client = container_of(socket, Client, socket);
                    // Check if we can fill the receive buffer first
                    uint32 availableLength = ArrSz(client->recvBuffer) - client->recvLength;
                    Error ret = client->socket.recv(&client->recvBuffer[client->recvLength], min(availableLength, 16u), availableLength);
                    if (ret.isError())
                    {
                        closeClient(client, Code::BadRequest);
                        continue;
                    }
                    client->recvLength += ret.getCount();

                    // Then parse the client code here at best as we can
                    if (!client->parse()) closeClient(client);
                    else {
//                        SLog(Level::Debug, "Client %s[%.*s]", client->socket.address, client->reqLine.URI.absolutePath.getLength(), client->reqLine.URI.absolutePath.getData());

                        // Check if we can query the routes now
                        if (client->parsingStatus >= Client::RecvHeaders)
                        {   // Yes we can, trigger the router with them
                            switch (Router.process(*client))
                            {
                            case ClientState::Error: closeClient(client); break;
                            case ClientState::Done:  closeClient(client); break;
                            // Don't remove the client from the pool in that case, let's simply continue later on
                            case ClientState::Processing: break;
                            case ClientState::NeedRefill: break;
                            }
                        }
                    }
                }

                if (pool.isReadable(0))
                {   // The server socket is active, let's check if we have any client to process
                    // Find the position for a free client in the array
                    for (auto i = 0; i < ArrSz(clientsArray); i++)
                        if (!clientsArray[i].isValid())
                        {
                            Error ret = server.accept(clientsArray[i].socket, 0);
                            if (ret.isError()) return ret;

                            // Client was received, so let's add this to the loop
                            if (!pool.append(clientsArray[i].socket)) return AllocationFailure;
                            break;
                        }

                    // None found, it'll be processed on the next loop anyway
                    return Success;
                }
            }

            return Success;
        }

        Server() {}

        Error create(uint16 port)
        {
            if (Error ret = server.listen(port, MaxClientCount); ret.isError())
                return ret;

            if (!pool.append(server)) return AllocationFailure;
            SLog(Level::Info, "HTTP server listening on port %u", port);
            return Success;
        }
    };
}


#endif
