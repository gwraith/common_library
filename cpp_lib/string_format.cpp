#include "string_format.h"

int vscprintf(const char *format, ...)
{
    va_list marker;
    va_start(marker, format);
    int num = vsnprintf(0, 0, format, marker);
    va_end(marker);
    return num;
}

//************************************
// 函数名称:  std_string_format
// 函数参数:  int num_of_chars
// 函数参数:  const char * format
// 函数参数:  ...
// 函数返回:  std::string
// 函数说明: 
//************************************
std::string std_string_format(int num_of_chars, const char * format, ...)
{
    std::shared_ptr<char> tmp_buffer(new char[num_of_chars], delete_ptr<char>());
    va_list marker;
    va_start(marker, format);
    int ret = vsnprintf(tmp_buffer.get(), num_of_chars, format, marker);
    va_end(marker);
    return std::string(tmp_buffer.get(), ret);
}

