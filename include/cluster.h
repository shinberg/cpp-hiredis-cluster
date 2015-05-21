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
#include "container.h"

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
    
    // cluster class for managing cluster redis connections. Thread safety depends on ConnectionContainer.
    // If ConnectionContainer is thread safe, then Cluster class is thread safe too
    
    template <typename redisConnection, typename ConnectionContainer = DefaultContainer<redisConnection> >
    class Cluster : public NonCopyable {

    public:
        // typedefs for redis host, for redis cluster slot indexes
        // and for pair SlotConnection(initial redis cluster connection applicable for slot range)
        // and for pair HostConnection(redis cluster connection, created for redirection purposes)
        typedef string Host;
        typedef unsigned int SlotIndex;
        typedef std::pair<SlotIndex, SlotIndex> SlotRange;
        typedef std::pair<SlotRange, redisConnection*> SlotConnection;
        typedef std::pair<Host, redisConnection*> HostConnection;
        
        // definition of user connect and disconnect callbacks that can be user defined
        typedef redisConnection* (*pt2RedisConnectFunc) ( const char*, int, void* );
        typedef void (*pt2RedisFreeFunc) ( redisConnection* );
        // definition of user error handling function that can be user defined
        typedef void (*MovedCb) ( void*, Cluster<redisConnection, ConnectionContainer> & );
        // definition of raw cluster pointer
        typedef Cluster* ptr_t;
        
        struct SlotComparator {
            bool operator()(const SlotRange& a, const SlotRange& b) const {
                return a.first < b.first;
            }
        };
        
        // cluster construction is based on parsing redis reply on "CLUSTER SLOTS" command
        Cluster( redisReply *reply, pt2RedisConnectFunc connect, pt2RedisFreeFunc disconnect, void *conData ) :
        connections_( new  ConnectionContainer( connect, disconnect, conData ) ),
        userMovedFn_(NULL),
        readytouse_( false ),
        moved_( false )
        {
            if( connect == NULL || disconnect == NULL )
                throw InvalidArgument();
            // init function will parse redisReply structure
            init( reply );
        }
        
        ~Cluster()
        {
            //disconnect();
            delete connections_;
        }
        
        // disconnect function applicable when we want to close all async connections from callback
        void disconnect()
        {
            connections_->disconnect();
        }
        // just cluster slots command
        inline static const char* CmdInit()
        {
            return "cluster slots";
        }
        // function gets a connection from container by slot number
        SlotConnection getConnection ( std::string key )
        {
            if( !readytouse_ )
            {
                throw NotInitializedException();
            }
            
            int slot = SlotHash::SlotByKey( key.c_str(), key.length() );
            return connections_->getConnection( slot );
        }
        
        // moved method set cluster to moved state
        // if cluster is in moved state, then you need to reinitialise it once a time
        // cluster can be used some time in moved state, but with processing redis cluster
        // redirections, this may hit some performance issues in your code
        // for information about cluster redirections read this link http://redis.io/topics/cluster-spec
        inline void moved()
        {
            moved_ = true;
            if( userMovedFn_ != NULL )
            {
                userMovedFn_( connections_->data_, *this );
            }
        }
        // can be used to identify that cluster mey need to be reinitialized in runtime
        // because there have been some redirections
        inline bool isMoved()
        {
            return moved_;
        }
        // set moved callback function, that can by user for logging redirections
        // and for aborting redirections
        inline void setMovedCb( MovedCb fn )
        {
            userMovedFn_ = fn;
        }
        // creates new connection when HiredisCommand or AsyncHiredisCommand needs a
        // connection for follow the redirection
        inline HostConnection createNewConnection( string host, string port )
        {
            return connections_->insert(host, port);
        }
        // if we want to cluster throw NotInitializedException (i.e. in other threads)
        // we can use this function
        inline void stop()
        {
            readytouse_ = false;
        }
        
        // functions for releasing the connection after use
        // usable in case of multithreaded ConnectionContainer
        void releaseConnection( HostConnection conn )
        {
            connections_->releaseConnection( conn );
        }
        
        void releaseConnection( SlotConnection conn )
        {
            connections_->releaseConnection( conn );
        }
        
    private:
        
        void init( redisReply *reply )
        {
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
                        
                        connections_->insert(slots,
                                            reply->element[i]->element[2]->element[0]->str,
                                            (int)reply->element[i]->element[2]->element[1]->integer);
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

        ConnectionContainer *connections_;
        volatile MovedCb userMovedFn_;
        volatile bool readytouse_;
        volatile bool moved_;
        
    };
}



#endif /* defined(__libredisCluster__cluster__) */
