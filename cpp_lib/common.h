#ifndef __COMMON_HEADER_H__
#define __COMMON_HEADER_H__
#include <string>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <memory>

template <typename T>
struct delete_ptr
{
    void operator()(T* p)
    {
        delete[] p;
    }
};

int vscprintf(const char *format, ...);
void std_string_format(std::string & _str, int num_of_chars, const char * format, ...);

template <typename... U>
void string_format(std::string &str, U&&... u)
{
    return std_string_format(str, vscprintf(std::forward<U>(u)...) + 1, std::forward<U>(u)...);
}

std::tuple<std::string, int> pasrse_dns(std::string dns);
std::tuple<std::string, std::string, int> pasrse_url(std::string url);

#endif
