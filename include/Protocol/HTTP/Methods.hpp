#ifndef hpp_HTTP_Methods_hpp
#define hpp_HTTP_Methods_hpp

// We need reflection code for enum to string conversion
#include "../../Reflection/AutoEnum.hpp"
// We need a string-view like class for avoiding useless copy here
#include "../../Strings/ROString.hpp"
// We need compile time string to produce the expected header name
#include "../../Strings/CTString.hpp"

// Simple macro to avoid splattering the code with #if/#endif everywhere
#define CONCAT2(A, B) A##B
#define CONCAT2_DEFERRED(A, B) CONCAT2(A, B)
#define IF_0(true_case, false_case) false_case,
#define IF_1(true_case, false_case) true_case,
#define IF(condition, true_case, false_case) CONCAT2_DEFERRED(IF_, condition)(true_case, false_case)

#define MaxSupport 1

namespace Protocol
{
    namespace HTTP
    {
        /** Inject the reflection tools to convert to/from enumerations */
        using Refl::toString;
        using Refl::fromString;


        /** The supported HTTP methods */
        enum Method
        {
            InvalidMethod = -1,
            DELETE  = 0,
            GET     = 1,
            HEAD    = 2,
            POST    = 3,
            PUT     = 4,
            OPTIONS = 5,
        };



        /** The important HTTP headers used in this library. All other headers are accessible via a callback but those are converted to an enum value since it's required for any request */
        enum RequestHeaders
        {
            InvalidRequest = -1,
            Accept = 0,
            AcceptCharset,
            IF(MaxSupport, AcceptDatetime, )
            AcceptEncoding,
            AcceptLanguage,
            IF(MaxSupport, AccessControlRequestMethod, )
            Authorization,
            CacheControl,
            Connection,
            ContentEncoding,
            ContentType,
            ContentLength,
            Cookie,
            Date,
            IF(MaxSupport, Expect, )
            IF(MaxSupport, Forwarded, )
            IF(MaxSupport, From, )
            Host,
            IF(MaxSupport, IfMatch, )
            IF(MaxSupport, IfModifiedSince, )
            IF(MaxSupport, IfNoneMatch, )
            IF(MaxSupport, IfRange, )
            IF(MaxSupport, IfUnmodifiedSince, )
            IF(MaxSupport, MaxForwards, )
            Origin,
            IF(MaxSupport, Prefer, )
            IF(MaxSupport, ProxyAuthorization, )
            Range,
            Referer,
            TE, // This one will break the parser
            IF(MaxSupport, Trailer, )
            TransferEncoding,
            UserAgent,
            Upgrade,
            IF(MaxSupport, Via, )
            IF(MaxSupport, XForwardedFor, )
        };

        /** The usual response headers in HTTP */
        enum ResponseHeaders
        {
            InvalidResponseHeader = -1,
            AccessControlAllowOrigin = 0,
            IF(MaxSupport, AccessControlAllowCredentials, )
            IF(MaxSupport, AccessControlExposeHeaders, )
            IF(MaxSupport, AccessControlMaxAge, )
            IF(MaxSupport, AccessControlAllowMethods, )
            IF(MaxSupport, AccessControlAllowHeaders, )
            IF(MaxSupport, AcceptPatch, )
            AcceptRanges,
            IF(MaxSupport, Allow, )
            CacheControl,
            Connection,
            ContentDisposition,
            ContentEncoding,
            ContentLanguage,
            ContentLength,
            IF(MaxSupport, ContentLocation, )
            ContentRange,
            ContentType,
            Date,
            IF(MaxSupport, ETag, ) // This one will break the parser
            Expires,
            LastModified,
            IF(MaxSupport, Link, )
            Location,
            Pragma,
            Server,
            SetCookie,
            IF(MaxSupport, StrictTransportSecurity, )
            IF(MaxSupport, Trailer, )
            TransferEncoding,
            Upgrade,
            WWWAuthenticate,
        };

        /** A compile time processing of the enumeration name to make an enumeration string representation that suit HTTP standard of header naming */
        template <auto name>
        constexpr size_t countUppercase() { size_t n = 0; for (size_t i = 0; name[i]; i++) if (name[i] >= 'A' && name[i] <= 'Z') ++n; return n; }

        /** This converts, at compile time, the enum value like "UserAgent" to HTTP expected format for headers: "User-Agent" */
        template <auto name>
        constexpr auto makeHTTPHeader()
        {
            if constexpr (name.size < 5) return name; // Name smaller than 4 char are returned as is
            else
            {
                // Count the number of uppercase letter
                constexpr size_t upCount = countUppercase<name>();
                char output[name.size + upCount - 1] = { name.data[0], 0};
                for (size_t i = 1, j = 1; i < name.size; i++)
                {
                    if (name.data[i] >= 'A' && name.data[i] <= 'Z') output[j++] = '-';
                    output[j++] = name.data[i];
                }
                return CompileTime::str(output);
            }
        }

        constexpr inline auto &_supports<RequestHeaders>()
        {
            constexpr auto maxV = Refl::find_max_value<RequestHeaders, 0>();
            return *[]<std::size_t ... i>(std::index_sequence<i ...>) constexpr { constexpr static std::array<const char*, maxV+1> values = {str_ref<makeHTTPHeader<str(Refl::enum_raw_name_only<RequestHeaders, (int)i>())>()>{}.data...}; return &values; }(std::make_index_sequence<maxV+1>{});
        }







    }
}

#endif
