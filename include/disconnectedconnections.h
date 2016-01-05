#ifndef __libredisCluster__disconnectedconnections__
#define __libredisCluster__disconnectedconnections__

#include <vector>
#include <algorithm> 

extern "C"
{
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
}

namespace RedisCluster
{
    // Class is singleton
    // Keeps pointers to disconnected and freed by hiredis library connection contexts
    class DisconnectedConnections
    {
        typedef redisAsyncContext Connection;
        typedef std::vector<const Connection*> Connections;
        
    private:
        DisconnectedConnections() {}
        DisconnectedConnections(const DisconnectedConnections&) = delete;        
        DisconnectedConnections& operator=(const DisconnectedConnections&) = delete;
        
    public:
        static DisconnectedConnections& getInstance() {
            static DisconnectedConnections instance;
            return instance;
        }    
    
        void addConnection(const Connection *connection)
        {
            connections_.push_back(connection);
        }
        
        void delConnection(const Connection* connection)
        {
            Connections::iterator it = std::find(connections_.begin(), connections_.end(), connection);
            if (it != connections_.end()) {
                connections_.erase(it);                
            }
        }
        
        bool isConnectionDisconnnected(const Connection* connection)
        {
            Connections::iterator it = std::find(connections_.begin(), connections_.end(), connection);
            return it != connections_.end();
        }
    private:
       
        Connections connections_;
    };
}

#endif // __libredisCluster__disconnectedconnections__
