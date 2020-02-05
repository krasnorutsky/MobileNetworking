//
//  SSLContext.hpp
//  httpclient
//
//  Created by Mikhail Krasnorutsky on 11.10.2018.
//

#ifndef SSLContext_hpp
#define SSLContext_hpp

#include <boost/asio/ssl/stream.hpp>

namespace networking
{
    class SSLContext
    {
        boost::asio::ssl::context ctx;
        
        SSLContext();
    public:
        
        static boost::asio::ssl::context& context();
    };
}

#endif /* SSLContext_hpp */
