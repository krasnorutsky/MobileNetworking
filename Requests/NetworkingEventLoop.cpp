//
//  NetworkingEventLoop.cpp
//  httpclient
//
//  Created by Mikhail Krasnorutsky on 11.10.2018.
//

#include "NetworkingEventLoop.hpp"
#include <thread>

using namespace networking;
using namespace std;
using namespace boost::asio;

NetworkingEventLoop::NetworkingEventLoop() : work(ioc)
{
    thread { [&]()
        {
            ioc.run();
        }
    }.detach();
}

io_context& NetworkingEventLoop::context()
{
    static NetworkingEventLoop loop;
    return loop.ioc;
}
