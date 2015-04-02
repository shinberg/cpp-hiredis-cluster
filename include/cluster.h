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

#ifndef __libredisCluster__cluster__
#define __libredisCluster__cluster__

#include <map>

extern "C"
{
#include <hiredis/hiredis.h>
}

#include "slothash.h"
#include "clusterexception.h"

namespace RedisCluster
{
    using std::string;
    
    class NonCopyable
    {
    protected:
        NonCopyable() {}
        ~NonCopyable() {}
    private:
        NonCopyable( const NonCopyable& );
        const NonCopyable& operator=( const NonCopyable& );
    };
    
    template <typename redisConnection>
    class Cluster : public NonCopyable {
        
        typedef unsigned int SlotIndex;
        typedef std::pair<SlotIndex, SlotIndex> SlotRange;
        
        struct SlotComparator {
            bool operator()(const SlotRange& a, const SlotRange& b) const {
                return a.first >= b.first;
            }
        };
        typedef std::map <SlotRange, redisConnection*, SlotComparator> ClusterNodes;
        
    public:
        typedef redisConnection* (*pt2RedisConnectFunc) ( const char*, int, void* );
        typedef void (*pt2RedisFreeFunc) ( redisConnection * );
        typedef Cluster* ptr_t;
        
        Cluster( redisReply *reply, pt2RedisConnectFunc connect, pt2RedisFreeFunc disconnect, void *conData ) : readytouse_( false ), isbroken_( false )
        {
            init( reply, connect, disconnect, conData );
        }
        
        ~Cluster()
        {
            disconnect();
        }
        
        void disconnect()
        {
            if( disconnect_ != NULL )
            {
                typename ClusterNodes::iterator it(nodes_.begin()), end(nodes_.end());
                while ( it != end )
                {
                    disconnect_( it->second );
                    ++it;
                }
            }
            nodes_.clear();
        }
        
        inline static const char* CmdInit()
        {
            return "cluster slots";
        }
        
        redisConnection * getConnection ( std::string key )
        {
            if( !readytouse_ )
            {
                throw NotInitializedException();
            }
            
            redisConnection *conn = NULL;
            int slot = SlotHash::SlotByKey( key.c_str(), (int)key.length() );
            
            SlotRange range = { slot + 1, 0 };
            
            typename ClusterNodes::iterator node = nodes_.lower_bound( range );
            
            if ( node != nodes_.end() )
            {
                range = node->first;
                if ( range.first > slot || range.second < slot )
                {
                    throw NodeSearchException();
                }
                else
                {
                    conn = node->second;
                }
            }
            else
            {
                throw NodeSearchException();
            }
            
            return conn;
        }
        
        // moved method set cluster to moved state
        // if cluster is in moved state, then you need to reinitialise it
        // cluster can be used some time in moved state, but with processing redis cluster
        // redirections, this may hit some performance issues in your code
        // for information about cluster redirections read this link http://redis.io/topics/cluster-spec
        void moved()
        {
            readytouse_ = false;
        }
        
        redisConnection* createNewConnection( string host, string port )
        {
            return connect_( host.c_str(), std::stoi(port), data_ );
        }
        
    private:
        
        void init(redisReply *reply, pt2RedisConnectFunc connect, pt2RedisFreeFunc disconnect, void *conData)
        {
            if( connect == NULL || disconnect == NULL )
                throw InvalidArgument();
            
            connect_ = connect;
            disconnect_ = disconnect;
            data_ = conData;
            
            if( reply->type == REDIS_REPLY_ARRAY )
            {
                size_t cnt = reply->elements;
                for( size_t i = 0; i < cnt; i++ )
                {
                    if( reply->element[i]->type== REDIS_REPLY_ARRAY &&
                       reply->element[i]->elements >= 3 &&
                       reply->element[i]->element[0]->type == REDIS_REPLY_INTEGER &&
                       reply->element[i]->element[1]->type == REDIS_REPLY_INTEGER &&
                       reply->element[i]->element[2]->type == REDIS_REPLY_ARRAY &&
                       reply->element[i]->element[2]->elements >= 2 &&
                       reply->element[i]->element[2]->element[0]->type == REDIS_REPLY_STRING &&
                       reply->element[i]->element[2]->element[1]->type == REDIS_REPLY_INTEGER)
                    {
                        SlotRange slots = { reply->element[i]->element[0]->integer,
                            reply->element[i]->element[1]->integer };
                        
                        redisConnection *conn = connect( reply->element[i]->element[2]->element[0]->str,
                                                        (int)reply->element[i]->element[2]->element[1]->integer,
                                                        conData );
                        
                        if( conn == NULL || conn->err )
                        {
                            throw ConnectionFailedException();
                        }
                        
                        nodes_.insert( typename ClusterNodes::value_type(slots, conn) );
                    }
                    else
                    {
                        throw ConnectionFailedException();
                    }
                }
            }
            else
            {
                throw ConnectionFailedException();
            }
            readytouse_ = true;
        }
        
        pt2RedisConnectFunc connect_;
        pt2RedisFreeFunc disconnect_;
        ClusterNodes nodes_;
        bool readytouse_;
        bool isbroken_;
        void* data_;
    };
}



#endif /* defined(__libredisCluster__cluster__) */
