#include <iostream>
#include <queue>
#include <thread>
#include <condition_variable>
#include <assert.h>

#include "hirediscommand.h"

using namespace RedisCluster;
using std::string;
using std::cout;
using std::cerr;
using std::endl;

/*
 *
 * It's an example showing how we can create multithreaded application with connection pool
 *
 * Creating custom logic consits of two step
 *
 * 1. Create a ThreadedPool class (or just copy and paste it from here)
 * 2. Use ThreadedPool class as template parameter for Cluster and HiredisCommand classes
 *
 * The benefits of not including such class into library is that you can copy and paste threadedpool
 * or modify class without any restrictions on copyrights!
 * This demonstrates maximum flexibility of library that other libraries doesn't have at this moment
 *
 * If ThreadedPool is too complicated to understand then just copy and paste this class to begin
 *
 */

// We have to define class with all methods, that DefaultContainer in library have (in container.h)
template<typename redisConnection>
class ThreadedPool
{
    static const int poolSize_ = 10;
    typedef Cluster<redisConnection, ThreadedPool> RCluster;
    // We will save our pool in std::queue here
    typedef std::queue<redisConnection*> ConQueue;
    // Define pair with condition variable, so we can notify threads, when new connection is released from some thread
    typedef std::pair<std::condition_variable, ConQueue> ConPool;
    // Container for saving connections by their slots, just as
    typedef std::map <typename RCluster::SlotRange, ConPool*, typename RCluster::SlotComparator> ClusterNodes;
    // Container for saving connections by host and port (for redirecting)
    typedef std::map <typename RCluster::Host, ConPool*> RedirectConnections;
    // rename cluster types
    typedef typename RCluster::SlotConnection SlotConnection;
    typedef typename RCluster::HostConnection HostConnection;
    
public:
    
    ThreadedPool( typename RCluster::pt2RedisConnectFunc conn,
                 typename RCluster::pt2RedisFreeFunc disconn,
                 void* userData ) :
    data_( userData ),
    connect_(conn),
    disconnect_(disconn)
    {
    }
    
    ~ThreadedPool()
    {
        disconnect();
    }
    
    // helper function for creating connections in loop
    inline void fillPool( ConPool &pool, const char* host, int port )
    {
        for( int i = 0; i < poolSize_; ++i )
        {
            redisConnection *conn = connect_( host,
                                            port,
                                            data_ );
            
            if( conn == NULL || conn->err )
            {
                throw ConnectionFailedException();
            }
            pool.second.push( conn );
        }
    }
    
    // helper for fetching connection from pool
    inline redisConnection* pullConnection( std::unique_lock<std::mutex> &locker, ConPool &pool )
    {
        redisConnection *con = NULL;
        // here we wait for other threads for release their connections if the queue is empty
        while (pool.second.empty())
        {
            // if queue is empty here current thread is waiting for somethread to release one
            pool.first.wait(locker);
        }
        // get a connection from queue
        con = pool.second.front();
        pool.second.pop();
        
        return con;
    }
    // helper for releasing connection and placing it in pool
    inline void pushConnection( std::unique_lock<std::mutex> &locker, ConPool &pool, redisConnection* con )
    {
        pool.second.push(con);
        locker.unlock();
        // notify other threads for their wake up in case of they are waiting
        // about empty connection queue
        pool.first.notify_one();
    }
    
    // function inserts new connection by range of slots during cluster initialization
    inline void insert( typename RCluster::SlotRange slots, const char* host, int port )
    {
        std::unique_lock<std::mutex> locker(conLock_);
        
        ConPool* &pool = nodes_[slots];
        pool = new ConPool();
        fillPool(*pool, host, port);
    }
    
    // function inserts or returning existing one connection used for redirecting (ASKING or MOVED)
    inline HostConnection insert( string host, string port )
    {
        std::unique_lock<std::mutex> locker(conLock_);
        string key( host + ":" + port );
        HostConnection con = { key, NULL };
        try
        {
            con.second = pullConnection( locker, *connections_.at( key ) );
        }
        catch( const std::out_of_range &oor )
        {
        }
        // create new connection in case if we didn't redirecting to this
        // node before
        if( con.second == NULL )
        {
            ConPool* &pool = connections_[key];
            pool = new ConPool();
            fillPool(*pool, host.c_str(), std::stoi(port));
            con.second = pullConnection( locker, *pool );
        }
        
        return con;
    }
    
    
    inline SlotConnection getConnection( typename RCluster::SlotIndex index )
    {
        std::unique_lock<std::mutex> locker(conLock_);
        
        typedef typename ClusterNodes::iterator iterator;
        iterator found = DefaultContainer<redisConnection>::template searchBySlots(index, nodes_);
        
        return { found->first, pullConnection( locker, *found->second ) };
    }
    
    // this function is invoked when library whants to place initial connection
    // back to the storage and the connections is taken by slot range from storage
    inline void releaseConnection( SlotConnection conn )
    {
        std::unique_lock<std::mutex> locker(conLock_);
        pushConnection( locker, *nodes_[conn.first], conn.second );
    }
    // same function for redirection connections
    inline void releaseConnection( HostConnection conn )
    {
        std::unique_lock<std::mutex> locker(conLock_);
        pushConnection( locker, *connections_[conn.first], conn.second );
    }
    
    // disconnect both thread pools
    inline void disconnect()
    {
        disconnect<ClusterNodes>( nodes_ );
        disconnect<RedirectConnections>( connections_ );
    }
    
    template <typename T>
    inline void disconnect(T &cons)
    {
        std::unique_lock<std::mutex> locker(conLock_);
        if( disconnect_ != NULL )
        {
            typename T::iterator it(cons.begin()), end(cons.end());
            while ( it != end )
            {
                for( int i = 0; i < poolSize_; ++i )
                {
                    // pullConnection will wait for all connection
                    // to be released
                    disconnect_( pullConnection( locker, *it->second) );
                }
                if( it->second != NULL )
                {
                    delete it->second;
                    it->second = NULL;
                }
                ++it;
            }
        }
        cons.clear();
    }
    
    void* data_;
private:
    typename RCluster::pt2RedisConnectFunc connect_;
    typename RCluster::pt2RedisFreeFunc disconnect_;
    RedirectConnections connections_;
    ClusterNodes nodes_;
    std::mutex conLock_;
};

/*
 * Now when most ThreadedPool is defined we can use new ThreadedPool class as template parameter
 * parametrize Cluster with ThreadedPool
 *
 */
typedef Cluster<redisContext, ThreadedPool<redisContext> > ThreadPoolCluster;

volatile int cnt = 0;
std::mutex lock;

void commandThread( ThreadPoolCluster::ptr_t cluster_p )
{
    redisReply * reply;
    // use defined custom cluster as template parameter for HiredisCommand here
    reply = static_cast<redisReply*>( HiredisCommand<ThreadPoolCluster>::Command( cluster_p, "FOO", "SET %s %s", "FOO", "BAR1" ) );
    
    // check the result with assert
    assert( reply->type == REDIS_REPLY_STATUS && string(reply->str) == "OK" );
    
    {
        std::lock_guard<std::mutex> locker(lock);
        cout << ++cnt << endl;
    }
    
    freeReplyObject( reply );
}

void processCommandPool()
{
    const int threadsNum = 1000;
    
    // use defined ThreadedPool here
    ThreadPoolCluster::ptr_t cluster_p;
    // and here as template parameter
    cluster_p = HiredisCommand<ThreadPoolCluster>::createCluster( "192.168.33.10", 7000 );
    
    std::thread thr[threadsNum];
    for( int i = 0; i < threadsNum; ++i )
    {
        thr[i] = std::thread( commandThread, cluster_p );
    }
    
    for( int i = 0; i < threadsNum; ++i )
    {
        thr[i].join();
    }
    
    delete cluster_p;
}

int main(int argc, const char * argv[])
{
    try
    {
        processCommandPool();
    } catch ( const RedisCluster::ClusterException &e )
    {
        cout << "Cluster exception: " << e.what() << endl;
    }
    return 0;
}
