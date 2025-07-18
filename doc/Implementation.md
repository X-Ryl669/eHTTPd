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
If the header is in the set of the expected header, the value is parsed and stored in the header map (technically, a fixed sized array the size of the set holding string views of the value). This array is dealt with a TranscientBuffer which is a double head array (a transcient area which is wiped often and a vault which keeps the non volatile information extracted from the selected headers).
If the received data is incomplete (meaning that no CRLFCRLF was found in the data), the last RequestHeaderLine parsing will fail and the client will be put back in the FIFO of client to process when more data is received. Parsing will resume from just after the last CRLF received.


Therefore a client object is, conceptually mainly storing this:
struct Client
{
    Socket socket;
    RequestLine requestLine;
    Set<N>      acceptedHeaders;  // Dynamic depending on the route, stored on the stack
    Vector<N, ROString> values;   // The static information from the headers
    Vector<M, bytes> receiveBuffer;
}

### Socket pool

While the server runs its main loop, it makes a pool of sockets to listen events to.
The server socket itself is in the pool, so accepting new client is done as soon as the loop runs.
Similarly, HTTP header parsing is done progressively (so if the whole headers can't be fetched in a single socket recv call, the whole pool will be monitored on the next loop, and the client will only progress parsing and receiving at that time too).
This allow to allocate as fair processing power to all clients and server's sockets.

However, once the client has received and parsed all its headers and calls the Route's callback function, it doesn't manage what that function does.
The library provides a lot of helper functions and classes to implement what's usually required for interfacing HTTP with code, but none of those will put the client back in the pool.
This means that if one client's callback function takes a long time to process with many socket/IO work, all other clients will be throttled/paused until it's done with this processing.

This is usually not an issue on embedded HTTP server since the number of client is very limited, but also because action requiring a large IO work are likely for firmware update or other vital importance and in that case, not serving the other client is short time will have limited impact.