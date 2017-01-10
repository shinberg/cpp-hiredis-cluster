#include <iostream>
#include <event2/event.h>
#include <signal.h>

#include "adapters/libeventadapter.h"  // for LibeventAdapter
#include "asynchirediscommand.h"

using RedisCluster::AsyncHiredisCommand;
using RedisCluster::Cluster;

using std::string;
using std::out_of_range;
using std::cerr;
using std::cout;
using std::endl;

typedef typename Cluster<redisAsyncContext>::ptr_t ClusterPtr;

static void setCallback( ClusterPtr cluster_p,
    const redisReply &reply, const string &demoStr )
{
    if( reply.type == REDIS_REPLY_STATUS  || reply.type == REDIS_REPLY_ERROR )
    {
        cout << " Reply to SET FOO BAR " << endl;
        cout << reply.str << endl;
    }
    
    cout << demoStr << endl;
    // cluster disconnect must be invoked, instead of redisAsyncDisconnect
    // this will brake event loop
    cluster_p->disconnect();
}

void processAsyncCommand()
{
    ClusterPtr cluster_p;
    
    signal(SIGPIPE, SIG_IGN);
    struct event_base *base = event_base_new();
    RedisCluster::LibeventAdapter adapter( *base );
    string demoData("Demo data is ok");
    
    cluster_p = AsyncHiredisCommand<>::createCluster( "127.0.0.1", 7000, adapter );
    
    AsyncHiredisCommand<>::Command( cluster_p,
                                 "FOO",
                                 [cluster_p, demoData](const redisReply &reply) {
                                    setCallback(cluster_p, reply, demoData);
                                 },
                                 "SET %s %s",
                                 "FOO",
                                 "BAR1");
    
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

