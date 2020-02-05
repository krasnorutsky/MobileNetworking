#ifndef URL_H_
#define URL_H_

#include <string>

class Url
{
    std::string protocol_, host_, path_, query_, url_, fileName_;
    
    void parse(const std::string& url);
public:
    Url(){}
    Url(const std::string& url);
    
    std::string originalUrlString() const
    {
        return url_;
    }

    std::string protocol() const
    {
        return protocol_;
    }
    
    std::string host() const
    {
        return host_;
    }
    
    std::string pathAndQuery() const
    {
        if (query_.empty())
        {
            return path_;
        }
        else
        {
            return path_ + "?" + query_;
        }
    }
    
    std::string path() const
    {
        return path_;
    }
    
    std::string query() const
    {
        return query_;
    }
    
    std::string fileName() const
    {
        return fileName_;
    }
};

#endif
