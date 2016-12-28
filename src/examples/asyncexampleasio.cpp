#include <iostream>
#include <signal.h>

#include "adapters/hiredis-boostasio-adapter/boostasio.hpp"
#include "adapters/boostasioadapter.h"  // for BoostAsioAdapter
#include "asynchirediscommand.h"

#ifdef _WIN32
#define SIGPIPE 13
#endif

using RedisCluster::AsyncHiredisCommand;
using RedisCluster::Cluster;

using std::string;
using std::out_of_range;
using std::cerr;
using std::cout;
using std::endl;

static void setCallback( typename Cluster<redisAsyncContext>::ptr_t cluster_p, void *r, void *data )
{
    redisReply * reply = static_cast<redisReply*>( r );
    string *demoData = static_cast<string*>( data );
    
    if( reply->type == REDIS_REPLY_STATUS  || reply->type == REDIS_REPLY_ERROR )
    {
        cout << " Reply to SET FOO BAR " << endl;
        cout << reply->str << endl;
    }
    
    cout << *demoData << endl;
    delete demoData;
    // cluster disconnect must be invoked, instead of redisAsyncDisconnect
    // this will brake event loop
    cluster_p->disconnect();
}

void processAsyncCommand()
{
    Cluster<redisAsyncContext>::ptr_t cluster_p;
    
    boost::asio::io_service io_service;
    RedisCluster::BoostAsioAdapter adapter( io_service );

    /*loop forever, ever, even if there is no work queued*/
    boost::asio::io_service::work forever(io_service);


    signal(SIGPIPE, SIG_IGN);
    string *demoData = new string("Demo data is ok");
    
    cluster_p = AsyncHiredisCommand<>::createCluster( "127.0.0.1", 7000, adapter );
    
    AsyncHiredisCommand<>::Command( cluster_p,
                                 "FOO",
                                 setCallback,
                                 static_cast<void*>( demoData ),
                                 "SET %s %s",
                                 "FOO",
                                 "BAR1" );
    io_service.run();
    delete cluster_p;
}

int main(int argc, const char * argv[])
{
    try
    {
        processAsyncCommand();
    } catch ( const RedisCluster::ClusterException &e )
    {
        cout << "Cluster exception: " << e.what() << endl;
    }
    return 0;
}

