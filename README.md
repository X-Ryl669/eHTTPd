# eHTTPD is a webserver for embedded system with minimal ressources

## Features
1. HTTP/1.0 and HTTP/1.1 web server
2. Fits in TBD memory and TBD flash size on TBD architecture
3. Written in recent C++, hand optimized for minimum size
4. No heap allocation required while running (no leak, no crash due to memory fragmentation)
5. Contains many features for a small and efficient user code (including Headers parsing and extracting, URL, and Form parsing, Firmware file uploading, Chunked/Stream sending transfer, ...)

## Usage

Please refer to tests' [RouteTesting.cpp](/tests/RouteTesting.cpp) for example usage code that you can copy and paste and adapt to your needs.

**Few things about this code:**
1. All that can be known at compile time will be `constexpr` and optimized away from the binary whenever possible (see example below). That's possible since C++17, but made convenient to write, use and maintain since C++20. So the minimum compiler requirement for this code is C++20.
2. Many chore code in HTTP server usage are included in the library, opposite to usual embedded webserver libraries. This makes final code easy to write, without having to read and understand painful RFC standards
3. Platform independant. This code has been tested to run on Linux, MacOS and esp-idf targets without any change. This makes making simulator of the webserver (and development) easier.

## Example of C++ concepts used in this library

### 1. Whenever possible, use hash instead of string literals

Typical form (or URL's query) parsing look like this:
```cpp
   const char * input = // get URL or form encoded data
   const char * decoded = URL_decode(input); // An O(N) algorithm with at least one allocation for the resulting string
   while (found) {
      const char * in_key = strstr(decoded, the_key_you_are_looking_for); // O(N²) algorithm
      if (inKey) {
         const char * value = strchr(in_key, '='); // O(N) algorithm
         if (!value) { decoded = in_key + 1; break; }
         value++;
         trim_left_space(value);
         const char * end = strchr(value, '&'); // O(N) algorithm
         if (end) *end = 0;
         return value;
      }
      return nullptr;
   }
```
The pseudo code above doesn't deal with URL decoding well, but the overall cost computation is right.
It will parse the input string (at least) 4 times (to look for relevant parts), requires to store the `the_key_you_are_looking_for` in the binary (and the various parser details, like `\r\n`, `&`, `=`), and allocate up to 3 times the input string on the heap.
This is done each time you are extracting a value since the code can't or doesn't know by advance what keys you are interested in.

**eHTTPd** code instead perform like this:
```cpp
   template<auto ... keys>
   struct HashFormPost {
       string_view values[sizeof...(keys)]; // We expect to extract up to count(keys) values

       constexpr size_t findIndexInKeys(u32 hash); // Returns the index of the given hash in the keys we are interested in, or sizeof...(keys) else. O(N) with N = count(keys)

       void parse(string_view input) {
           string_view decoded = URL_decode_in_place(input); // O(N) algorithm, but update the input in place with no allocation
           while(decoded)
           {
              u32 keyHash = decoded.split_at('=').hash(); // O(N) algorithm
              size_t pos = findIndexInKeys(keyHash);
              string_view value = decoded.split_at('&'); // Ditto
              if (pos != sizeof...(keys))
                values[pos] = value;
           }
       }

       template<u32 hash>
       string_view getValue() {
          string_view r;
          size_t pos = findIndexInKeys(hash);
          if (pos != sizeof...(keys)) r = values[pos];
          return r;
       }
   };

   // Usage code is like this:
   HashFormPost<constHash(the_key_you_are_looking_for)> form;
   // Done once in route callback
   form.parse(input);

   // Then you can extract the already parsed value like this
   auto v = form.getValue<constHash(the_key_you_are_looking_for)>();
```
It never stores the literal for the keys (only the hash of the key, which is fixed size (typically 4 bytes) and is computed at compile time, not runtime).
Since you're declaring (at compile time, in the template value list) the keys you're interested in, it doesn't waste storage for useless keys and simply skips them while its single parsing phase. It's not allocating any memory at runtime.
So if you need to extract 4 keys from a query, it'll be parsed only once, which is 3 O(N) pass on the input string, and getting the appropriate value index in the array are typically replaced by O(1) algorithm by the compiler.

### 2. Declare beforehand what you need so it can cut the fat away

In usual HTTP server library, the parser doesn't know what you'll need, so it must keep the code to read and understand everything in the request even if you are only interested in 10 bytes of the query (usually, the URL). This makes HTTP parser large and complex and hard for the compiler to optimize away.

In **eHTTPd**, the route are declared `constexpr` (you can't have dynamic route) and you declare the HTTP headers you are interested into.
All other HTTP headers are simply skipped and ignored (a code to skip an HTTP header consist in finding the next end-of-line, while parsing an HTTP header is a lot more complex and error prone).
With the latter point below, this makes the HTTP request parsing and matching extremely efficient and small.

You code for your server will thus look like this (should be self explainatory):
```cpp
    auto Color = [](Client & client, auto headers) { ... }; // The route callback
    // Same for LongAnswer, PostFile, CatchAll used below

    constexpr Router<
        Route<Color, MethodsMask{ Method::GET, Method::POST }, "/Color", Headers::ContentLength, Headers::ContentDisposition>{},
        Route<LongAnswer, Method::GET, "/long", Headers::AcceptLanguage >{},
        Route<PostFile, Method::POST, "/postFile", Headers::ContentType >{},
        DefaultRoute<CatchAll, Method::GET>{}
    > router;
```
Notice that the `router` is a `constexpr` variable that's build and optimized at compile time.
This will write a compile time code that'll perform like this pseudo code:
```cpp
if (auto route = routes.filter(client.method).match(client.URL))
   route.callback(client, route.headers);
```

The match method is performing a O(N) string search on all the possible routes.
It's possible to use `SimilarRoutes` too if you don't need dynamic URL routes and you are interested in the same set of headers in each route.
In that case the string literal for the route isn't saved in the binary, only the hash is (see above point for the induced saving).

### 3. Use of reflection for parsing

In typical HTTP library parser, it has to either, include all possible the HTTP headers parsing code, status and error code or focus on few of them or delegate the parsing to the client of the code.
This hides away the cost of the parser and likely result in bugs on your side (after all, there are multiple way to shoot yourself in the foot while parsing HTTP headers).
All embedded HTTP parser used in embedded world only focus on vital headers parsing (like `Content-Length`) and delegate the rest to your code.

**eHTTPd** doesn't do that, it embeds the code to parse all HTTP headers declared in the RFC. This means that when your callback is called, the headers are already parsed, and you can fetch the meaningful value directly. Since you say which headers you're interested in at compile time, most of the code for parsing the **other** headers isn't included in the final binary, still resulting in a low footprint for your code.

To avoid tedious and repetitive code for header parsing, **eHTTPd** makes use of *pseudo* reflection from enum name to/from string (or hash) and compile time maps (telling what enum value expect what value type). Thus the code to figure out what to do with `Accept-Language` is simply declared as a specialized template type `ValueMap<AcceptLanguage, ValueListWithAttributes<Languages>>`.

The mapping to/from enum is using some kind of compile time coding in the names (since C++ variable names have strong limitations in what can be done, you can't have a variable named `Accept-Language` or `application/x-www-form-urlencoded`, so a name is made like this `AcceptLanguage`, `application_xWwwFormUrlencoded`).

Notice that parsing `fromString` (that is, converting from the received `"Accept-Language"`) only need the hash of the string (not the string itself).
However parsing `toString` (that is, converting an answer header `AcceptLanguage` to `"Accept-Language"`) does need the whole string in the binary.

There are multiple optimization in the library to avoid storing string literals when not required, but it's not possible to know beforehand if you'll need to generate those string or not. If you don't need that, you can declare a template specialization for the enumrarion for a header type to only use hashes (resulting in a smaller binary).

Also notice that the enum reflection used in the library is expecting the enums to be stored sorted in their string representation, allowing O(log N) search when matching string to enum value (a very welcome optimization when you see the possible number of headers in HTTP) instead of O(N*M).

### 4. Splitting network code in 2 (independant) steps

Usual HTTP library parser parses the input request, and trigger the callback for the route (some do that early in the parsing process, some do that when the request is complete). This means that all the information for all the routes must be available in the library, and any large "route" will impact all other routes.

For example, if one route needs to parse the `Content-Disposition` header, this means that the library will need to store the value for this header (usually on the heap), before figuring out what route callback to call.

In **eHTTPd**, the network code and parsing is split in these steps:
1. Accept the client socket (usual with all HTTP server)
2. Receive a first buffer containing at least the first line of the request (usual with all HTTP server)
3. Figure out the route to match (if none, errors out) from the URI found in the first line. (From now, this is unusual with other HTTP server)
4. The route code will instantiate, on the stack, the headers storage list it's interested in, and "rewrite" the next parsing code
5. Only the interesting headers are parsed (other are simply skipped)
6. Call the route callback with the client and the headers' values.

All of this is possible because of the `TranscientBuffer` container which is like a ring buffer with 2 heads.
The left head tracks transcient data (data that'll be wiped away while receiving the next part of the request) and the right head tracks vault data (data that need to persist for the client to process the request).

When the route to process is known, the headers's value are stored in the vault area of the `TranscientBuffer` so they aren't wiped out when the request is large and need refilling the buffer with next headers. Only the route code knows how/where to find the value in the vault, and only the data for **this** route is saved in the vault.
This allows to never allocate anything on the heap while processing a HTTP request, be fast for routes with few headers and slower for those with larger requirements.

It's now possible to parse large request (like a 800 bytes request) with a single 512 byte buffer (in 2 passes), limiting the client memory size requirement on the embedded system. Similarly, if you have more RAM available, a larger buffer will avoid storing to/from the vault, speeding up the process.


### 5. Answer helpers

The same principle for request are implemented for answering the request. This makes writing a route callback easy and short, for example:
```cpp
// A REST POST/GET method some parameter
auto Color = [](Client & client, const auto & headers)
{
    // You can do a runtime method testing like this or a compile time testing while registering the route for a post method only
    if (client.reqLine.method == Method::POST) {
        // We don't want to store "some_param" in the binary here, so instead store the hash of this string, much smaller
        // If the parameter is not known at compile time (unlikely), you can use FormPost instead.
        HashFormPost<"some_param"_hash, "value"_hash> form;

        if (!client.fetchContent(headers, form))
            return client.closeWithError(Code::BadRequest);

        // We can use the found parameter here (even if it's in the same buffer as the reply, it'll be copied in the vault only if required)
        return client.reply(Code::Ok, form.getValue<"some_param"_hash>());
    }
    return client.reply(Code::Ok, "GET default output");
};

// Example to show how you could write your file serving route (catch all)
auto CatchAll = [](Client & client, const auto & headers)
{
    // Simple catch all callback, used for serving files
    // First, check the requested URI (it's already URL decoded and normalized [i.e "/b/../../a" => "/a"] for you)
    ROString path = client.getRequestedPath();
    // Ok, make an FileAnswer that'll copy the file content to the send buffer repeatedly until done.
    FileAnswer<Streams::FileInput> answer(path);
    // Example setting some headers for the answer (useless here)
    answer.setHeader<Headers::ContentEncoding>(Encoding::identity);
    return client.sendAnswer(answer);
};
```
Code is explicit and easy to follow. No need to deal with HTTP strangeness here.