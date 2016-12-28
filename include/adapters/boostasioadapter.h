// Author: Jin Qing (http://blog.csdn.net/jq0123)

#ifndef __libredisCluster_adapters_boostasioadapter_h__
#define __libredisCluster_adapters_boostasioadapter_h__

#include <vector>

#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include "adapter.h"  // for Adapter
#include "hiredis-boostasio-adapter/boostasio.hpp"  // for redisBoostClient

namespace boost
{
    namespace asio
    {
        class io_service;
    }
}  // namespace boost

namespace RedisCluster
{
    // Wrap boost asio adapter.
    // https://github.com/ryangraham/hiredis-boostasio-adapter
    class BoostAsioAdapter : public Adapter
    {
    public:
        BoostAsioAdapter( boost::asio::io_service & ios ) : io_service_( ios ) {}
        virtual ~BoostAsioAdapter() {}
    
    public:
        // Returns REDIS_OK on success.
        virtual int attachContext( redisAsyncContext & ac )
        {
            ClientSptr sptr = boost::make_shared<redisBoostClient>(
                io_service_, &ac );
            client_vector_.push_back( sptr );
            return REDIS_OK;
        }

    private:
        boost::asio::io_service & io_service_;

        typedef boost::shared_ptr<redisBoostClient> ClientSptr;
        typedef std::vector<ClientSptr> ClientVector;
        ClientVector client_vector_;
    };  // class BoostAsioAdapter
}  // namespace RedisCluster

#endif  // __libredisCluster_adapters_boostasioadapter_h__
