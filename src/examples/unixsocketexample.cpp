#include <iostream>
#include <sstream>

#include "hirediscommand.h"

using RedisCluster::HiredisCommand;
using RedisCluster::Cluster;

using std::string;
using std::out_of_range;
using std::cout;
using std::cerr;
using std::endl;

// Example of usage redis cluster with unix socket
// It may be useful when all cluster nodes are running and is being accessed
// on the same machine
// There is no support for unix sockets out of the box in redis cluster
// so we create our own Table with sockets accordance and use it in our
// custom connection function, which we will pass to the redis cluster library

// our socket table in text format (it can be file)
const char config[] =   "192.168.33.10:7000=/tmp/redis0.sock\n"
"192.168.33.10:7001=/tmp/redis1.sock\n"
"192.168.33.10:7002=/tmp/redis2.sock\n";

// our socket in std map format (can be replaced with unordered_map if you need so)
typedef std::map< string, string > SocketTable;

redisContext *customRedisConnect( const char *ip, int port, void *data )
{
    redisContext *ctx = NULL;
    SocketTable *table = static_cast<SocketTable*>(data);
    string hostPort( string( ip ) + ":" + std::to_string( port ) );
    
    try {
	string filename = table->at( hostPort );
        ctx = redisConnectUnix( filename.c_str() );
    } catch ( const std::out_of_range &oor ) {
        cerr << "Can't find unix socket for " << hostPort << endl;
    }
    
    return ctx;
}

void processUnixSocketCluster()
{
    // First, in our client code we must initialize std::map container
    // with each line whose address corresponds to appropriate unix socket
    
    // you can load config from file, but for visibility
    // it has been made here through basic char array
    static SocketTable table;
    std::istringstream is_file(config);
    
    std::string line;
    while( std::getline(is_file, line) )
    {
        std::istringstream is_line(line);
        std::string key;
        if( std::getline( is_line, key, '=' ) )
        {
            std::string value;
            if( std::getline(is_line, value) )
            {
                table.insert( SocketTable::value_type (key, value) );
            }
        }
    }
    
    Cluster<redisContext>::ptr_t cluster_p;
    redisReply * reply;
    
    // Second, pass our custom connection function
    cluster_p = HiredisCommand<>::createCluster( "127.0.0.1",
                                              7000,
                                              static_cast<void*>( &table ),
                                              customRedisConnect,
                                              redisFree );
    
    // That's all, we are ready to execute commands as usual
    // In case of adding nodes you need to update config
    // and restart redisCluster with destroying old and constructing some new cluster
    
    reply = static_cast<redisReply*>( HiredisCommand<>::Command( cluster_p, "FOO", "SET %s %s", "FOO", "BAR" ) );
    
    if( reply->type == REDIS_REPLY_STATUS  || reply->type == REDIS_REPLY_ERROR )
    {
        std::cout << " Reply to SET FOO BAR " << std::endl;
        std::cout << reply->str << std::endl;
    }
    
    freeReplyObject( reply );
    delete cluster_p;
}

int main(int argc, const char * argv[])
{
    try
    {
        processUnixSocketCluster();
    } catch ( const RedisCluster::ClusterException &e )
    {
        cout << "Cluster exception: " << e.what() << endl;
    }
    return 0;
}

