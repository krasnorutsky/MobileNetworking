//
//  SSLContext.cpp
//  httpclient
//
//  Created by Mikhail Krasnorutsky on 11.10.2018.
//

#include "SSLContext.hpp"
#include "root_certificates.hpp"

using namespace networking;
namespace ssl = boost::asio::ssl;

SSLContext::SSLContext() : ctx{ssl::context::sslv23_client}
{
    //SSL_set_tlsext_host_name(ctx.native_handle(),"adcel.co");
    /*
    ctx.set_options(boost::asio::ssl::context::default_workarounds |
                    boost::asio::ssl::context::no_sslv2 |
                    boost::asio::ssl::context::no_sslv3);
    */
    load_root_certificates(ctx);
}

ssl::context& SSLContext::context()
{
    static SSLContext context;
    return context.ctx;
}
