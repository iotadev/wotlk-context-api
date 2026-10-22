/* GPL v2 or later. Fixed-schema JSON output without a runtime dependency. */
#ifndef GAME_CONTEXT_JSON_H
#define GAME_CONTEXT_JSON_H

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace WotlkContext
{
class JsonWriter
{
public:
    JsonWriter() { _out.imbue(std::locale::classic()); }

    void StartObject() { Value(); _out << '{'; _stack.push_back({false, true, false}); }
    void EndObject() { End(false, '}'); }
    void StartArray() { Value(); _out << '['; _stack.push_back({true, true, false}); }
    void EndArray() { End(true, ']'); }

    void Key(char const* key)
    {
        if (_stack.empty() || _stack.back().Array || _stack.back().Pending)
            throw std::logic_error("invalid JSON key state");
        Separator();
        Quote(key, std::strlen(key));
        _out << ':';
        _stack.back().Pending = true;
    }

    void String(char const* value) { String(value, std::strlen(value)); }
    void String(char const* value, size_t size) { Value(); Quote(value, size); }
    void Uint64(uint64_t value) { Value(); _out << value; }
    void Bool(bool value) { Value(); _out << (value ? "true" : "false"); }
    void Null() { Value(); _out << "null"; }

    // Only a statically registered native module serializer may supply this.
    // External text never enters this path; consumers validate the full schema.
    void NativeObject(std::string const& value)
    {
        if (value.size() > 49152 || value.empty() || value.front() != '{' || value.back() != '}')
            throw std::runtime_error("invalid native module object");
        Value();
        _out << value;
    }

    void Double(double value)
    {
        if (!std::isfinite(value))
            throw std::runtime_error("non-finite context value");
        Value();
        _out << std::setprecision(17) << value;
    }

    std::string Finish() const
    {
        if (!_stack.empty() || !_root)
            throw std::logic_error("incomplete JSON document");
        return _out.str();
    }

private:
    struct Frame
    {
        bool Array, First, Pending;
    };

    void Separator()
    {
        if (!_stack.back().First)
            _out << ',';
        _stack.back().First = false;
    }

    void Value()
    {
        if (_stack.empty())
        {
            if (_root)
                throw std::logic_error("multiple JSON roots");
            _root = true;
        }
        else if (_stack.back().Array)
            Separator();
        else
        {
            if (!_stack.back().Pending)
                throw std::logic_error("JSON value lacks a key");
            _stack.back().Pending = false;
        }
    }

    void End(bool array, char delimiter)
    {
        if (_stack.empty() || _stack.back().Array != array || _stack.back().Pending)
            throw std::logic_error("invalid JSON container close");
        _stack.pop_back();
        _out << delimiter;
    }

    void Quote(char const* text, size_t size)
    {
        static char const hex[] = "0123456789abcdef";
        _out << '"';
        for (size_t index = 0; index < size; ++index)
        {
            unsigned char ch = static_cast<unsigned char>(text[index]);
            if (ch == '"' || ch == '\\')
                _out << '\\' << static_cast<char>(ch);
            else if (ch < 0x20)
                _out << "\\u00" << hex[ch >> 4] << hex[ch & 15];
            else
                _out << static_cast<char>(ch);
        }
        _out << '"';
    }

    std::ostringstream _out;
    std::vector<Frame> _stack;
    bool _root = false;
};
}
#endif
