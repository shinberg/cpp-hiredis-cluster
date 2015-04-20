/*
 * Copyright (c) 2015, Dmitrii Shinkevich <shinmail at gmail dot com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __libredisCluster__container__
#define __libredisCluster__container__

#include "cluster.h"

namespace RedisCluster {

    template<typename redisConnection, typename ConnectionContainer>
    class Cluster;
    
    // Container for redis connections. Simple container defined here, it's not thread safe
    // but can be replaced by user defined container as Cluster template class
    template<typename redisConnection>
    class DefaultContainer
    {
        typedef Cluster<redisConnection, DefaultContainer> RCluster;
        typedef typename RCluster::SlotRange SlotRange;
        typedef typename RCluster::Host Host;
        
        typedef std::map <SlotRange, redisConnection*, typename RCluster::SlotComparator> ClusterNodes;
        typedef std::map <Host, redisConnection*> RedirectConnections;
        
    public:
        
        DefaultContainer( typename RCluster::pt2RedisConnectFunc conn,
                         typename RCluster::pt2RedisFreeFunc disconn,
                         void* userData ) :
        data_( userData ),
        connect_(conn),
        disconnect_(disconn)
        {
        }
        
        ~DefaultContainer()
        {
            disconnect();
        }
        
        inline
        void insert( typename RCluster::SlotRange slots, const char* host, int port )
        {
            redisConnection* conn = connect_( host,
                                            port,
                                            data_ );
            
            if( conn == NULL || conn->err )
            {
                throw ConnectionFailedException();
            }
            
            nodes_.insert( typename ClusterNodes::value_type(slots, conn) );
        }
        
        inline
        typename RCluster::HostConnection insert( string host, string port )
        {
            string key( host + ":" + port );
            try
            {
                return typename RCluster::HostConnection( key, connections_.at( key ) );
            }
            catch( const std::out_of_range &oor )
            {
            }
            
            typename RCluster::HostConnection conn( key, connect_( host.c_str(), std::stoi(port), data_ ) );
            if( conn.second != NULL && conn.second->err == 0 )
            {
                connections_.insert( conn );
            }
            return conn;
        }
        
        template<typename Storage>
        inline static typename Storage::iterator searchBySlots( typename RCluster::SlotIndex index, Storage &storage )
        {
            typename RCluster::SlotRange range = { index + 1, 0 };
            typename Storage::iterator node = storage.lower_bound( range );
            // as with lower bound we find greater (next) slotrange, so now decrement
            --node;
            
            if ( node != storage.end() )
            {
                range = node->first;
                if ( range.first > index || range.second < index )
                {
                    throw NodeSearchException();
                }
                else
                {
                    return node;
                }
            }
            else
            {
                throw NodeSearchException();
            }
        }
        
        inline
        typename RCluster::SlotConnection getConnection( typename RCluster::SlotIndex index )
        {
            return *searchBySlots(index, nodes_);
        }
        
        // for a not multithreaded container this functions are dummy
        inline void releaseConnection( typename RCluster::SlotConnection ) {}
        inline void releaseConnection( typename RCluster::HostConnection ) {}
        
        inline
        void disconnect()
        {
            disconnect<ClusterNodes>( nodes_ );
            disconnect<RedirectConnections>( connections_ );
        }
        
        template <typename T>
        inline void disconnect(T &cons)
        {
            if( disconnect_ != NULL )
            {
                typename T::iterator it(cons.begin()), end(cons.end());
                while ( it != end )
                {
                    disconnect_( it->second );
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
    };
    
}

#endif /* defined(__libredisCluster__cluster__) */
