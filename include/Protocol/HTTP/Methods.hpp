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
            OPTIONS = 3,
            POST    = 4,
            PUT     = 5,
        };


        /** The important HTTP headers used in this library. All other headers are accessible via a callback but those are converted to an enum value since it's required for any request */
        enum Headers
        {
            InvalidHeader = -1,
            Accept = 0,
            AcceptCharset,
            IF(MaxSupport, AcceptDatetime, )
            AcceptEncoding,
            AcceptLanguage,
            IF(MaxSupport, AcceptPatch, )
            AcceptRanges,
            IF(MaxSupport, AccessControlAllowCredentials, )
            IF(MaxSupport, AccessControlAllowHeaders, )
            IF(MaxSupport, AccessControlAllowMethods, )
            AccessControlAllowOrigin = 0,
            IF(MaxSupport, AccessControlExposeHeaders, )
            IF(MaxSupport, AccessControlMaxAge, )
            IF(MaxSupport, AccessControlRequestMethod, )
            IF(MaxSupport, Allow, )
            Authorization,
            CacheControl,
            Connection,
            ContentDisposition,
            ContentEncoding,
            ContentLanguage,
            ContentLength,
            IF(MaxSupport, ContentLocation, )
            ContentRange,
            ContentType,
            Cookie,
            Date,
            IF(MaxSupport, ETag, )
            IF(MaxSupport, Expect, )
            Expires,
            IF(MaxSupport, Forwarded, )
            IF(MaxSupport, From, )
            Host,
            IF(MaxSupport, IfMatch, )
            IF(MaxSupport, IfModifiedSince, )
            IF(MaxSupport, IfNoneMatch, )
            IF(MaxSupport, IfRange, )
            IF(MaxSupport, IfUnmodifiedSince, )
            LastModified,
            IF(MaxSupport, Link, )
            Location,
            IF(MaxSupport, MaxForwards, )
            Origin,
            Pragma,
            IF(MaxSupport, Prefer, )
            IF(MaxSupport, ProxyAuthorization, )
            Range,
            Referer,
            Server,
            SetCookie,
            IF(MaxSupport, StrictTransportSecurity, )
            TE,
            IF(MaxSupport, Trailer, )
            TransferEncoding,
            Upgrade,
            UserAgent,
            IF(MaxSupport, Via, )
            WWWAuthenticate,
            IF(MaxSupport, XForwardedFor, )
        };

        /** The important HTTP headers used in this library. All other headers are accessible via a callback but those are converted to an enum value since it's required for any request */
        enum class RequestHeaders
        {
            Accept = Headers::Accept,
            AcceptCharset = Headers::AcceptCharset,
            IF(MaxSupport, AcceptDatetime = Headers::AcceptDatetime, )
            AcceptEncoding = Headers::AcceptEncoding,
            AcceptLanguage = Headers::AcceptLanguage,
            IF(MaxSupport, AccessControlRequestMethod = Headers::AccessControlRequestMethod, )
            Authorization = Headers::Authorization,
            CacheControl = Headers::CacheControl,
            Connection = Headers::Connection,
            ContentEncoding = Headers::ContentEncoding,
            ContentType = Headers::ContentType,
            ContentLength = Headers::ContentLength,
            Cookie = Headers::Cookie,
            Date = Headers::Date,
            IF(MaxSupport, Expect = Headers::Expect, )
            IF(MaxSupport, Forwarded = Headers::Forwarded, )
            IF(MaxSupport, From = Headers::From, )
            Host = Headers::Host,
            IF(MaxSupport, IfMatch = Headers::IfMatch, )
            IF(MaxSupport, IfModifiedSince = Headers::IfModifiedSince, )
            IF(MaxSupport, IfNoneMatch = Headers::IfNoneMatch, )
            IF(MaxSupport, IfRange = Headers::IfRange, )
            IF(MaxSupport, IfUnmodifiedSince = Headers::IfUnmodifiedSince, )
            IF(MaxSupport, MaxForwards = Headers::MaxForwards, )
            Origin = Headers::Origin,
            IF(MaxSupport, Prefer = Headers::Prefer, )
            IF(MaxSupport, ProxyAuthorization = Headers::ProxyAuthorization, )
            Range = Headers::Range,
            Referer = Headers::Referer,
            TE = Headers::TE,
            IF(MaxSupport, Trailer = Headers::Trailer, )
            TransferEncoding = Headers::TransferEncoding,
            UserAgent = Headers::UserAgent,
            Upgrade = Headers::Upgrade,
            IF(MaxSupport, Via = Headers::Via, )
            IF(MaxSupport, XForwardedFor = Headers::XForwardedFor, )
        };

        /** The usual response headers in HTTP */
        enum class ResponseHeaders
        {
            AccessControlAllowOrigin = Headers::AccessControlAllowOrigin,
            IF(MaxSupport, AccessControlAllowCredentials = Headers::AccessControlAllowCredentials, )
            IF(MaxSupport, AccessControlExposeHeaders = Headers::AccessControlExposeHeaders, )
            IF(MaxSupport, AccessControlMaxAge = Headers::AccessControlMaxAge, )
            IF(MaxSupport, AccessControlAllowMethods = Headers::AccessControlAllowMethods, )
            IF(MaxSupport, AccessControlAllowHeaders = Headers::AccessControlAllowHeaders, )
            IF(MaxSupport, AcceptPatch = Headers::AcceptPatch, )
            AcceptRanges = Headers::AcceptRanges,
            IF(MaxSupport, Allow = Headers::Allow, )
            CacheControl = Headers::CacheControl,
            Connection = Headers::Connection,
            ContentDisposition = Headers::ContentDisposition,
            ContentEncoding = Headers::ContentEncoding,
            ContentLanguage = Headers::ContentLanguage,
            ContentLength = Headers::ContentLength,
            IF(MaxSupport, ContentLocation = Headers::ContentLocation, )
            ContentRange = Headers::ContentRange,
            ContentType = Headers::ContentType,
            Date = Headers::Date,
            IF(MaxSupport, ETag = Headers::ETag, ) // This one will break the parser
            Expires = Headers::Expires,
            LastModified = Headers::LastModified,
            IF(MaxSupport, Link = Headers::Link, )
            Location = Headers::Location,
            Pragma = Headers::Pragma,
            Server = Headers::Server,
            SetCookie = Headers::SetCookie,
            IF(MaxSupport, StrictTransportSecurity = Headers::StrictTransportSecurity, )
            IF(MaxSupport, Trailer = Headers::Trailer, )
            TransferEncoding = Headers::TransferEncoding,
            Upgrade = Headers::Upgrade,
            WWWAuthenticate = Headers::WWWAuthenticate,
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








    }
}

namespace Refl
{
    template <> constexpr bool isCaseSensitive<Protocol::HTTP::Method> = false;
    template <> constexpr bool isCaseSensitive<Protocol::HTTP::Headers> = false;

    template <>
    constexpr inline auto & _supports<Protocol::HTTP::Headers>()
    {
        constexpr auto maxV = find_max_value<Protocol::HTTP::Headers, 0>();
        return *[]<std::size_t ... i>(std::index_sequence<i ...>) constexpr { static std::array<const char*, maxV+1> values = { CompileTime::str_ref<Protocol::HTTP::makeHTTPHeader<enum_raw_name_only_str<Protocol::HTTP::Headers, (int)i>()>()>{}.data...}; return &values; }(std::make_index_sequence<maxV+1>{});
    }

}

#endif
