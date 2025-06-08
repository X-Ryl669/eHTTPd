#include <stdio.h>

// We are testing method parsing and header parsing in this file, so let's include the required stuff
#include "Protocol/HTTP/Methods.hpp"
// We are also testing HTTP status code parsing too
#include "Protocol/HTTP/Codes.hpp"
// We need request lines too for testing
#include "Protocol/HTTP/RequestLine.hpp"

#include "Container/CTVector.hpp"

using namespace Protocol::HTTP;


template <typename A>
bool testEqual(const A a, const Refl::Opt<A> b)
{
    if (a != b.orElse((A)-1))
    {
        fprintf(stderr, "Failed test for %d, got %d\n", (int)a, (int)b.get());
        return false;
    }
    return true;
}

bool testEqual(const char * a, const char * b)
{
    if (ROString(a) != ROString(b))
    {
        fprintf(stderr, "Failed test for %s, got %s\n", a, b);
        return false;
    }
    return true;
}

template <Headers E>
struct MakeRequest { typedef Protocol::HTTP::RequestHeader<E> Type; };

int main()
{
    // Method
    if (!testEqual("DELETE",    toString(Method::DELETE))) return 1;
    if (!testEqual("GET",       toString(Method::GET))) return 1;
    if (!testEqual("HEAD",      toString(Method::HEAD))) return 1;
    if (!testEqual("POST",      toString(Method::POST))) return 1;
    if (!testEqual("PUT",       toString(Method::PUT))) return 1;
    if (!testEqual("OPTIONS",   toString(Method::OPTIONS))) return 1;

    if (!testEqual(Method::DELETE, fromString<Method>("DELETE"))) return 1;
    if (!testEqual(Method::DELETE, fromString<Method>("delete"))) return 1;
    if (!testEqual(Method::DELETE, fromString<Method>("DELETe"))) return 1;
    fprintf(stderr, "Expecting failure here: ");
    if (testEqual(Method::DELETE, fromString<Method>("DELETEn"))) return 1;
    if (!testEqual(Method::GET, fromString<Method>("GET"))) return 1;
    if (!testEqual(Method::HEAD, fromString<Method>("HEAD"))) return 1;
    if (!testEqual(Method::POST, fromString<Method>("POST"))) return 1;
    if (!testEqual(Method::PUT, fromString<Method>("PUT"))) return 1;
    if (!testEqual(Method::OPTIONS, fromString<Method>("OPTIONS"))) return 1;

    if (MethodsMask{Method::DELETE, Method::GET}.mask != 3) return 1;


    // Headers
    if (!testEqual("Accept",                        toString(Headers::Accept))) return 1;
    if (!testEqual("Accept-Charset",                toString(Headers::AcceptCharset))) return 1;
    if (!testEqual("Accept-Encoding",               toString(Headers::AcceptEncoding))) return 1;
    if (!testEqual("Accept-Language",               toString(Headers::AcceptLanguage))) return 1;
    if (!testEqual("Accept-Ranges",                 toString(Headers::AcceptRanges))) return 1;
    if (!testEqual("Access-Control-Allow-Origin",   toString(Headers::AccessControlAllowOrigin))) return 1;
    if (!testEqual("Authorization",                 toString(Headers::Authorization))) return 1;
    if (!testEqual("Cache-Control",                 toString(Headers::CacheControl))) return 1;
    if (!testEqual("Connection",                    toString(Headers::Connection))) return 1;
    if (!testEqual("Content-Disposition",           toString(Headers::ContentDisposition))) return 1;
    if (!testEqual("Content-Encoding",              toString(Headers::ContentEncoding))) return 1;
    if (!testEqual("Content-Language",              toString(Headers::ContentLanguage))) return 1;
    if (!testEqual("Content-Length",                toString(Headers::ContentLength))) return 1;
    if (!testEqual("Content-Range",                 toString(Headers::ContentRange))) return 1;
    if (!testEqual("Content-Type",                  toString(Headers::ContentType))) return 1;
    if (!testEqual("Cookie",                        toString(Headers::Cookie))) return 1;
    if (!testEqual("Date",                          toString(Headers::Date))) return 1;
    if (!testEqual("Expires",                       toString(Headers::Expires))) return 1;
    if (!testEqual("Host",                          toString(Headers::Host))) return 1;
    if (!testEqual("Last-Modified",                 toString(Headers::LastModified))) return 1;
    if (!testEqual("Location",                      toString(Headers::Location))) return 1;
    if (!testEqual("Origin",                        toString(Headers::Origin))) return 1;
    if (!testEqual("Pragma",                        toString(Headers::Pragma))) return 1;
    if (!testEqual("Range",                         toString(Headers::Range))) return 1;
    if (!testEqual("Referer",                       toString(Headers::Referer))) return 1;
    if (!testEqual("Server",                        toString(Headers::Server))) return 1;
    if (!testEqual("Set-Cookie",                    toString(Headers::SetCookie))) return 1;
    if (!testEqual("TE",                            toString(Headers::TE))) return 1;
    if (!testEqual("Transfer-Encoding",             toString(Headers::TransferEncoding))) return 1;
    if (!testEqual("Upgrade",                       toString(Headers::Upgrade))) return 1;
    if (!testEqual("User-Agent",                    toString(Headers::UserAgent))) return 1;
    if (!testEqual("WWW-Authenticate",              toString(Headers::WWWAuthenticate))) return 1;


    // MIME types
#if defined(MaxSupport)
    if (!testEqual("application",                   toString(MediaType::application))) return 1;
    if (!testEqual("audio",                         toString(MediaType::audio))) return 1;
    if (!testEqual("font",                          toString(MediaType::font))) return 1;
    if (!testEqual("image",                         toString(MediaType::image))) return 1;
    if (!testEqual("model",                         toString(MediaType::model))) return 1;
    if (!testEqual("multipart",                     toString(MediaType::multipart))) return 1;
    if (!testEqual("text",                          toString(MediaType::text))) return 1;
    if (!testEqual("video",                         toString(MediaType::video))) return 1;

    if (!testEqual("ecmascript",                    toString(ApplicationType::ecmascript))) return 1;
    if (!testEqual("javascript",                    toString(ApplicationType::javascript))) return 1;
    if (!testEqual("json",                          toString(ApplicationType::json))) return 1;
    if (!testEqual("octet-stream",                  toString(ApplicationType::octetStream))) return 1;
    if (!testEqual("pdf",                           toString(ApplicationType::pdf))) return 1;
    if (!testEqual("xhtml+xml",                     toString(ApplicationType::xhtml__xml))) return 1;
    if (!testEqual("xml",                           toString(ApplicationType::xml))) return 1;
    if (!testEqual("zip",                           toString(ApplicationType::zip))) return 1;

    if (!testEqual("apng",                          toString(ImageType::apng))) return 1;
    if (!testEqual("avif",                          toString(ImageType::avif))) return 1;
    if (!testEqual("gif",                           toString(ImageType::gif))) return 1;
    if (!testEqual("jpeg",                          toString(ImageType::jpeg))) return 1;
    if (!testEqual("png",                           toString(ImageType::png))) return 1;
    if (!testEqual("svg+xml",                       toString(ImageType::svg__xml))) return 1;
    if (!testEqual("vnd.microsoft.icon",            toString(ImageType::vnd___microsoft___icon))) return 1;
    if (!testEqual("webp",                          toString(ImageType::webp))) return 1;
#endif

    if (!testEqual("application/ecmascript",        toString(MIMEType::application_ecmascript))) return 1;
    if (!testEqual("application/javascript",        toString(MIMEType::application_javascript))) return 1;
    if (!testEqual("application/json",              toString(MIMEType::application_json))) return 1;
    if (!testEqual("application/octet-stream",      toString(MIMEType::application_octetStream))) return 1;
    if (!testEqual("application/pdf",               toString(MIMEType::application_pdf))) return 1;
    if (!testEqual("application/xhtml+xml",         toString(MIMEType::application_xhtml__xml))) return 1;
    if (!testEqual("application/xml",               toString(MIMEType::application_xml))) return 1;
    if (!testEqual("application/zip",               toString(MIMEType::application_zip))) return 1;
    if (!testEqual("audio/mpeg",                    toString(MIMEType::audio_mpeg))) return 1;
    if (!testEqual("audio/vorbis",                  toString(MIMEType::audio_vorbis))) return 1;
    if (!testEqual("font/otf",                      toString(MIMEType::font_otf))) return 1;
    if (!testEqual("font/ttf",                      toString(MIMEType::font_ttf))) return 1;
    if (!testEqual("font/woff",                     toString(MIMEType::font_woff))) return 1;
    if (!testEqual("image/apng",                    toString(MIMEType::image_apng))) return 1;
    if (!testEqual("image/avif",                    toString(MIMEType::image_avif))) return 1;
    if (!testEqual("image/gif",                     toString(MIMEType::image_gif))) return 1;
    if (!testEqual("image/jpeg",                    toString(MIMEType::image_jpeg))) return 1;
    if (!testEqual("image/png",                     toString(MIMEType::image_png))) return 1;
    if (!testEqual("image/svg+xml",                 toString(MIMEType::image_svg__xml))) return 1;
    if (!testEqual("image/vnd.microsoft.icon",      toString(MIMEType::image_vnd___microsoft___icon))) return 1;
    if (!testEqual("image/webp",                    toString(MIMEType::image_webp))) return 1;
    if (!testEqual("model/3mf",                     toString(MIMEType::model_3mf))) return 1;
    if (!testEqual("model/vrml",                    toString(MIMEType::model_vrml))) return 1;
    if (!testEqual("multipart/form-data",           toString(MIMEType::multipart_formData))) return 1;
    if (!testEqual("multipart/byteranges",          toString(MIMEType::multipart_byteranges))) return 1;
    if (!testEqual("text/*",                        toString(MIMEType::text_all))) return 1;
    if (!testEqual("text/css",                      toString(MIMEType::text_css))) return 1;
    if (!testEqual("text/csv",                      toString(MIMEType::text_csv))) return 1;
    if (!testEqual("text/html",                     toString(MIMEType::text_html))) return 1;
    if (!testEqual("text/javascript",               toString(MIMEType::text_javascript))) return 1;
    if (!testEqual("text/plain",                    toString(MIMEType::text_plain))) return 1;
    if (!testEqual("*/*",                           toString(MIMEType::all))) return 1;

    // HTTP status code
    if (!testEqual("Continue",                      toString(Code::Continue))) return 1;
    if (!testEqual("Ok",                            toString(Code::Ok))) return 1;
    if (!testEqual("Created",                       toString(Code::Created))) return 1;
    if (!testEqual("Accepted",                      toString(Code::Accepted))) return 1;
    if (!testEqual("Non Auth Info",                 toString(Code::NonAuthInfo))) return 1;
    if (!testEqual("No Content",                    toString(Code::NoContent))) return 1;
    if (!testEqual("Reset Content",                 toString(Code::ResetContent))) return 1;
    if (!testEqual("Partial Content",               toString(Code::PartialContent))) return 1;
    if (!testEqual("Multiple Choices",              toString(Code::MultipleChoices))) return 1;
    if (!testEqual("Moved Forever",                 toString(Code::MovedForever))) return 1;
    if (!testEqual("Moved Temporarily",             toString(Code::MovedTemporarily))) return 1;
    if (!testEqual("See Other",                     toString(Code::SeeOther))) return 1;
    if (!testEqual("Not Modified",                  toString(Code::NotModified))) return 1;
    if (!testEqual("Use Proxy",                     toString(Code::UseProxy))) return 1;
    if (!testEqual("Unused",                        toString(Code::Unused))) return 1;
    if (!testEqual("Temporary Redirect",            toString(Code::TemporaryRedirect))) return 1;
    if (!testEqual("Bad Request",                   toString(Code::BadRequest))) return 1;
    if (!testEqual("Unauthorized",                  toString(Code::Unauthorized))) return 1;
    if (!testEqual("Payment Required",              toString(Code::PaymentRequired))) return 1;
    if (!testEqual("Forbidden",                     toString(Code::Forbidden))) return 1;
    if (!testEqual("Not Found",                     toString(Code::NotFound))) return 1;
    if (!testEqual("Bad Method",                    toString(Code::BadMethod))) return 1;
    if (!testEqual("Not Acceptable",                toString(Code::NotAcceptable))) return 1;
    if (!testEqual("Proxy Required",                toString(Code::ProxyRequired))) return 1;
    if (!testEqual("Timed Out",                     toString(Code::TimedOut))) return 1;
    if (!testEqual("Conflict",                      toString(Code::Conflict))) return 1;
    if (!testEqual("Gone",                          toString(Code::Gone))) return 1;
    if (!testEqual("Length Required",               toString(Code::LengthRequired))) return 1;
    if (!testEqual("Precondition Fail",             toString(Code::PreconditionFail))) return 1;
    if (!testEqual("Entity Too Large",              toString(Code::EntityTooLarge))) return 1;
    if (!testEqual("URI Too Large",                 toString(Code::URITooLarge))) return 1;
    if (!testEqual("Unsupported MIME",              toString(Code::UnsupportedMIME))) return 1;
    if (!testEqual("Request Range",                 toString(Code::RequestRange))) return 1;
    if (!testEqual("Expectation Fail",              toString(Code::ExpectationFail))) return 1;
    if (!testEqual("Internal Server Error",         toString(Code::InternalServerError))) return 1;
    if (!testEqual("Not Implemented",               toString(Code::NotImplemented))) return 1;
    if (!testEqual("Bad Gateway",                   toString(Code::BadGateway))) return 1;
    if (!testEqual("Unavailable",                   toString(Code::Unavailable))) return 1;
    if (!testEqual("Gateway Timed Out",             toString(Code::GatewayTimedOut))) return 1;
    if (!testEqual("Unsupported HTTP Version",      toString(Code::UnsupportedHTTPVersion))) return 1;
    if (!testEqual("Connection Timed Out",          toString(Code::ConnectionTimedOut))) return 1;

    // Header line parsing
    ROString headersLine[] = { "POST /upload?to=me&name=John HTTP/1.1\r\n"
        "Host: localhost:4500\r\n"
        "Accept: */*\r\n"
        "Content-Length: 198\r\n"
        "Content-Type: multipart/form-data; boundary=------------------------46b4fe28f353713c\r\n"
        "User-Agent: curl/8.2.1\r\n"
        "\r\n"
        "--------------------------46b4fe28f353713c\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"test.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "test qwerty\r\n"
        "\r\n"
        "--------------------------46b4fe28f353713c--\r\n",

        "Transfer-Encoding: gzip, chunked\r\n",
        "Accept: text/html, application/xhtml+xml, application/xml;q=0.9, image/webp, */*;q=0.8\r\n",
        "Accept-Encoding: deflate, gzip;q=1.0, *;q=0.5\r\n",
        "Accept-Language: fr-CH, fr;q=0.9, en;q=0.8, de;q=0.7, *;q=0.5\r\n",
        "Cache-Control: max-age=604800, must-revalidate\r\n",
        "Connection: keep-alive\r\n",
        "Content-Encoding: gzip, deflate\r\n",
        "Content-Type: text/html; charset=utf-8\r\n",
        "Content-Length: 198\r\n",
        "Cookie: name=value; both=true\r\n",
    };

    // Test request line
    RequestLine line;
    if (line.parse(headersLine[0]) != MoreData) return 1;
    if (line.method != Method::POST || line.URI.absolutePath != "/upload?to=me&name=John" || line.version != Version::HTTP1_1) return fprintf(stderr, "Bad method, URI or version\n");
    if (line.URI.getQueryPart().getValueFor("to") != "me") return fprintf(stderr, "Wrong URI or query\n");

    Query query = line.URI.getQueryPart();
    ROString key, value;
    for (size_t qi = 0; query.iterateKeys(qi, key, value);)
        printf("Found query key %.*s = %.*s\n", key.getLength(), key.getData(), value.getLength(), value.getData());

    // Test header parsing too
    RequestHeader<Headers::TransferEncoding> te;
    if (te.parse(headersLine[1]) != EndOfRequest) return 1;
    for (size_t i = 0; i < te.parsed.count; i++)
        printf("%s found: %s\n", toString(te.header), toString(te.parsed.value[i].value));
    RequestHeader<Headers::Accept> a;
    if (a.parse(headersLine[2]) != EndOfRequest) return 1;
    for (size_t i = 0; i < a.parsed.count; i++)
        printf("%s found: %s\n", toString(a.header), toString(a.parsed.value[i].value));
    RequestHeader<Headers::AcceptEncoding> ae;
    if (ae.parse(headersLine[3]) != EndOfRequest) return 1;
    for (size_t i = 0; i < ae.parsed.count; i++)
        printf("%s found: %s\n", toString(ae.header), toString(ae.parsed.value[i].value));
    RequestHeader<Headers::AcceptLanguage> al;
    if (al.parse(headersLine[4]) == InvalidRequest) return 1;
    for (size_t i = 0; i < al.parsed.count; i++)
        printf("%s found: %s\n", toString(al.header), toString(al.parsed.value[i].value));
    RequestHeader<Headers::CacheControl> cc;
    if (cc.parse(headersLine[5]) != EndOfRequest) return 1;
    for (size_t i = 0; i < cc.parsed.count; i++)
        printf("%s found: %s with token %.*s\n", toString(cc.header), toString(cc.parsed.value[i].value), cc.parsed.value[i].attributes.getLength(), cc.parsed.value[i].attributes.getData());
    RequestHeader<Headers::Connection> c;
    if (c.parse(headersLine[6]) != EndOfRequest) return 1;
    printf("%s found: %s\n", toString(c.header), toString(c.parsed.value));
    RequestHeader<Headers::ContentEncoding> ce;
    if (ce.parse(headersLine[7]) != EndOfRequest) return 1;
    for (size_t i = 0; i < ce.parsed.count; i++)
        printf("%s found: %s\n", toString(ce.header), toString(ce.parsed.value[i].value));
    RequestHeader<Headers::ContentType> ct;
    if (ct.parse(headersLine[8]) != EndOfRequest) return 1;
    printf("%s found: %s with token %.*s (charset: %.*s)\n", toString(ct.header), toString(ct.parsed.value), ct.parsed.attributes.getLength(), ct.parsed.attributes.getData(), ct.parsed.findAttributeValueFor("charset").getLength(), ct.parsed.findAttributeValueFor("charset").getData());
    RequestHeader<Headers::ContentLength> cl;
    if (cl.parse(headersLine[9]) != EndOfRequest) return 1;
    printf("%s found: %lu\n", toString(cl.header), cl.parsed.value);
    RequestHeader<Headers::Cookie> co;
    if (co.parse(headersLine[10]) != EndOfRequest) return 1;
    printf("%s found: %.*s (both: %.*s)\n", toString(co.header), co.parsed.value.getLength(), co.parsed.value.getData(), co.parsed.findValueFor("both").getLength(), co.parsed.findValueFor("both").getData());




    auto j = Container::makeTypes<MakeRequest, std::array<Headers, 4>{Headers::TransferEncoding, Headers::AcceptEncoding, Headers::Connection, Headers::Cookie}>();
    auto k = Container::withMinimumTypes<MakeRequest, std::array<Headers, 4>{Headers::TransferEncoding, Headers::ContentEncoding, Headers::Connection, Headers::Cookie}, std::array<Headers, 2>{Headers::Authorization, Headers::Upgrade}>();

    constexpr auto l = Container::getUnique<std::array<Headers, 4>{Headers::TransferEncoding, Headers::ContentEncoding, Headers::Connection, Headers::Cookie}, std::array<Headers, 3>{Headers::Authorization, Headers::Upgrade, Headers::Cookie}>();
    printf("Size: %d [", l.size());
    for (auto e: l) printf("%s, ", toString(e));
    printf("]\n");
    //
    // static_assert(j == k);


    fprintf(stdout, "OK\n");
    return 0;

}
