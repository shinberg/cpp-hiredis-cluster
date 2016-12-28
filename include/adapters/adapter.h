// Author: Jin Qing (http://blog.csdn.net/jq0123)

#ifndef __libredisCluster_adapters_adapter_h__
#define __libredisCluster_adapters_adapter_h__

extern "C"
{
#include <hiredis/hiredis.h>  // for REDIS_ERR
}

struct redisAsyncContext;

namespace RedisCluster
{
    // Wrap hiredis adapters.
    class Adapter
    {
    public:
        Adapter() {}
        virtual ~Adapter() {}
    
    public:
        // Returns REDIS_OK on success.
        virtual int attachContext( redisAsyncContext & )
        {
            return REDIS_ERR;
        }
    };  // class Adapter
}  // namespace RedisCluster

#endif  // __libredisCluster_adapters_adapter_h__
