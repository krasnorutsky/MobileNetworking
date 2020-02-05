//
//  Url.cpp
//  httpclient
//
//  Created by Mikhail Krasnorutsky on 04.10.2018.
//

#include "Url.hpp"

#include <string>
#include <algorithm>
#include <cctype>
#include <functional>

using namespace std;

void Url::parse(const std::string& url)
{
    auto tmpUrl = url;
    const string prot_end = "://";
    auto i_protocol_end = tmpUrl.find(prot_end);
    
    if (i_protocol_end == string::npos)
    {
        return;
    }
    
    protocol_ = tmpUrl.substr(0, i_protocol_end);
    transform(protocol_.begin(), protocol_.end(), protocol_.begin(), ::tolower);
    tmpUrl = tmpUrl.substr(i_protocol_end + prot_end.size());
    
    const string host_end = "/";
    auto path_i = tmpUrl.find(host_end);
    if (path_i == string::npos)
    {
        return;
    }
    
    host_ = tmpUrl.substr(0, path_i);
    transform(host_.begin(), host_.end(), host_.begin(), ::tolower);
    tmpUrl = tmpUrl.substr(path_i);
    
    const string query_start = "?";
    auto query_i = tmpUrl.find(query_start);
    if (query_i == string::npos)
    {
        path_ = tmpUrl;
    }
    else
    {
        path_ = tmpUrl.substr(0, query_i);
        query_ = tmpUrl.substr(query_i + query_start.size());
    }
}

Url::Url(const std::string& url) : url_(url)
{
    parse(url);
    
    if (!path_.empty())
    {
        auto i = path_.rfind("/");
        if (i != string::npos)
        {
            fileName_ = path_.substr(i+1);
        }
        else
        {
            fileName_ = path_;
        }
    }
}
