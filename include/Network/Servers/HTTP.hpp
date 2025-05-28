#ifndef hpp_CPP_HTTPServer_CPP_hpp
#define hpp_CPP_HTTPServer_CPP_hpp


// We need HTTP parsers here
#include "Parser.hpp"

namespace Network::Server::HTTP
{
    using Protocol::HTTP;

    /** A client which is linked with a single session.
        There's a fixed possible number of clients while a server is started to avoid dynamic allocation (and memory fragmentation)
        Thus a client is identified by its index in the client array */
    struct Client
    {

    };

    /** The Route interface all route must implement */
    struct RouteBase
    {
        /** Check if this route accept the given client (the request is inside the client)
            @return true if the route is accepted and no more route is searched, false otherwise */
        virtual bool accept(Client & client) const = 0;

        ~RouteBase()
    };

    /** A HTTP route that's accepted by this server. You'll define a list of routes here */
    template <auto route, size_t N, std::array<Protocol::HTTP::Headers, N> allowedHeaders>
    struct Route final : public RouteBase
    {

    };
}


#endif
