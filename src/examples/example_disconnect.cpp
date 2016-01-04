#include <iostream>
#include <queue>
#include <thread>
#include <assert.h>
#include <unistd.h>

#include "hirediscommand.h"

using namespace RedisCluster;
using std::string;
using std::cout;
using std::cerr;
using std::endl;

void processClusterCommand()
{
    Cluster<redisContext>::ptr_t cluster_p;
    redisReply * reply;
    
    cluster_p = HiredisCommand<>::createCluster( "127.0.0.1", 7000 );
    
    for (int i=0 ; i<100 ; ++i)
    {
        cout << ">>> Loop iteration: " << i << endl;
        cout << ">>> Stop or kill redis cluster to emulate cluster disconnection" << endl;
        
        reply = static_cast<redisReply*>( HiredisCommand<>::Command( cluster_p, "FOO", "SET %s %s", "FOO", "BAR1" ) );
        
        if( reply->type == REDIS_REPLY_STATUS  || reply->type == REDIS_REPLY_ERROR )
        {
            cout << " Reply to SET FOO BAR " << endl;
            cout << reply->str << endl;
        }
        
        freeReplyObject( reply );
       
        sleep(1);
    }    
    
    cluster_p->disconnect();
    delete cluster_p;
}

int main(int argc, const char * argv[])
{
    try
    {
        processClusterCommand();
    } catch ( const RedisCluster::ClusterException &e )
    {
        cout << "Cluster exception: " << e.what() << endl;
    }
    return 0;
}
