#ifndef hpp_JSONSerDes_hpp
#define hpp_JSONSerDes_hpp

// We need readonly strings
#include "ROString.hpp"
#include "RWString.hpp"
// We need JSON too
#include "JSON.hpp"
// We need logs too for error reporting
//#include "Log.hpp"
#include <stdlib.h>
// We need automated struct parsing
#include "AutoEnum.hpp"
#include "AutoStruct.hpp"


// Allow serializing and deserializing std::vector or std::array
#define AllowSerializingDynamicContainer 1
// Add code for serialization (and the serialize function too)
#define AllowSerializing                 1

/** This file contains a magic JSON deserializer and serializer based on C++ reflection.

    Since C++20, and with a little bit of preprocessor help, it's possible to reflect basic, aggregate structures without writing any (de)serialization code.

    An aggregate is an array or a class (clause 9) with no user-declared constructors (12.1), no private or protected non-static data members (clause 11), no base classes (clause 10), and no virtual functions (10.3).

    Typically, this is a basic simple structure that you can construct with {}.

    Since an example is better than long text, here is some sample code:
    @code
        struct A
        {
            int i;
            float f;
            double d;
            bool b;
            char text[16];
        };

        struct B
        {
            std::vector<int> series;
            std::string name;
        };


        A a;
        const char* text = R"({"i":-45.2, "f": 3.14, "d": 2.71, "b": true, "text": "hello world!" })";
        if (!deserialize(a, text))
            std::cerr << "Error deserializing text" << text << std::endl;
        dump(a); // Outputs: i = -45, f = 3.14, d = 2.71, b = true, text = "hello world!"
        printf(serialize(a)); // Outputs: {"i":-45,"f":3.14,"d":2.71,"b":true,"text":"hello world!"}

        B b;
        const char* text2 = R"({"name":"Time to market", "series": [3.14, 1.23, 3.45, 5.67, 7.89] })";
        if (!deserialize(b, text2))
            std::cerr << "Error deserializing text2" << text2 << std::endl;
        printf(serialize(b)); // Outputs: {"series":[3,1,3,5,7],"name":"Time to market"}
    @endcode

    Notice that no code was written for A or B serialization.
    The "dump" function can be as simple as this:
    @code
        template <typename T, typename ... Members>
        void dump(T & instance, std::tuple<Members...> const & tup)
        {
            std::apply([&instance](Members const &... args)
                {
                    ((std::cout << std::string_view(args.name().getData(), args.name().getLength()) << " = " << args.get(instance) << std::endl), ...);
                }, tup);

        }

        // Don't do that, it's bad
        namespace std {
           std::ostream& operator<<(std::ostream& s, std::vector<int> const& v) { for(auto const& elem : v) { s << elem << ", "; } return s; }
        }

        template <typename T>
        void dumpAggregate(T & t)
        {
          dump(t, Refl::Members::get_member_functors<T>(0));
        }
    @endcode */



/** A very simple LIFO class, with fixed size depth, no dynamic allocation */
template <typename T, size_t count>
struct LIFO
{
    T array[count];
    size_t top;

    void push(T v) { if (top == count) return; array[top++] = v; }
    T pop()        { if (!top) return 0; return array[--top];    }
    T peek()       { if (!top) return 0; return array[top - 1];  }
    size_t size() const  { return top; }

    LIFO() : top(0) {}
};

/** The main parser object that's implementing the logic for extracting keys from simple objects */
struct Parser
{
    typedef JSONT<int16> JSON;


    const ROString & data;
    JSON             parser;
    JSON::Token      token;
    LIFO<int16, 64>  superPos;
    int16            lastSuper;
    int16            errorPos;

    ROString current() const { return data.midString(token.start, token.end - token.start); }

    bool Error(JSON::IndexType res, const char * err = NULL) {
        errorPos = parser.pos;
        size_t start = (size_t)std::max(errorPos - 16, 0), end = std::min(start + 32, data.getLength());
        size_t prev = (size_t)std::min((int)errorPos, 16);

        const char* resName = res ? Refl::enum_value_name((JSON::ParsingResult)res) : "";
        if (!err) err = resName;
        elogm(Log::Error | Log::Format, "Parse error: %s@%d: %.*s > HERE < %.*s\"\n", err, errorPos, (int)prev, data.getData() + start, (int)((end - start) - prev), data.getData()+start + prev);

        return false;
    }

    bool parseNext()
    {
        if (parser.state == JSON::Done) return false;

        JSON::IndexType res = parser.parseOne(data.getData(), data.getLength(), token, lastSuper);
        if (res < 0) return Error(res);

        if (res == JSON::SaveSuper)
            superPos.push(lastSuper);
        if (res == JSON::RestoreSuper) {
            // Need to consume the last value anyway, it's useless
            if (superPos.size()) superPos.pop();
            lastSuper = superPos.size() ? superPos.peek() : (JSON::IndexType)JSON::InvalidPos;
        }
        if (res == JSON::Finished) {
            // We are done.
            return false;
        }
        return true;
    }

    ROString nextObjectKey()
    {
        if (token.state == JSON::HadKey) {
            ROString key = current();
            parseNext();
            return key;
        }

        if (token.state != JSON::LeavingObject) {
            Error(JSON::ParsingResult::Invalid);
            return ROString();
        }

        parseNext();
        return ROString();
    }

    ROString getString()
    {
        if (token.type == JSON::Token::String) return current();
        return ROString();
    }
    bool getBool()      { return token.type == JSON::Token::True; }
    double getDouble()  { return token.type == JSON::Token::Number ? (double)current() : 0.0; }
    int getInt()        { return token.type == JSON::Token::Number ? (int)current() : 0; }

    JSON::SAXState currentState() const { return (JSON::SAXState)token.state; }

    Parser(const ROString & in) : data(in), lastSuper(JSON::InvalidPos), errorPos(JSON::InvalidPos)
    {
        parseNext();
    }
};

namespace Details
{
    // You must specialize this function for non supported types
    template <typename U> RWString deserializeFromJSON(Parser & json, U & t, const bool allowPartial = false);


    template <typename T, typename ... Members>
    bool deserializeField(Parser & parser, RWString & err, const ROString & key, T & instance, std::tuple<Members...> const & tup)
    {
        bool found = false;
        std::apply([&found, &parser, &key, &err, &instance](Members const &... args)
            {
                ((key == args.name() && (found = true) && (err = deserializeFromJSON(parser, const_cast<std::remove_cvref_t<decltype(args.get(instance))> &>(args.get(instance))))), ...);
            }, tup);
        return found;
    }

    // Traits below used to detect the variable type at compile time
    template <class> struct is_bounded_char_array : std::false_type {};
    template <size_t N> struct is_bounded_char_array<char[N]> : std::true_type {};
    template <typename T> constexpr bool is_bounded_char_array_v = is_bounded_char_array<T>::value;
#ifdef AllowSerializingDynamicContainer
    template <typename>                     struct IsStdContainer : std::false_type { };
    template <typename T, typename... Ts>   struct IsStdContainer<std::vector<T, Ts...>> : std::true_type { };
    template <typename T, size_t N>         struct IsStdContainer<std::array<T, N>> : std::true_type { };
    template <typename T, typename... Ts>   constexpr bool is_std_container_v = IsStdContainer<T, Ts...>::value;
#endif

    template <typename U>
    RWString deserializeFromBasicType(Parser & parser, U & t)
    {
        using T = std::decay_t<U>;
        if (parser.currentState() != Parser::JSON::HadValue) return "Expected value";
        if constexpr (std::is_enum_v<T>)
        {
            // We accept either the enum name as string or the enum value here
            ROString val = parser.getString();
            if (val[0] >= '0' && val[0] <= '9') // Only positive integer supported here
                t = static_cast<T>(parser.getInt());
            else
                t = Refl::from_enum_value(val, T{});
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            t = parser.getBool();
        }
        else if constexpr (std::is_arithmetic_v<T>)
        {
            double v = parser.getDouble();
            t = static_cast<T>(v);
        }
        else if constexpr (std::is_same_v<T, RWString>)
        {
            t = parser.getString();
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            t = std::string(parser.getString().getData(), parser.getString().getLength());
        }
        else if constexpr (is_bounded_char_array_v<U>)
        {
            ROString json = parser.getString();
            memset(t, 0, sizeof(t));
            if (json.getLength() < sizeof(t))
                memcpy(t, json.getData(), json.getLength());
            else return "Given text is too large for the destination array";
        }
        else if constexpr (std::is_same_v<T, std::string_view> || std::is_convertible_v<T, const char *>)
        {
            static_assert(Refl::always_false_v<T>, "std::string_view or const char* are not deserializable, since the source will disappear after deserialization");
        }

        parser.parseNext();
        return "";
    }

    template <typename T>
    consteval bool isBasicType()
    {
        if constexpr (std::is_enum_v<T>) return true;
        else if constexpr (std::is_same_v<T, bool>) return true;
        else if constexpr (std::is_arithmetic_v<T>) return true;
        else if constexpr (is_bounded_char_array_v<T>) return true;
        else if constexpr (std::is_convertible_v<T, const char *> || std::is_same_v<T, RWString>) return true;
        else if constexpr (std::is_same_v<T, std::string_view> || std::is_same_v<T, std::string>) return true;
        else return false;
    }

    /** Deserialize from a JSON string to the expected type.
        @param json   The JSON string to deserialize from
        @param t      The expected type that should map the JSON string
        @return       An error string on failure or empty string on success */
    template <typename U>
    RWString deserializeFromJSON(Parser & parser, U & t, const bool allowPartial)
    {
        using T = std::decay_t<U>;
        if constexpr (isBasicType<T>())
            return deserializeFromBasicType(parser, t);
 #ifdef AllowSerializingDynamicContainer
        else if constexpr (is_std_container_v<T>)
        {
            if (parser.currentState() != Parser::JSON::EnteringArray) return "Expecting JSON array";
            parser.parseNext();

            T tmp;

            size_t i = 0;
            typename T::value_type V;
            while (true)
            {
                if (parser.currentState() == Parser::JSON::LeavingArray) break;

                RWString ret = deserializeFromJSON(parser, V);
                if (ret) return ret;

                if constexpr (requires { tmp.push_back(V); })
                    tmp.push_back(V);
                else
                {
                    if (tmp.size() > i)
                        tmp[i] = V;
                    else
                        return RWString::format("Array size (%d) too small", (int)tmp.size());
                    i++;
                }
            }
            t.swap(tmp);
            parser.parseNext();
            return "";
        }
#endif
        else if constexpr (std::is_array_v<U>)
        {
            if (parser.currentState() != Parser::JSON::EnteringArray) return "Expecting JSON array";
            parser.parseNext();

            std::decay_t<decltype(*t)> V;
            size_t i = 0, size = sizeof(t) / sizeof(V);

            for (size_t j = 0; j < size; j++) t[j] = decltype(V){}; // Clear array, since we can't be sure we'll find as many value as there were in the array

            while (true)
            {
                if (parser.currentState() == Parser::JSON::LeavingArray) break;

                RWString ret = deserializeFromJSON(parser, V);
                if (ret) return ret;

                if (i < size)
                    t[i] = V;
                else
                    return RWString::format("Array size (%d) too small", (int)size);
                i++;
            }
            parser.parseNext();
            return "";
        }
        else if constexpr (std::is_aggregate_v<T>)
        {
            if (parser.currentState() != Parser::JSON::EnteringObject) return "Expecting JSON object";
            // Remove object bracket
            const auto& members = Refl::Members::get_member_functors<T>(0);
            parser.parseNext();
            RWString err;
            while (true)
            {
                if (parser.currentState() == Parser::JSON::LeavingObject) break;
                ROString key = parser.nextObjectKey();
                if (!key) return "Expecting object key";
                // Find the member with the given name and deserialize it
                bool ret = deserializeField(parser, err, key, t, members);
                if (!ret && allowPartial)
                    // Key not found in the given partial object, so we don't have a schema to continue parsing, let's give up.
                    // TODO: add an ignore value function to the parser to continue parsing nonetheless.
                    return "";

                if (err) return err;
            }
            parser.parseNext();
            return "";
        }
        else
        {
            static_assert(Refl::always_false_v<T>, "Can't deserialize this type from JSON");
            return RWString("Unsupported underlying type for deserialization: ") + Refl::Name::type(t).c_str();
        }
    }

#ifdef AllowSerializing
    // Forward declare the class
    template <typename T>
    RWString serializeToJSONKeyValue(const ROString & key, const T & t);

    // Compile time visitor pattern for a aggregate converted to a tuple of reflected members
    template <typename T, typename ... Members>
    void serializeToJSONMembers(RWString & str, const T & instance, std::tuple<Members...> const & tup)
    {
        std::apply(
            [&str, &instance](Members const &... args)
            {
                ((str += serializeToJSONKeyValue(args.name(), args.get(instance)) + ","), ...);
            }, tup);
    }

    template <typename U>
    RWString serializeBasicType(const U & t)
    {
        using T = std::decay_t<U>;
        if constexpr (std::is_enum_v<T>)
            return RWString::format("\"%s\"",Refl::enum_value_name(t));
        else if constexpr (std::is_same_v<T, bool>)
            return RWString(t ? "true" : "false");
        else if constexpr (std::is_arithmetic_v<T>)
            return RWString::format("%g", (double)t);
        else if constexpr (std::is_convertible_v<T, const char *> || std::is_same_v<T, RWString>)
            return RWString::format("\"%s\"", (const char*)t);
        else if constexpr (std::is_same_v<T, std::string_view> || std::is_same_v<T, std::string>)
            return RWString::format("\"%.*s\"", t.length(), t.data());
        else if constexpr (std::is_same_v<T, ROString>)
            return RWString::format("\"%.*s\"", t.getLength(), t.getData());
        return "";
    }

    /** Serialize an aggregate object (or JSON basic type) to a JSON string.
        This doesn't handle array yet */
    template <typename U>
    RWString serializeToJSONKeyValue(const ROString & key, const U & t)
    {
        using T = std::decay_t<U>;

        RWString res = key ? RWString::format("\"%.*s\":", key.getLength(), key.getData()) : "";
        if constexpr (isBasicType<T>())
            return res + serializeBasicType(t);
        else if constexpr (is_std_container_v<T> || std::is_array_v<U>)
        {
            // Need to create an JSON array here
            res += '[';
            for (auto const & elem : t)
                res += serializeToJSONKeyValue("", elem) + ",";
            if (res[res.getLength() - 1] == ',')
                res[res.getLength() - 1] = ']';
            else res += ']';
            return res;
        }
        else if constexpr (std::is_aggregate_v<T>)
        {
            const auto& members = Refl::Members::get_member_functors<T>(0);
            res += '{'; serializeToJSONMembers(res, t, members);
            if (res[res.getLength() - 1] == ',')
                res[res.getLength() - 1] = '}';
            else res += '}';
            return res;
        }
        else
        {
            static_assert(Refl::always_false_v<T>, "Can't serialize this type to JSON");
            return "";
        }
    };

#endif

}

/** The simple deserializer class that's only dealing with basic aggregate types.
    An aggregate type is a structure with members without a non default constructor.
    Expected here is plain basic types, like those supported by JSON (int, float, double, RWString, fixed size array)

    The system use some kind of magic to automatically find out the member name and its type and deserialize from them.
    You don't need to write any serialization or deserialization code for it, it's reflected automatically (thanks C++20)

    @param obj          The object to deserialize into, the expected JSON schema is deduced from this object by reflection
    @param json         The JSON read only text
    @param allowPartial If true, the parsing stop without error on the first key that's not in the given object.
                        This allows to deserialize polymorphic object by deserializing first a common type to figure out
                        what to deserialize next. Beware however that since the schema can't be deduced from the object, if the
                        JSON text doesn't contain the required keys first, you won't get any useful output from this.
 */
template <class T>
bool deserialize(T & obj, const ROString & json, const bool allowPartial = false)
{
    Parser parser(json);
    if (parser.currentState() != Parser::JSON::EnteringObject) return false;
    RWString err = Details::deserializeFromJSON(parser, obj, allowPartial);
    if (err) return parser.Error(0, err);
    return true;
}

/** The simple deserializer function that's dealing with arrays.
    @warning A polymorphic array isn't supported, all array element must be the same type
    @sa deserialize */
template <class T, size_t N>
bool deserialize(T (&obj)[N], const ROString & json)
{
    Parser parser(json);
    if (parser.currentState() != Parser::JSON::EnteringArray) return false;
    RWString err = Details::deserializeFromJSON(parser, obj);
    if (err) return parser.Error(0, err);
    return true;
}

#ifdef AllowSerializing
/** Serialize the given object to a JSON valid string. */
template <class T>
RWString serialize(T & obj)
{
    return Details::serializeToJSONKeyValue("", obj);
}
#endif


#endif
