# Implementation details for space saving

### Memory saving

The same ideas as in eMQTT5 are used here. That is, no (or as low as possible and made at construction time) heap allocation.
Code is split to reuse a maximum (no code repeating anywhere), template on the underlying types.

### Request headers
Unlike conventional servers that are storing the whole HTTP request in memory and often a copy of the parsed header too, this server make use of stream parsing instead.
In effect, when a route is declared on this server, the set of accepted header are also declared (this is a static set of enumeration value, so with minimal impact on memory requirement).
When the request is being sent to the server, the parser starts dealing with the received request, and remembers the last CRLF (end of line) position.
Then it calls the parsing method for each stage of the parsing (starting from the RequestLine) to the RequestHeaderLine.
If an header isn't in the set of the expected header, it's ignored until next the CRLF is found (both being fast and avoid storing useless header anyway).
The parser doesn't make any copy of the input data and works on view only, so it's mainly skipping char and advancing pointers.
If the header is in the set of the expected header, the value is parsed and stored in the header map (technically, a fixed sized array the size of the set holding string views of the value). This array is "allocated" from an atomic circular buffer beforehand.
If the received data is incomplete (meaning that no CRLFCRLF was found in the data), the last RequestHeaderLine parsing will fail and the client will be put back in the FIFO of client to process when more data is received. Parsing will resume from just after the last CRLF received.


Therefore a client object is mainly storing this:
struct Client
{
    Socket socket;
    RequestLine requestLine;
    Set<N>      acceptedHeaders;
    Vector<N, ROString> values;
    Vector<M, bytes> receiveBuffer;
}