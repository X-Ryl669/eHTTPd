#ifndef hpp_RequestLine_hpp
#define hpp_RequestLine_hpp

// We need a string-view like class for avoiding useless copy here
#include "../../Strings/ROString.hpp"
// We need methods too
#include "Methods.hpp"

#if defined(MaxSupport)
  // We need path normalization
  #include "../../Path/Normalization.hpp"
#endif

// The REQUEST line is defined in section 5.1 in RFC2616
namespace Protocol::HTTP
{
    enum ParsingError
    {
        InvalidRequest = -1,
        EndOfRequest = 0,
        MoreData = 1,
    };

    /** A typical Query part in the URL is what follow the question mark in this: "?a=b&c[]=3&c[]=4&d"
        This class allows to locate keys and value and returns them one by one.
        The URI isn't URL decoded, here, it can be done in the RequestURI's function too by using the normalize method */
    struct Query
    {
        ROString query;

        // Interface
    public:
        /** Get the value for the given key
            @param key      The key to search for
            @param startPos Optional: If provided, skip the given bytes of the query (used as an optimization step)

            This algorithm is O(N*M), with N the query size in byte and M the key size in byte.
            So even with small N or M try to avoid iterating with this, use the next method instead

            @return The value if any match found or empty string else */
        ROString getValueFor(const ROString & key, const size_t startPos = 0)
        {
            ROString candidate = query.midString(startPos, query.getLength()).fromFirst(key);
            while (candidate)
            {
                if (candidate[0] == '=')
                {
                    candidate.splitAt(1);
                    return candidate.splitUpTo("&");
                }
                candidate = candidate.fromFirst(key);
            }
            return ROString();
        }
        /** Iterate the keys in the query
            @param iter  Opaque value used to speed value extraction. Start with 0 here. You can pass iter to getValueFor for faster value extraction.
            @param key   If any key found, this will be stored here
            @param value If any key found, this will store the associated value or empty string for non valued keys
            @return true if iteration was successful, false when ending */
        bool iterateKeys(size_t & iter, ROString & key, ROString & value)
        {
            if (iter >= query.getLength()) return false;
            ROString q = query.midString(iter, query.getLength());
            key = q.splitUpTo("=");
            if (key) {
                iter += key.getLength() + 1;
                value = q.splitUpTo("&");
                iter += value.getLength() + 1;
                return true;
            }
            if (q)
            {
                key = q; value = ROString();
                iter += q.getLength();
                return true;
            }
            return false;
        }
    };

    /** For the sake of compactness, this isn't a full URI parsing scheme. The request URI for a server that's not a proxy is
        always: (section 5.2.1)
            '*'  (typically for OPTIONS)
         or absolute path (including query parameters if any)
    */
    struct RequestURI
    {
        // Members
    public:
        /** The absolute path given to the URI */
        ROString absolutePath;

        // Interface
    public:
        /** Check if the given request applies to all ressources */
        bool appliesToAllRessources() const { return absolutePath == "*"; }
        /** Check if the given URI is not empty */
        explicit operator bool() const { return absolutePath; }
        /** Get the first query part in the request URI (if any) */
        Query getQueryPart() const { return Query{absolutePath.fromFirst("?")}; }

        RequestURI & operator = (const ROString & path) { absolutePath = path; return *this; }
#if defined(MaxSupport)
        bool normalizePath() { return Path::normalize(absolutePath, true); }
#endif
    };
    /** Request line is defined as METHOD SP Request-URI SP HTTP-Version CRLF */
    struct RequestLine
    {
        /** The requested method */
        Method      method = Method::Invalid;
        /** The requested URI */
        RequestURI  URI;
        /** The protocol version */
        Version     version = Version::Invalid;

        // Interface
    public:
        /** Parse the given data stream */
        ParsingError parse(ROString & input)
        {
            ROString m = input.splitUpTo(" ");
            method = Refl::fromString<Method>(m).orElse(Protocol::HTTP::Method::Invalid);
            if (method == Method::Invalid) return InvalidRequest;

            input = input.trimLeft(' ');
            URI = input.splitUpTo(" ");
            if (!URI || !input) return InvalidRequest;

            input = input.trimLeft(' ');
            if (input.splitUpTo("/1.") != "HTTP") return InvalidRequest;
            if (input[0] > '1' || input[0] < '0') return InvalidRequest;
            version = input[0] == '1' ? Version::HTTP1_1 : Version::HTTP1_0;

            if (input[1] != '\r' || input[2] != '\n') return InvalidRequest;
            input = input.splitAt(3);
            return MoreData;
        }
    };

    namespace RequestType
    {
        template <Headers> struct ValueMap;

        /** Link a header with its type (with serializing function for both type) */
        struct ValueBase
        {
            virtual ParsingError parseFrom(ROString & value) = 0;
            template <Headers h, typename T> T * as() { if constexpr(std::is_same_v<typename ValueMap<h>::ExpectedType, T>) { return static_cast<typename ValueMap<h>::ExpectedType*>(this); } else return (void*)0; }
        };

        /** String value (opaque) */
        struct StringValue : public ValueBase
        {
            ROString value;
            virtual ParsingError parseFrom(ROString & val) {
                value = val.Trim(' ');
                return EndOfRequest;
            }
        };
        template <Headers> struct ValueMap { typedef StringValue ExpectedType; };

        /** Key value in the form "name=value" */
        struct KeyValue : public StringValue
        {
            ROString findValueFor(const ROString & key)
            {
                ROString v = value.fromFirst(key).trimLeft(' ');
                if (v[0] != '=') return ROString();
                return v.trimmedLeft("= ").upToFirst(";").trimRight(' ');
            }
        };

        /** Unsigned integer value (opaque) */
        struct UnsignedValue : public ValueBase
        {
            size_t value;
            virtual ParsingError parseFrom(ROString & val) {
                value = (size_t)val.Trim(' ');
                return EndOfRequest;
            }
        };

        /** Simple enum value for the given type */
        template <typename Enum, bool strict = false> struct EnumValue : public ValueBase
        {
            Enum value;
            virtual ParsingError parseFrom(ROString & val) {
                value = Refl::fromString<Enum>(val.Trim(' ')).orElse(static_cast<Enum>(-1));
                // If we find some unknown value, we don't return an error here, simply continue parsing
                return value != (static_cast<Enum>(-1)) ? EndOfRequest : (strict ? InvalidRequest : EndOfRequest);
            }
        };
        /** Simple enum value for the given type */
        template <typename Enum> struct StrictEnumValue : public EnumValue<Enum, true> {};

        struct EnumValueWithToken
        {
            /** Extract the enum and token value from the given input value */
            static ParsingError parseFrom(ROString & val, ROString & e, ROString & token)
            {
                size_t p = val.findAnyChar(";,", 0, 2);
                if (p != val.getLength() && val[p] == ';') {
                    e = val.splitAt(p).Trim(' ');
                    p = val.findAnyChar(",", 0, 1);
                    token = val.splitAt(p).Trim(' ');
                    val = val.trimLeft(',');
                    return val ? MoreData : EndOfRequest;
                }
                e = val.splitAt(p).Trim(' ');
                val = val.trimLeft(',');
                token = ROString();
                return val ? MoreData : EndOfRequest;
            }
        };
        /** Enum value with quality factor ";q=[.0-9]+,token="
            The quality factor is ignored and so is any token */
        template <typename Enum> struct EnumValueToken : public ValueBase
        {
            Enum value;
            virtual ParsingError parseFrom(ROString & val)
            {
                ROString v, t;
                ParsingError err = EnumValueWithToken::parseFrom(val, v, t);
                if (err == InvalidRequest) return err;
                value = Refl::fromString<Enum>(v).orElse(static_cast<Enum>(-1));
                return err;
            }
        };
        /** Enum value that stores the key and value after ';' and before '=' */
        template <typename Enum> struct EnumKeyValue : public ValueBase
        {
            Enum value;
            ROString attributes;
            virtual ParsingError parseFrom(ROString & val)
            {
                ROString v;
                ParsingError err = EnumValueWithToken::parseFrom(val, v, attributes);
                if (err == InvalidRequest) return err;
                // Fix case with value containing the attribute itself
                if (!attributes) { attributes = v; v = attributes.splitUpTo("="); }
                value = Refl::fromString<Enum>(v).orElse(static_cast<Enum>(-1));
                return err;
            }

            ROString findAttributeValueFor(const ROString & key)
            {
                ROString v = attributes.fromFirst(key).trimLeft(' ');
                if (v[0] != '=') return ROString();
                return v.trimmedLeft("= ").upToFirst(";").trimRight(' ');
            }
        };

        template <typename E, size_t N, bool strict = false>
        struct ValueList : public ValueBase
        {
            E    value[N];
            size_t count = 0;
            virtual ParsingError parseFrom(ROString & val) {
                ParsingError err;
                for(count = 0; count < N;) {
                    err = value[count++].parseFrom(val);
                    if (err == InvalidRequest) { count--; return err; }
                    if (err == EndOfRequest) return EndOfRequest;
                }
                return count < N ? MoreData : (strict ? InvalidRequest : MoreData); // Is there too much allowed element and we don't support them?
            }
        };


        template <> struct ValueMap<Headers::Accept>            { typedef ValueList<EnumValueToken<MIMEType>, 16, true> ExpectedType; };
        template <> struct ValueMap<Headers::AcceptCharset>     { typedef ValueList<EnumValueToken<Charset>, 4> ExpectedType; };
        template <> struct ValueMap<Headers::AcceptEncoding>    { typedef ValueList<EnumValueToken<Encoding>, 4> ExpectedType; };
        template <> struct ValueMap<Headers::AcceptLanguage>    { typedef ValueList<EnumKeyValue<Language>, 8> ExpectedType; };
        template <> struct ValueMap<Headers::Authorization>     { typedef StringValue ExpectedType; };
        template <> struct ValueMap<Headers::CacheControl>      { typedef ValueList<EnumKeyValue<CacheControl>, 4> ExpectedType; };
        template <> struct ValueMap<Headers::Connection>        { typedef StrictEnumValue<Connection> ExpectedType; };
        template <> struct ValueMap<Headers::ContentEncoding>   { typedef ValueList<EnumValueToken<Encoding>, 2> ExpectedType; };
        template <> struct ValueMap<Headers::ContentType>       { typedef EnumKeyValue<MIMEType> ExpectedType; };
        template <> struct ValueMap<Headers::ContentLength>     { typedef UnsignedValue ExpectedType; };
        template <> struct ValueMap<Headers::Cookie>            { typedef KeyValue ExpectedType; };
        template <> struct ValueMap<Headers::Date>              { typedef StringValue ExpectedType; };
        template <> struct ValueMap<Headers::Host>              { typedef StringValue ExpectedType; };
        template <> struct ValueMap<Headers::Origin>            { typedef StringValue ExpectedType; };
        template <> struct ValueMap<Headers::Range>             { typedef KeyValue ExpectedType; };
        template <> struct ValueMap<Headers::Referer>           { typedef StringValue ExpectedType; };
        template <> struct ValueMap<Headers::TE>                { typedef ValueList<EnumValueToken<Encoding>, 4> ExpectedType; };
        template <> struct ValueMap<Headers::TransferEncoding>  { typedef ValueList<EnumValueToken<Encoding>, 4> ExpectedType; };
        template <> struct ValueMap<Headers::Upgrade>           { typedef StringValue ExpectedType; };
        template <> struct ValueMap<Headers::UserAgent>         { typedef StringValue ExpectedType; };


    }

    struct GenericHeaderParser
    {
        /** Parse the given data stream until the header is found (stop at value) */
        static ParsingError parseHeader(ROString & input, ROString & header)
        {
            input = input.trimmedLeft();
            if (!input) return EndOfRequest; // End of headers here or error
            header = input.splitUpTo(":").trimRight(' ');
            return MoreData;
        }

        /** Skip value for this header */
        static ParsingError skipValue(ROString & input)
        {
            input.splitUpTo("\r\n");
            return MoreData;
        }

        /** Parse the given data stream until the end of the value, should be at value */
        static ParsingError parseValue(ROString & input, ROString & value)
        {
            input = input.trimLeft(' ');
            if (!input) return InvalidRequest;
            value = input.splitUpTo("\r\n").trimRight(' ');
            return MoreData;
        }
    };

    /** Request header line parsing, as specified in section 5.3 */
    struct GenericRequestHeaderLine
    {
        /** The header name */
        ROString header;
        /** The header value */
        ROString value;

        // Interface
    public:
        /** Parse the given data stream until the header is found (stop at value) */
        ParsingError parseHeader(ROString & input) { return GenericHeaderParser::parseHeader(input, header); }
        /** Skip value for this header */
        ParsingError skipValue(ROString & input) { return GenericHeaderParser::skipValue(input); }
        /** Parse the given data stream */
        ParsingError parse(ROString & input)
        {
            if (ParsingError err = parseHeader(input); err != MoreData) return err;
            return GenericHeaderParser::parseValue(input, value);
        }
        /** Extract header name type
            @return InvalidHeader upon unknown or invalid header */
        Headers getHeaderType() const { return Refl::fromString<Headers>(header).orElse(Headers::Invalid); }
    };

    struct RequestHeaderBase
    {
        /** The header value */
        ROString rawValue;

        ParsingError parse(ROString & input)
        {
            ROString header;
            if (ParsingError err = GenericHeaderParser::parseHeader(input, header); err != MoreData) return err;
            if (!acceptHeader(header)) return Protocol::HTTP::GenericHeaderParser::skipValue(input);
            return acceptValue(input, rawValue);
        }

        virtual bool acceptHeader(ROString & header) const { return true; }
        virtual ParsingError acceptValue(ROString & input, ROString & value) { return GenericHeaderParser::parseValue(input, value); }
    };

    /** Type specified request header line and value */
    template <Headers h>
    struct RequestHeader : public RequestHeaderBase
    {
        typename RequestType::ValueMap<h>::ExpectedType parsed;
        static constexpr Headers header = h;

        /** Check to see if this header is the expected type and in that case, capture the value */
        bool acceptHeader(ROString & hdr) const { return hdr == Refl::toString(h); }
        /** Accept the value for this header */
        virtual ParsingError acceptValue(ROString & input, ROString & val) {
            val = input.splitUpTo("\r\n");
            ROString tmp = val;
            val = val.trimRight(' ');
            return parsed.parseFrom(tmp);
        }
    };
}

#endif
