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


/** Enable max compatibility support with RFC2616 (HTTP) standard.
    Allows to support more features in the HTTP server.
    This increases the binary size
    Default: 0 */
#define MaxSupport            1

#endif
