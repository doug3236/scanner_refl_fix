#ifndef VALIDATION_H
#define VALIDATION_H

#include <stdexcept>
#include <string>


// if b not true, throw with message
inline void validate(bool b, const std::string& message)
{
    if (!b)
        throw std::runtime_error(message.c_str());
}

// parses filename, ie: foo.h returns pair {"foo", ".h"}
inline std::pair<std::string, std::string> file_parts(const std::string& name)
{
    auto loc=name.find_last_of('.');
    return std::make_pair(name.substr(0,loc), name.substr(loc));
}

// test file type
inline bool file_is_tif(const std::string& name)
{
    return file_parts(name).second == (".tif");
}

// test file type
inline bool file_is_txt(const std::string& name)
{
    return file_parts(name).second == (".txt");
}
#endif
