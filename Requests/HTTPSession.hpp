//
//  HTTPSession.hpp
//  httpclient
//
//  Created by Mikhail Krasnorutsky on 11.10.2018.
//

#ifndef HTTPSession_hpp
#define HTTPSession_hpp

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/deadline_timer.hpp>
#include "SSLContext.hpp"
#include <boost/asio/connect.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <iostream>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>

#include <boost/beast/core/buffered_read_stream.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/version.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "Url.hpp"
#include "NetworkingEventLoop.hpp"
#include "RetryPolicy.hpp"

#define HTTP_SESSION_NO_RETRY -1

namespace networking
{
    using namespace std;
    using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
    namespace ssl = boost::asio::ssl;
    namespace http = boost::beast::http;    // from <boost/beast/http.hpp>
    namespace errc = boost::system::errc;
    namespace net = boost::asio;

    class HTTPSession
    {        
        typedef boost::system::error_code http_error;
        typedef boost::asio::deadline_timer net_timer;
        
        boost::asio::io_context& ioc;
        Url originalUrl_;
        Url url_;
        boost::asio::ip::tcp::resolver resolver_;
        boost::beast::http::request<boost::beast::http::string_body> req_;
        boost::beast::http::response_parser<boost::beast::http::string_body> res_;
        
        typedef function<void (int,string,int,boost::beast::http::response_parser<boost::beast::http::string_body>&)> http_session_callback;
        
        http_session_callback callback_;
        
        unique_ptr<tcp::socket> httpSocket_;
        unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> httpsSocket_;
        
    public:
        explicit HTTPSession(boost::asio::io_context& ioc_) :
            ioc(ioc_) ,
            resolver_(ioc)
        {
            res_.body_limit(numeric_limits<uint64_t>::max());
        }
        
        static shared_ptr<HTTPSession> runRequest (Url url,
                                                   http::request<http::string_body> request,
                                                   http_session_callback callback,
                                                   boost::asio::io_context& ioc = NetworkingEventLoop::context(),
                                                   shared_ptr<RetryPolicy> retryPolicy = RetryPolicy::create())
        {
            auto session = make_shared<HTTPSession>(ioc);
            
            session->callback_ = callback;
            session->url_ = url;
            session->originalUrl_ = url;
            session->req_ = request;
            
            boost::asio::spawn(session->ioc, [session,retryPolicy] (boost::asio::yield_context ctx)
                               {
                                   run(session,
                                       ctx,
                                       retryPolicy);
                               });
            
            return session;
        }
        
        static shared_ptr<HTTPSession> runRequest (Url url,
                                                   http::request<http::string_body> request,
                                                   http_session_callback callback,
                                                   const boost::asio::yield_context& ctx,
                                                   boost::asio::io_context& ioc = NetworkingEventLoop::context(),
                                                   shared_ptr<RetryPolicy> retryPolicy = RetryPolicy::create())
        {
            auto session = make_shared<HTTPSession>(ioc);
            
            session->callback_ = callback;
            session->url_ = url;
            session->originalUrl_ = url;
            session->req_ = request;
            
            run(session,
                ctx,
                retryPolicy);
            
            return session;
        }
        
    private:
        static void run(shared_ptr<HTTPSession> session,
                        const boost::asio::yield_context& ctx,
                        std::shared_ptr<RetryPolicy> retryPolicy)
        {
            boost::beast::flat_buffer buffer;
            
            for(;;)
            {
                http_error ec;
                int httpCode = 0;
                
                for(;;)
                {
                    httpCode = 0;
                    bool is_https = session->url_.protocol()=="https";
                    SinglePassResult result = SinglePassResult_complete;
                    
                    if (is_https)
                    {
                        if (!session->httpsSocket_)
                        {
                            session->httpsSocket_ = make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(session->ioc, SSLContext::context());
                            if (!SSL_set_tlsext_host_name(session->httpsSocket_->native_handle(), session->url_.host().c_str()))
                            {
                                ec = http_error{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
                            }
                        }
                        
                        result = session->single_pass(*(session->httpsSocket_),
                                                      is_https,
                                                      buffer,
                                                      ctx,
                                                      ec,
                                                      httpCode);
                    }
                    else
                    {
                        if (!session->httpSocket_)
                        {
                            session->httpSocket_ = make_unique<tcp::socket>(session->ioc);
                        }
                        
                        result = session->single_pass(*(session->httpSocket_),
                                             is_https,
                                             buffer,
                                             ctx,
                                             ec,
                                             httpCode);
                    }
                    
                    if (result == SinglePassResult_complete)
                    {
                        return session->shutdown(ctx);
                    }
                    else if (result == SinglePassResult_error)
                    {
                        break;
                    }
                }
                
                int secondsToWait = retryPolicy->secondsToWaitForRetry(ec.value(),httpCode);
                if (secondsToWait < 0/* HTTP_SESSION_NO_RETRY */)
                {
                    session->callback_(ec.value(),
                                       ec.message(),
                                       httpCode,
                                       session->res_);
                    
                    return session->shutdown(ctx);
                }
                
                net_timer timer(session->ioc);
                timer.expires_from_now(boost::posix_time::seconds(secondsToWait));
                timer.async_wait(ctx[ec]);
            }
        }
        
        enum SinglePassResult
        {
            SinglePassResult_complete,
            SinglePassResult_error,
            SinglePassResult_redirect,
        };
        
        template<class T>
        SinglePassResult single_pass(T& socket,
                                     bool is_https,
                                     boost::beast::flat_buffer& buffer,
                                     const boost::asio::yield_context& ctx,
                                     http_error& ec,
                                     int& httpCode)
        {
            auto results = resolver_.async_resolve(url_.host(),
                                                   is_https ? "443" : "80", //Port
                                                   ctx[ec]);
            
            if (ec)
            {
                return SinglePassResult_error;
            }
            
            auto& connection_target = is_https ? httpsSocket_->next_layer() : *httpSocket_;
            boost::asio::async_connect(connection_target,
                                       results.begin(),
                                       results.end(),
                                       ctx[ec]);
            
            if (ec)
            {
                return SinglePassResult_error;
            }
            
            if (is_https)
            {
                httpsSocket_->async_handshake(ssl::stream_base::client,
                                              ctx[ec]);
                
                if (ec)
                {
                    std::string dscc = ec.message();
                    
                    return SinglePassResult_error;
                }
            }
            
            http::async_write(socket,
                              req_,
                              ctx[ec]);
            
            if (ec)
            {
                return SinglePassResult_error;
            }
            
            http::async_read(socket,
                             buffer,
                             res_,
                             ctx[ec]);
            
            if (ec)
            {
                return SinglePassResult_error;
            }
            
            httpCode = res_.get().result_int();
            if ((httpCode == 301 ||
                 httpCode == 302) &&
                 res_.get().find("Location") != res_.get().end())
            {
                string redirectedTo = res_.get()["Location"].to_string();
                url_ = Url(redirectedTo);
                
                return SinglePassResult_redirect;
            }
            
            if ((httpCode/100) != 2)
            {
                return SinglePassResult_error;
            }
            
            callback_(0,
                      "",
                      200,
                      res_);
            
            return SinglePassResult_complete;
        }

        void shutdown(const boost::asio::yield_context& ctx)
        {
            boost::system::error_code ec;
            
            if (httpSocket_)
            {
                httpSocket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            
                // not_connected happens sometimes so don't bother reporting it.
                if(ec &&
                   ec != boost::system::errc::not_connected)
                {
                    cerr << "http socket shutdown" << ": " << ec.message() << "\n";
                }
            }
            
            if (httpsSocket_)
            {
                httpsSocket_->async_shutdown(ctx[ec]);
            
                if(ec == boost::asio::error::eof)
                {
                    // Rationale:
                    // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
                    ec.assign(0, ec.category());
                }
            
                if(ec)
                {
                    // "https socket shutdown: short read"
                    //cerr << "https socket shutdown" << ": " << ec.message() << "\n";
                }
            }
        }
    };
}

#endif /* HTTPSession_hpp */
