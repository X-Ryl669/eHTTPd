#ifndef hpp_CPP_HTTPServer_CPP_hpp
#define hpp_CPP_HTTPServer_CPP_hpp


// We need HTTP parsers here
#include "Parser.hpp"
// We need compile time vectors here to cast some magical spells on types
#include "../../Container/CTVector.hpp"
#include "../../Container/RingBuffer.hpp"
#include <type_traits>

#ifndef ClientBufferSize
  #define ClientBufferSize 256
#endif

namespace Network::Servers::HTTP
{

    using namespace Protocol::HTTP;
    struct Socket {}; // TODO reuse class from eMQTT5

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
    };


    /** A client which is linked with a single session.
        There's a fixed possible number of clients while a server is started to avoid dynamic allocation (and memory fragmentation)
        Thus a client is identified by its index in the client array */
    struct Client
    {
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
        uint8       recvBuffer[ClientBufferSize * 3];
        std::size_t recvLength;
        /** The client socket */
        Socket client;
        /** The current request as received and parsed by the server */
        Protocol::HTTP::RequestLine reqLine;

        /** Wait for the request line to come */
        bool waitForRequestLine() const { return true; } // TODO
        /** Check if we already had the request line, or wait for it if not */
        bool hadRequestLine() const { return parsingStatus >= ReqLine || waitForRequestLine(); }


    protected:
        /** Reset this client state and buffer. This is called from the server's accept method before actually using the client */
        void reset() {
#ifdef ParanoidServer
            Zero(recvBuffer);
#endif
            recvLength = 0;
            sessionBuffer.reset();
            reqLine.reset();
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


    /** This is use to share the common code for all template specialization to avoid code bloat */
    struct RouteHelper
    {
        static bool accept(Client & client, uint32 methodsMask, const char * route, const std::size_t routeLength)
        {
            if (((1<<(uint32)client.reqLine.method) & methodsMask)
                && client.reqLine.URI.absolutePath.midString(0, routeLength) == route) // TODO: Use match here instead of plain old string comparison to allow wildcards
                return true;
            return false;
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
        static bool parse(Client & client)
        {
            ExpectedHeaderArray headers;
            // TODO: parsing the headers now we have them (resuming the client receving buffer if required)

            // Then call the route callback method with all the informations
            return CallbackCRTP(client, headers);
        }
    };

    /** Allow to compute the merge of all static routes in a single object */
    template <auto ... Routes>
    struct Router
    {
        static constexpr auto routes = std::make_tuple(Routes...);
        /** Accept a client and call the appropriate route accordingly */
        bool process(Client & client) {
            // TODO: Read some data from the client to fetch, at least, the request line
            do {
                break;
            } while(!client.hadRequestLine());

            // Usual trick to test all routes in a static type list
            bool handled = [&]<std::size_t... Is>(std::index_sequence<Is...>)  {
                return ( (std::get<Is>(routes).accept(client) ? std::get<Is>(routes).parse(client) : false) || ... );
            }(std::make_index_sequence<sizeof...(Routes)>{});

            return handled;
        }
    };
}


#endif
