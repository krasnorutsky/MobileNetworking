//
//  DownloadSession.cpp
//  httpclient
//
//  Created by Mikhail Krasnorutsky on 11.10.2018.
//

#include "DownloadSession.hpp"
#include <iostream>
#include <boost/beast/version.hpp>
#include "NetworkingEventLoop.hpp"

#include <filesystem>
namespace fs = std::filesystem;

#define MAX_ACTIVE_DOWNLOAD_CHUNKS 4

using namespace networking;
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http;    // from <boost/beast/http.hpp>
    
http::request<http::string_body> DownloadSession::createRequest(const Url& urlComponents,
                                                     long downloadStart,
                                                     long downloadSize)
{
    http::request<http::string_body> req;
    
    req.version(11);
    req.method(http::verb::get);
    req.target(urlComponents.path());
    req.set(http::field::host, urlComponents.host());
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    
    if (-1 != downloadStart &&
        -1 != downloadSize)
    {
        if (downloadSize)
        {
            req.set(http::field::range, "bytes=" + std::to_string(downloadStart) + "-" + std::to_string(downloadStart + downloadSize - 1));
        }
    }
    
    return req;
}

http::request<http::string_body> DownloadSession::createRequestForObtainingSize(const Url& urlComponents,
                                                                     long downloadStart,
                                                                     long downloadSize)
{
    return createRequest(urlComponents, 0, 1);
}

http::request<http::string_body> DownloadSession::createRequestForChunk(const Url& urlComponents,
                                                      size_t i)
{
    bool lastChunk = i == (chunksCount-1);
    
    return createRequest(urlComponents,
                         i*chunkSize_,
                         lastChunk ? lastChunkSize_ : chunkSize_);
}

void DownloadSession::download(std::string urlString,
                                                           std::string dir,
                                                           std::function<void (int,//error code
                                                                               std::string,//error description
                                                                               int,
                                                                               std::string)> callback,
                                                           std::function<void (double)> progressCallback,
                                                           long theFileSize,
                                                           boost::asio::io_context& ioc)
{
    std::cout << dir << std::endl;
    
    boost::asio::spawn(ioc,
                       [&ioc,
                        urlString,
                        dir,
                        callback,
                        progressCallback,
                        theFileSize] (boost::asio::yield_context ctx)
                       {
                           download(urlString,
                                    dir,
                                    callback,
                                    progressCallback,
                                    ctx,
                                    theFileSize,
                                    ioc);
                       });
}

std::shared_ptr<DownloadSession> DownloadSession::download(std::string urlString,
                                                 std::string dir,
                                                 std::function<void (int,//error code
                                                                     std::string,//error description
                                                                     int,
                                                                     std::string)> callback,
                                                 std::function<void (double)> progressCallback,
                                                 const boost::asio::yield_context& ctx,
                                                 long theFileSize,
                                                 boost::asio::io_context& ioc)
{
    auto s = std::make_shared<DownloadSession>(ioc);
    
    std::cout << dir << std::endl;
    
    s->directoryForFile = dir;
    s->url = Url(urlString);
    s->callback_ = callback;
    s->progressCallback_ = progressCallback;
    
    if (theFileSize == -1)
    {
        int code = 0;
        std::string description;
        int httpCode = 0;
        
        theFileSize = s->getFileSize(urlString,
                                     code,
                                  description,
                                  httpCode,
                                  ctx);
        
        if ((httpCode/100) != 2)
        {
            s->onError(code,description,httpCode);
            s->callback_(-6666,"",0,"");
            
            return s;
        }
        else if (code)
        {
            s->onError(-3333,"",0);
            s->callback_(-3333,"",0,"");
            
            return s;
        }
    }
    
    if (theFileSize != -1)
    {
        s->startDownload(theFileSize);
    }
    
    return s;
}

long DownloadSession::getFileSize(std::string urlString,
                                  int& code_,
                                  std::string& description_,
                                  int& httpCode_,
                                  const boost::asio::yield_context& ctx)
{
    long fileSize = -1;
    
    Url url(urlString);
    auto request = createRequestForObtainingSize(url);
    HTTPSession::runRequest(url, request, [&](int code,
                                              std::string description,
                                              int httpCode,
                                              http::response_parser<http::string_body>& res)
                            {
                                code_ = code;
                                description_ = description;
                                httpCode_ = httpCode;
                                
                                if ((httpCode/100) != 2)
                                {
                                    return;
                                }
                                
                                std::cout << res.get();
                                
                                std::string range = res.get()[http::field::content_range].to_string();
                                auto j = range.rfind("/");
                                if (j != std::string::npos)
                                {
                                    try
                                    {
                                        fileSize = std::stoi(range.substr(j+1));
                                    }
                                    catch (...)
                                    {
                                        code_ = 6337;
                                        description_ = "Cannot parse file size";
                                    }
                                }
                            },ctx,ioc,RetryPolicy::create(RetryPolicyType_Download));
    
    return fileSize;
}

void DownloadSession::startDownload(long fileSize)
{
    fileSize_ = fileSize;
    lastChunkSize_ = fileSize_ % chunkSize_;
    if (!lastChunkSize_)
    {
        lastChunkSize_ = chunkSize_;
        chunksCount = (int)(fileSize_ / chunkSize_);
    }
    else
    {
        chunksCount = (int)(fileSize_ / chunkSize_ + 1);
    }
    
    fs::path dir(directoryForFile);
    dir /= url.fileName();
    filePath = dir.generic_string();
    
    file = std::make_shared<std::ofstream>(filePath,
                                           std::ios::out |
                                           std::ios::binary |
                                           std::ios::trunc);

    downloadPart(0);
}

void DownloadSession::downloadPart(int part)
{
    int start=0, end=chunksCount;
    
    auto _this = shared_from_this();
    boost::asio::spawn(ioc, [_this,start,end,part] (boost::asio::yield_context ctx)
                       {
                           for (int i=start; i<end; ++i)
                           {
                               auto request = _this->createRequestForChunk(_this->url, i);
                               HTTPSession::runRequest(_this->url, request, [_this,i](int code,
                                                                                       std::string description,
                                                                                       int httpCode,
                                                                                       http::response_parser<http::string_body>& res)
                                                               {
                                                                   if ((httpCode/100) != 2)
                                                                   {
                                                                       return _this->onError(code, description, httpCode);
                                                                   }
                                                                   
                                                                   _this->writeFileFragment(i, res.get().body());
                                                               }, ctx,NetworkingEventLoop::context(), RetryPolicy::create(RetryPolicyType_Download));
                               
                               ++_this->chunksDownloaded;
                               _this->progressCallback_((double)_this->chunksDownloaded/_this->chunksCount);
                           }
                           
                           _this->onDownloadCompleteForPart(part);
                       });
}

void DownloadSession::onDownloadCompleteForPart(int part)
{
    bool complete = true;

    if (complete)
    {
        file->close();

        if (!errorOccured_)
        {
            callback_(0,"",200,filePath);
        }
        else
        {
            //Код стирания файла с диска.
            callback_(-5555,"",0,"");
        }
    }
}

void DownloadSession::writeFileFragment(int i, std::string& body)
{
    file->seekp(i * chunkSize_);
    file->write(body.c_str(), body.size());
}

void DownloadSession::onError(int code, std::string description, int httpCode)
{
    errorOccured_ = true;
}
