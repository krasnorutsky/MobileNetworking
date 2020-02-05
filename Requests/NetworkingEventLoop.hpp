//
//  NetworkingEventLoop.hpp
//  httpclient
//
//  Created by Mikhail Krasnorutsky on 11.10.2018.
//

#ifndef NetworkingEventLoop_hpp
#define NetworkingEventLoop_hpp

#include <boost/asio/io_service.hpp>

namespace networking
{
    class NetworkingEventLoop
    {
        boost::asio::io_context ioc;
        boost::asio::io_service::work work;
        
        NetworkingEventLoop();
    public:
        
        static boost::asio::io_context& context();
    };
}

#endif /* NetworkingEventLoop_hpp */
