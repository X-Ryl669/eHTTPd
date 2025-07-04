#ifndef hpp_Route_hpp
#define hpp_Route_hpp

// We need Client declaration
#include "HTTP.hpp"


namespace Network::Servers::HTTP
{
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
            ROString input = client.recvBuffer.getView<ROString>(), header;

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

        /** A generic header parser that's using the given lambda function for the specialized stuff (this limits the binary size) */
        template <typename Func>
        static ClientState parsePersist(Client & client, Func && f)
        {
            // Parse the headers as much as we can
            ROString input = client.recvBuffer.getView<ROString>(), header;

            // Here the logic is different, since we don't have a complete headers here, we have to parse
            // line by line and adjust our buffer to persist the string values in the vault

            do
            {
                if (input.Find("\r\n") == input.getLength())
                {
                    // Ok, we're done here, let's save the headers for the next iteration.
                    // Drop anything we've already processed to make space for later buffers
                    client.recvBuffer.drop((std::size_t)((const uint8*)input.getData() - client.recvBuffer.getHead()));
                    return ClientState::NeedRefill;
                }
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

                    // Check if we need to persist the header to the vault here
                    HeaderMap::ValueBase * persist = reqHdr->getValue();
                    if (persist)
                    {
                        MaxPersistStringArray arr;
                        persist->getStringToPersist(arr);
                        if (!Container::persistStrings(arr, client.recvBuffer, (std::size_t)((const uint8*)input.getData() - client.recvBuffer.getHead())))
                        {
                            client.closeWithError(Code::InternalServerError);
                            return ClientState::Error;
                        }
                        input = client.recvBuffer.getView<ROString>();
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

            ClientState state = client.parsingStatus == Client::HeadersDone ? RouteHelper::parse(client, cb) : RouteHelper::parsePersist(client.routeFound(headers), cb);
            if (state == ClientState::NeedRefill)
            {
                return client.saveHeaders(headers);
            }
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

    /** The default route */
    template <RouteCallback auto CallbackCRTP, MethodsMask methods, Headers ... allowedHeaders> using DefaultRoute = Route<CallbackCRTP, methods, "", allowedHeaders...>;

    /** Allow to compute the merge of all static routes in a single object */
    template <auto ... Routes>
    struct Router
    {
        static constexpr auto routes = std::make_tuple(Routes...);
        /** Accept a client and call the appropriate route accordingly */
        static ClientState process(Client & client) {
            // TODO: Read some data from the client to fetch, at least, the request line
            if (client.parsingStatus < Client::NeedRefillHeaders) return ClientState::Error;

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
                    uint32 availableLength = client->recvBuffer.freeSize();
                    if (!availableLength)
                    {
                        closeClient(client, Code::EntityTooLarge);
                        continue;
                    }
                    Error ret = client->socket.recv((char*)client->recvBuffer.getTail(), 0, availableLength);
                    if (ret.isError())
                    {
                        closeClient(client, Code::BadRequest);
                        continue;
                    }
                    client->recvBuffer.stored(ret.getCount());

                    // Then parse the client code here at best as we can
                    if (!client->parse()) closeClient(client);
                    else {
//                        SLog(Level::Debug, "Client %s[%.*s]", client->socket.address, client->reqLine.URI.absolutePath.getLength(), client->reqLine.URI.absolutePath.getData());

                        // Check if we can query the routes now
                        if (client->parsingStatus > Client::RecvHeaders)
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