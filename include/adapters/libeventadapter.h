// Author: Jin Qing (http://blog.csdn.net/jq0123)

#ifndef __libredisCluster_adapters_libeventadapter_h__
#define __libredisCluster_adapters_libeventadapter_h__

#include <cassert>  // for assert()

#include "adapter.h"  // for Adapter

extern "C"
{
#include <hiredis/adapters/libevent.h>  // for redisLibeventAttach()
}

namespace RedisCluster
{
    // Wrap hiredis libevent adapter.
    class LibeventAdapter : public Adapter
    {
    public:
        explicit LibeventAdapter( struct event_base & base ) : base_( base ) {}
        virtual ~LibeventAdapter() {}
    
    public:
        virtual int attachContext( redisAsyncContext &ac ) override
        {
            return redisLibeventAttach( &ac, &base_ );
        }
    
    private:
        struct event_base & base_;
    };  // class Adapter
}  // namespace RedisCluster

#endif  // __libredisCluster_adapters_libeventadapter_h__
