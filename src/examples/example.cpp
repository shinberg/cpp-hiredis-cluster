#include <iostream>
#include <queue>
#include <thread>
#include <assert.h>

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
    
    cluster_p = HiredisCommand<>::createCluster( "192.168.33.10", 7000 );
    
    reply = static_cast<redisReply*>( HiredisCommand<>::Command( cluster_p, "FOO", "SET %s %s", "FOO", "BAR1" ) );
    
    if( reply->type == REDIS_REPLY_STATUS  || reply->type == REDIS_REPLY_ERROR )
    {
        cout << " Reply to SET FOO BAR " << endl;
        cout << reply->str << endl;
    }
    
    freeReplyObject( reply );
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
