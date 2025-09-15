#ifndef hpp_HTTPDConfig_hpp
#define hpp_HTTPDConfig_hpp


/** Client transcient-vault buffer size. Must be a power of 2.
    This is used to store the transcient data received from the client (like headers and content of an HTTP request)
    and also keeps important information.
    Default: 1024 */
#define ClientBufferSize      1024


/** Enable SSL/TLS code for server.
    It's quite rare that an embedded server requires TLS, since certificate management is almost impossible to ensure
    on an embedded system.

    Default: 0 */
#define UseTLSServer          0

/** Enable SSL/TLS code for client.
    Use a client that accept to connect to any HTTPS website.

    Default: 0 */
#define UseTLSClient          0

/** Build a HTTP client too
    A HTTP client is very similar to a server for message parsing, so it makes senses to also
    build a HTTP client to avoid wasting another HTTP client library code (and parser) in your binary

    Default: 0 */
#define BuildClient           1

/** Prefer more code to less memory usage
    If this parameter is set, the code will try to limit using stack and/or heap space to create HTTP
    protocol's buffers, and instead will directly write to the socket (thus deporting the work to the network stack)

    Default: 0 */
#define MinimizeStackSize     1


/** Enable max compatibility support with RFC2616 (HTTP) standard.
    Allows to support more features in the HTTP server.
    This increases the binary size
    Default: 0 */
#define MaxSupport            1


#if UseTLSServer == 1 || UseTLSClient == 1
  #define UseTLS 1
#else
  #define UseTLS 0
#endif

#endif
