//
//  RetryPolicy.hpp
//
//  Created by Mikhail Krasnorutsky on 22.11.2018.
//

#ifndef RetryPolicy_hpp
#define RetryPolicy_hpp

#include <memory>
#include <vector>

namespace networking
{
    enum RetryPolicyType
    {
        RetryPolicyType_Default     = 0,
        RetryPolicyType_ChatMessage = 1,
        RetryPolicyType_Download    = 2,
    };
    
    class RetryPolicy
    {
    public:
        static std::shared_ptr<RetryPolicy> create(RetryPolicyType type = RetryPolicyType_Default);
        virtual int secondsToWaitForRetry(int errorCode, int httpCode) = 0;
    };
}

#endif /* RetryPolicy_hpp */
