# Notes about compliance with RFC 2616

## What this project aims for

Building an embedded webserver on an embedded system for the smallest possible code size, and the largest possible feature set.
For each **possible** feature, a cost/utility ratio is computed and if it's in defavor of the project, it's not implemented or worked around.

## What this project isn't

This project isn't a full featured web server, like Apache or Nginx. It doesn't support many features but should support the minimal feature set to be:
1. Compact
2. Auditable
3. Usable
4. Embedable

So when some specification implies breaking one of the feature set above, it's either dropped (if the non compliance doesn't prevent the whole project to work),
or worked around.

## List of dropped features

### HTTP Proxy

HTTP Proxy implies many costly requirments like URL parsing (it's very hard to make a compliant URL parser) and a HTTP server doesn't need one.
It also implies handling many query & response headers.

**Decision**: HTTP proxy isn't and won't be supported.

### HTTPS - HTTP over TLS

While HTTPS is a major requirement on the global Internet, the current status-quo implies that for having a SSL/TLS server, an embedded server can should implement
certificate requesting automated bot (usually called certbot), since certificate are becoming shorter and shorted with time (major SSL certificate issuer will now refuse
to create certificate for more than one year validity). For an embedded server with sparse internet connection (if any), it's impossible to support and managing
certificate expiration time also implies that the embedded server has a time synchronization primitive with Internet.
In addition, the embedded server is unlikely to have a public routable IP address and a valid domain name (althrough it's possible to workaround those), so even with a
certification bot, it's both impossible to validate a local private address or to manipulate DNS server for DNS challenge passing.

Thus the cost of HTTPS is very high: TLS libraries are larger and clumsy, automation bot for certificate, NTP client for time synchronization, DNS and/or HTTP challenge too.

**Note**: Current IoT system "solves" this issue by connecting with a HTTP client (or MQTT) to a secure global HTTPS server on the internet and redirect the user to this global service.
          This implies that on one hand, data is shared with the global service for accessing it (and probably resold) and on the second hand,  once the global service isn't economical anymore, it's shut down and the IoT system becomes e-Waste. This isn't a viable solution and a source of ranting against IoT in general

**Decision**: HTTPS isn't and won't be supported.

### HTTP Cache

HTTP caching is a useful feature to skip request the same content again and again.
It's mainly used to reduce the server's bandwidth bills and accelerate page rendering when revisiting a website.

The implied cost of caching is mainly in computing a fingerprint of the requested ressource and checking if this fingerprint matches what the browser already knows about.
It also implies maintaining time to invalidate cached content.

**Decision**: Since an embedded system isn't really suited to compute fingerprint for its served ressources, there's no code implementing the feature in this server.
              That doesn't prevent the developper to store fingerprinting of the static content on her own and answering/parsing with the appropriate headers but its not done automatically

### HTTP2 / HTTP3

Both these versions are costly to implement for limited advantges for an embedded server.

**Decision**: This server's developer decided he had better use of his time skipping learning a new protocol for no practical benefit

### Websockets

Decision isn't made yet on this topic. Supporting websocket isn't too large by itself, but many other mechanisms exists for the same feature (like SSE) that would use less binary space by themselves.

**Decision**: Not supported yet. Maybe in the future.

### CR or LF fallback for end of line

Previous HTTP/1.0 client could have used only CR or LF as end of line (but CRLF is a single end of line). RFC2616 states that only CRLF should be used for end of line (section 2.2).
Thus, in order to simplify parsing, only CRLF is expected for end of line and old or incompatible client won't be able to request an answer. I have yet to see a non compliant client.

**Decision**: No tolerance for non compliant client, only CRLF supported

### Request URI extended support

As stated in the HTTP proxy part above, the request URI may contain an absolute URI or an authority.
The former is only required for HTTP proxy and the latter for supporting the CONNECT method.
Since none of them is supported in this server, the request URI parsing is simplified to only support "*" or an absolute path to the resource.

**Decision**: No proxy and no CONNECT method support means must simplified request URI parsing code.

### Range support

Accepting ranges is a method to only send part of an reply to the client. This is usually required for downloading large file, allowing to resume a failed download or download them via multiple clients at the same time (to maximize bandwidth from the server). On an embedded system, it doesn't really make any sense, but the impact isn't very high

**Decision**: Range support is possible, but disabled by default. It's up to the developer to implement range parsing from the header and spliting the source document.

### Accept Charset

This HTTP header is obsolete and not used by user agent anymore.

**Decision**: Accept-Charset is possible, but disabled by default. It's up to the developer to enable the charset parsing.
