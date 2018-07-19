#include "common.h"

int vscprintf(const char *format, ...)
{
    va_list marker;
    va_start(marker, format);
    int num = vsnprintf(0, 0, format, marker);
    va_end(marker);
    return num;
}

std::string std_string_format(int num_of_chars, const char * format, ...)
{
    std::shared_ptr<char> tmp_buffer(new char[num_of_chars], delete_ptr<char>());
    va_list marker;
    va_start(marker, format);
    int ret = vsnprintf(tmp_buffer.get(), num_of_chars, format, marker);
    va_end(marker);
    return std::string(tmp_buffer.get(), ret);
}

std::tuple<std::string, int> pasrse_dns(std::string dns)
{
    auto found = dns.find(":");
    if (found != std::string::npos)
    {
        std::string host = dns.substr(0, found);
        if (dns.length() == found + 1)
            return std::make_tuple(host, 80);
        if (!isdigit(dns[found + 1]))
            return std::make_tuple(host, 80);
        int port = stoi(dns.substr(found + 1));
        return std::make_tuple(host, port);
    }
    return std::make_tuple(dns, 80);
}

std::tuple<std::string, std::string, int> pasrse_url(std::string url)
{
    if (url.length() == 0) return std::make_tuple("", "", 0);

    std::string host;
    std::string uri = "/";
    int port;

    std::size_t found = url.find("://");
    if (found == std::string::npos)
    {
        found = url.find("/");
        if (found != std::string::npos)
        {
            uri = url.substr(found);
            url = url.substr(0, found);
        }
        std::tie(host, port) = pasrse_dns(url);
        return std::make_tuple(host, uri, port);
    }
    else
    {
        if (url.length() == found + 3) return std::make_tuple("", "", 0);
        url = url.substr(found + 3);
        return pasrse_url(url);
    }
}


int main()
{
    std::string test = string_format("name: %s year: %d\n", "gaoyaming", 20);
    printf("test: %s", test.c_str());
    return 0;
}

