//
//  DownloadSession.hpp
//  httpclient
//
//  Created by Mikhail Krasnorutsky on 11.10.2018.
//

#ifndef DownloadSession_hpp
#define DownloadSession_hpp

#include "HTTPSession.hpp"
#include "Url.hpp"
#include <mutex>
#include <fstream>

namespace networking
{
    using namespace std;
    using tcp = boost::asio::ip::tcp;
    namespace ssl = boost::asio::ssl;
    
    class DownloadSession : public std::enable_shared_from_this<DownloadSession>
    {
        boost::asio::io_context& ioc;
        
        Url url;
        std::string filePath;
        std::shared_ptr<std::ofstream> file;
        std::string directoryForFile;
        long chunkSize_{1000000};
        long fileSize_{0};
        long lastChunkSize_{0};
        int chunksDownloaded{0};
        int chunksCount{0};
        bool errorOccured_{false};
        
        static boost::beast::http::request<boost::beast::http::string_body> createRequest(const Url& urlComponents,
                                                             long downloadStart=-1,
                                                             long downloadSize=-1);
        
        static boost::beast::http::request<boost::beast::http::string_body> createRequestForObtainingSize(const Url& urlComponents,
                                                                             long downloadStart=-1,
                                                                             long downloadSize=-1);
        
        boost::beast::http::request<boost::beast::http::string_body> createRequestForChunk(const Url& urlComponents,
                                                              size_t i);
        
        std::function<void (double)> progressCallback_;
        std::function<void (int,//error code
                            std::string,//error description
                            int,//httpCode
                            std::string)> callback_;
    public:
        DownloadSession(boost::asio::io_context& ioc_) : ioc(ioc_){}
        
        static void download(std::string urlString,
                                                         std::string dir,
                                                         std::function<void (int,//error code
                                                                             std::string,//error description
                                                                             int,
                                                                             std::string)> callback,
                                                         std::function<void (double)> progressCallback,
                                                         long fileSize = -1,
                                                         boost::asio::io_context& ioc = NetworkingEventLoop::context());
        
        static std::shared_ptr<DownloadSession> download(std::string urlString,
                                                         std::string dir,
                                                         std::function<void (int,//error code
                                                                               std::string,//error description
                                                                               int,
                                                                               std::string)> callback,
                                                         std::function<void (double)> progressCallback,
                                                         const boost::asio::yield_context& ctx,
                                                         long theFileSize,
                                                         boost::asio::io_context& ioc = NetworkingEventLoop::context());
        
        long getFileSize(std::string urlString,
                         int& code_,
                          std::string& description_,
                          int& httpCode_,
                          const boost::asio::yield_context& ctx);
        
        void startDownload(long fileSize);
        void downloadPart(int part);
        void onDownloadCompleteForPart(int part);
        void writeFileFragment(int i, std::string& body);
        void onError(int code, std::string description, int httpCode);
    };
}

#endif /* DownloadSession_hpp */
