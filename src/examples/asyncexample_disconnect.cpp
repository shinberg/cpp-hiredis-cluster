#include <iostream>
#include <event2/event.h>
#include <signal.h>
#include <unistd.h>

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
    
    if (!reply) {
        cout << " Empty reply..." << endl;
        return;
    }
    
    if( reply->type == REDIS_REPLY_STATUS  || reply->type == REDIS_REPLY_ERROR )
    {
        cout << " Reply to SET FOO BAR " << endl;
        cout << reply->str << endl;
    }
}

void processAsyncCommand()
{
    Cluster<redisAsyncContext>::ptr_t cluster_p;
    
    signal(SIGPIPE, SIG_IGN);
    struct event_base *base = event_base_new();
    
    cluster_p = AsyncHiredisCommand<>::createCluster( "127.0.0.1", 7000, static_cast<void*>( base ) );
    
    try {
        
        int lastsecond;
        for (int i=0 ; i<100 ; )
        {
            if (lastsecond != time(NULL)) {
                lastsecond = time(NULL);
                ++i;
                
                cout << ">>> Loop iteration: " << i << endl;
                cout << ">>> Stop or kill redis cluster to emulate cluster disconnection" << endl;                
        
                AsyncHiredisCommand<>::Command( cluster_p,
                                             "FOO",
                                             setCallback,
                                             nullptr,
                                             "SET %s %s",
                                             "FOO",
                                             "BAR1" );
            }
                        
            event_base_loop(base, EVLOOP_NONBLOCK | EVLOOP_ONCE);                        
            usleep(1000);
        }        
    } catch ( const RedisCluster::ClusterException &e )
    {
        cout << "Cluster exception: " << e.what() << endl;        
    }
    
    cluster_p->disconnect();
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

