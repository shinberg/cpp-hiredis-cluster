#include <assert.h>
#include <iostream>
#include <thread>
#include <event2/thread.h>

#include "hirediscommand.h"
#include "asynchirediscommand.h"

using namespace RedisCluster;
using namespace std;

void processClusterKeysSubset()
{
    Cluster<redisContext>::ptr_t cluster_p;
    redisReply * reply;
    
    cluster_p = HiredisCommand<>::createCluster( "192.168.33.10", 7000 );
    
    for ( int i = 0; i < 16384; i++ ) {
        
        string key = std::to_string( i );
        
        reply = static_cast<redisReply*>( HiredisCommand<>::Command( cluster_p, key, "SET %s %s", key.c_str(), "test" ) );
        
        assert( REDIS_REPLY_ERROR != reply->type );
        assert( string("OK") == reply->str );
        
        cout << key << endl;
        
        freeReplyObject( reply );
        
    }
    
    delete cluster_p;
}

static void getCallback( typename Cluster<redisAsyncContext>::ptr_t cluster_p, void *r, void *data )
{
    redisReply * reply = static_cast<redisReply*>( r );
    
    assert( reply == NULL );
    assert( REDIS_REPLY_STRING == reply->type );
    assert( string("test") == reply->str );
}

static void setCallback( typename Cluster<redisAsyncContext>::ptr_t cluster_p, void *r, void *data )
{
    redisReply * reply = static_cast<redisReply*>( r );
    
    assert( REDIS_REPLY_ERROR != reply->type );
    assert( string("OK") == reply->str );
}

AsyncHiredisCommand<>::Action errorHandler(const AsyncHiredisCommand<> &cmd,
                                         const ClusterException &exception,
                                         HiredisProcess::processState state )
{
    AsyncHiredisCommand<>::Action action = AsyncHiredisCommand<>::FINISH;
    
    if( dynamic_cast<const CriticalException*>(&exception) == NULL )
    {
        cerr << "Exception in processing async redis callback: " << exception.what() << endl;
        cerr << "Retrying" << endl;
        action = AsyncHiredisCommand<>::RETRY;
    }
    else
    {
        cerr << "Critical exception in processing async redis callback: " << exception.what() << endl;
        action = AsyncHiredisCommand<>::RETRY;
    }
    return action;
}

void getKeyVal( char *str, Cluster<redisAsyncContext>::ptr_t cluster_p )
{
    AsyncHiredisCommand<> &cmd = AsyncHiredisCommand<>::Command( cluster_p, str, getCallback, NULL, "GET %s", str );
    cmd.setUserErrorCb( errorHandler );
}

void setKeyVal( char *str, Cluster<redisAsyncContext>::ptr_t cluster_p )
{
    AsyncHiredisCommand<> &cmd = AsyncHiredisCommand<>::Command( cluster_p, str, setCallback, NULL, "SET %s test",  str );
    cmd.setUserErrorCb( errorHandler );
}

typedef void (*redisFunc_p) ( char *str, Cluster<redisAsyncContext>::ptr_t cluster_p );

template < class RCLuster, typename Func >
void testOneSLot( RCLuster cluster_p, Func func, int maxdepth )
{
    // fill with only printable charecters to check visualy
    // but redis can also use all types of binary arrays as keys or as values
    const int maxprintable = 127;
    const int minprintable = 33;
    
    char str[maxdepth+1];
    for ( int depth = 0; depth < maxdepth; depth++ )
    {
        str[depth] = minprintable;
    }
    str[maxdepth] = 0;
    
    int depth = 0;
    int keysSlotCntr = 0;
    do
    {
        if( RedisCluster::SlotHash::SlotByKey( str, maxdepth ) == 1 )
        {
            func( (char*)str, cluster_p );
            ++keysSlotCntr;
            cout << str << endl;
        }
        
        ++str[depth];
        if( str[depth] >= maxprintable )
        {
            str[depth] = minprintable;
            ++depth;
        }
        else
        {
            depth = 0;
        }
        
    } while ( depth != maxdepth );
    
    cout << keysSlotCntr << endl;
}

void runAsyncAskingTest( )
{
    Cluster<redisAsyncContext>::ptr_t cluster_p;
    redisFunc_p func = getKeyVal;
    
    event_init();
    struct event_base *base = event_base_new();
    
    cluster_p = AsyncHiredisCommand<>::createCluster( "192.168.33.10", 7000, static_cast<void*>( base ) );
    
    testOneSLot( cluster_p, func, 5 );
    
    event_base_dispatch(base);
    
    delete cluster_p;
    event_base_free(base);
}

void getSyncKeyVal( char *str, Cluster<redisContext>::ptr_t cluster_p )
{
    redisReply *reply = static_cast<redisReply*>( HiredisCommand<>::Command( cluster_p, str, "GET %s", str ) );
    
    assert( REDIS_REPLY_STRING == reply->type );
    assert( string("test") == reply->str );
    
    freeReplyObject(reply);
}

void runAskingTest()
{
    Cluster<redisContext>::ptr_t cluster_p;
    cluster_p = HiredisCommand<>::createCluster( "192.168.33.10", 7000 );
    
    testOneSLot( cluster_p, getSyncKeyVal, 5 );
    
    delete cluster_p;
}

int main(int argc, const char * argv[])
{
    try
    {
//        fillClusterSLot( );
//        processClusterKeysSubset();
        runAskingTest();
//        runAsyncAskingTest();
    } catch ( const RedisCluster::ClusterException &e )
    {
        cout << "Cluster exception: " << e.what() << endl;
    }
    return 0;
}

