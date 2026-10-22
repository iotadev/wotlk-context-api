#include "ContextJson.h"
#include <cassert>
#include <iostream>
#include <limits>

struct CommaLocale : std::numpunct<char>
{
    char do_decimal_point() const override { return ','; }
};

int main()
{
    std::locale::global(std::locale(std::locale::classic(), new CommaLocale));
    WotlkContext::JsonWriter writer;
    writer.StartObject();
    writer.Key("text"); writer.String("quote\" slash\\ newline\n\x01 UTF8 \xc3\xa9");
    writer.Key("array"); writer.StartArray(); writer.Null(); writer.Bool(true);
    writer.Uint64(std::numeric_limits<uint64_t>::max()); writer.Double(1.25);
    writer.StartObject(); writer.EndObject(); writer.EndArray(); writer.EndObject();
    assert(writer.Finish() == "{\"text\":\"quote\\\" slash\\\\ newline\\u000a\\u0001 UTF8 \xc3\xa9\",\"array\":[null,true,18446744073709551615,1.25,{}]}");
    std::cout << writer.Finish() << '\n';
    bool rejected = false;
    try
    {
        WotlkContext::JsonWriter invalid;
        invalid.Double(std::numeric_limits<double>::quiet_NaN());
    }
    catch (std::runtime_error const&) { rejected = true; }
    assert(rejected);
    rejected = false;
    try
    {
        WotlkContext::JsonWriter invalid;
        invalid.StartObject(); invalid.Key("missing"); invalid.EndObject();
    }
    catch (std::logic_error const&) { rejected = true; }
    assert(rejected);
}
