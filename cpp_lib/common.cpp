#include "common.h"

int vscprintf(const char *format, ...)
{
    va_list marker;
    va_start(marker, format);
    int num = vsnprintf(0, 0, format, marker);
    va_end(marker);
    return num;
}

void std_string_format(std::string & _str, int num_of_chars, const char * format, ...)
{
    std::shared_ptr<char> tmp_buffer(new char[num_of_chars], delete_ptr<char>());
    va_list marker;
    va_start(marker, format);
    int ret = vsnprintf(tmp_buffer.get(), num_of_chars, format, marker);
    va_end(marker);
    _str = std::string(tmp_buffer.get(), ret);
}

