// Author: Hong Jen-Yee (PCMan) (pcman.hong@appier.com)

#ifndef __libredisCluster_adapters_libuvadapter_h__
#define __libredisCluster_adapters_libuvadapter_h__

#include "adapter.h"  // for Adapter

extern "C"
{
#include <hiredis/adapters/libuv.h>  // for redisLibeventAttach()
}

namespace RedisCluster
{
    // Wrap hiredis libuv adapter.
    class LibUvAdapter : public Adapter
    {
    public:
        explicit LibUvAdapter( uv_loop_t* loop = uv_default_loop() ) : loop_( loop ) {}
        virtual ~LibUvAdapter() {}

    public:
        virtual int attachContext( redisAsyncContext &ac ) override
        {
            return redisLibuvAttach( &ac, loop_ );
        }

    private:
        uv_loop_t* loop_;
    };  // class Adapter
}  // namespace RedisCluster

#endif  // __libredisCluster_adapters_libuvadapter_h__
