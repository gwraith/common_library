#ifndef __COMMON_HEADER_H__
#define __COMMON_HEADER_H__
#include <string>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <memory>

/* string.format */
template <typename T>
struct delete_ptr
{
    void operator()(T* p)
    {
        delete[] p;
    }
};

int vscprintf(const char *format, ...);
std::string std_string_format(int num_of_chars, const char * format, ...);

template <typename... U>
std::string string_format(U&&... u)
{
    return std_string_format(vscprintf(std::forward<U>(u)...) + 1, std::forward<U>(u)...);
}

/* url_pasrse */
std::tuple<std::string, int> pasrse_dns(std::string dns);
std::tuple<std::string, std::string, int> pasrse_url(std::string url);

#endif
