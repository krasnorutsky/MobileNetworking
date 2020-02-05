//
//  RetryPolicy.cpp
//  TelegramUpdater
//
//  Created by Mikhail Krasnorutsky on 22.11.2018.
//

#include "RetryPolicy.hpp"
#include "HTTPSession.hpp"

using namespace std;
using namespace networking;

class RetryPolicyChatMessage : public RetryPolicy
{
    int attempts_http_{0};
    int attempts_connection_{0};
public:
    virtual int secondsToWaitForRetry(int errorCode, int httpCode) override
    {
        int httpCodeMain = httpCode / 100;
        if (errorCode)
        {
            if (attempts_connection_ >= 2)
            {
                return HTTP_SESSION_NO_RETRY;
            }
            ++attempts_connection_;
            
            return 4;
        }
        else if (httpCodeMain != 2)
        {
            if (4 == httpCodeMain)
            {
                return HTTP_SESSION_NO_RETRY;
            }
            else
            {
                if (attempts_http_ >= 2)
                {
                    return HTTP_SESSION_NO_RETRY;
                }
                ++attempts_http_;
                
                return 8;
            }
        }
        else
        {
            return HTTP_SESSION_NO_RETRY;//No error
        }
    }
};

#define DEFAULT_ATTEMPT_COUNT 2
#define HTTP_ERROR_RETRY_INTERVAL 5
//#define DEFAULT_ATTEMPT_COUNT 10
//#define HTTP_ERROR_RETRY_INTERVAL 30

class RetryPolicyDefault : public RetryPolicy
{
    int attempts_http_{0};
    int attempts_connection_{0};
public:
    virtual int secondsToWaitForRetry(int errorCode, int httpCode) override
    {
        int httpCodeMain = httpCode / 100;
        if (errorCode)
        {
            if (attempts_connection_ >= DEFAULT_ATTEMPT_COUNT)
            {
                return HTTP_SESSION_NO_RETRY;
            }
            ++attempts_connection_;
            
            int retryInterval = 4 * attempts_connection_;
            if (retryInterval > 60)
            {
                retryInterval = 60;
            }
            
            return retryInterval;
        }
        else if (httpCodeMain != 2)
        {
            if (attempts_http_ >= 4)
            {
                return HTTP_SESSION_NO_RETRY;
            }
            ++attempts_http_;
            
            return HTTP_ERROR_RETRY_INTERVAL;
        }
        else
        {
            return HTTP_SESSION_NO_RETRY;//No error
        }
    }
};


class RetryPolicyDownload : public RetryPolicy
{
    int attempts_http_{0};
    int attempts_connection_{0};
public:
    virtual int secondsToWaitForRetry(int errorCode, int httpCode) override
    {
        if (errorCode)
        {
            if (attempts_connection_ >= 1000)
            {
                return HTTP_SESSION_NO_RETRY;
            }
            ++attempts_connection_;
            
            int retryInterval = 4 * attempts_connection_;
            if (retryInterval > 60)
            {
                retryInterval = 60;
            }
            
            return retryInterval;
        }
        else if (httpCode)
        {
            if (attempts_http_ >= 100)
            {
                return HTTP_SESSION_NO_RETRY;
            }
            ++attempts_http_;
            
            return 60;
        }
        else
        {
            return HTTP_SESSION_NO_RETRY;//No error
        }
    }
};

shared_ptr<RetryPolicy> RetryPolicy::create(RetryPolicyType type)
{
    switch(type)
    {
        case RetryPolicyType_Download:
            return make_shared<RetryPolicyDownload>();
            
        case RetryPolicyType_ChatMessage:
            return make_shared<RetryPolicyChatMessage>();
            
        case RetryPolicyType_Default:
            return make_shared<RetryPolicyDefault>();
    }
}

