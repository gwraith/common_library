#ifndef __STRING_FORMAT_HPP__
#define __STRING_FORMAT_HPP__

#include <memory>
#include <string>
#include <stdarg.h>

template <typename T>
struct delete_ptr
{
    void operator()(T* p) { delete[] p; }
};

int vscprintf(const char *format, ...);
std::string std_string_format(int num_of_chars, const char * format, ...);

template <typename... Args>
std::string string_format(Args&&... args)
{
    return std_string_format(vscprintf(std::forward<Args>(args)...) + 1, std::forward<Args>(args)...);
}

#endif
