#include <iostream>
#include <event2/event.h>
#include <signal.h>

#include "asynchirediscommand.h"

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
    
    signal(SIGPIPE, SIG_IGN);
    struct event_base *base = event_base_new();
    string *demoData = new string("Demo data is ok");
    
    cluster_p = AsyncHiredisCommand<>::createCluster( "192.168.33.10", 7000, static_cast<void*>( base ) );
    
    AsyncHiredisCommand<>::Command( cluster_p,
                                 "FOO",
                                 setCallback,
                                 static_cast<void*>( demoData ),
                                 "SET %s %s",
                                 "FOO",
                                 "BAR1" );
    
    event_base_dispatch(base);
    delete cluster_p;
    event_base_free(base);
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

