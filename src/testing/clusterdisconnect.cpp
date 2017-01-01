#include <iostream>
#include <event2/event.h>
#include <signal.h>
#include <unistd.h>

#include "asynchirediscommand.h"
#include <adapters/libeventadapter.h>

using RedisCluster::AsyncHiredisCommand;
using RedisCluster::Cluster;
using RedisCluster::LibeventAdapter;

using std::string;
using std::out_of_range;
using std::cerr;
using std::cout;
using std::endl;

static void setCallback( typename Cluster<redisAsyncContext>::ptr_t cluster_p,
    const redisReply &reply, const string &demoStr )
{
    if( reply.type == REDIS_REPLY_STATUS || reply.type == REDIS_REPLY_ERROR )
    {
        cout << " Reply to SET FOO BAR " << endl;
        cout << reply.str << endl;
    }
    
    cout << demoStr << endl;
    // cluster disconnect must be invoked, instead of redisAsyncDisconnect
    // this will brake event loop
    //cluster_p->disconnect();
}

void processAsyncCommand()
{
    Cluster<redisAsyncContext>::ptr_t cluster_p;
    
    signal(SIGPIPE, SIG_IGN);
    struct event_base *base = event_base_new();

    LibeventAdapter adapter(*base);
    cluster_p = AsyncHiredisCommand<>::createCluster( "192.168.33.10", 7000, adapter);
    
    while (true) {
        string demoStr("Demo data is ok");
        AsyncHiredisCommand<>::Command( cluster_p,
                                           "FOO",
                                           [cluster_p, demoStr](const redisReply& reply) {
                                                setCallback(cluster_p, reply, demoStr);
                                           },
                                           "SET %s %s",
                                            "FOO",
                                           "BAR1" );
            
        event_base_loop(base, EVLOOP_NONBLOCK | EVLOOP_ONCE);
        usleep(1000);
    }
    
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

