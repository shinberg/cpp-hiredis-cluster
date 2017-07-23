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

#ifndef __libredisCluster__command__
#define __libredisCluster__command__

#include <iostream>
#include "cluster.h"
#include "hiredisprocess.h"
#include <memory>

extern "C"
{
#include "hiredis/hiredis.h"
}

namespace RedisCluster
{
    using std::string;

    typedef std::shared_ptr<redisReply> Reply;
    template < typename Cluster = Cluster<redisContext> >
    class HiredisCommand
    {
        typedef redisContext Connection;
        enum CommandType
        {
            SDS,
            FORMATTED_STRING
        };
        
        HiredisCommand(const HiredisCommand&) = delete;
        HiredisCommand& operator=(const HiredisCommand&) = delete;
        
    public:
        
        static typename Cluster::ptr_t createCluster(const char* host,
                                                          int port,
                                                          void* data = NULL,
                                                          typename Cluster::pt2RedisConnectFunc conn = connectFunction,
                                                          typename Cluster::pt2RedisFreeFunc free = freeFunction,
                                                          const struct timeval &timeout = { 3, 0 } )
        {
            typename Cluster::ptr_t cluster(NULL);
            redisReply *reply = nullptr;
            
            redisContext *con = redisConnectWithTimeout( host, port, timeout );
            if( con == NULL || con->err )
                throw ConnectionFailedException(nullptr);
            
            reply = static_cast<redisReply*>( redisCommand( con, Cluster::CmdInit() ) );
            HiredisProcess::checkCritical( reply, true );

            cluster = new Cluster( reply, conn, free, data );
            
            freeReplyObject( reply );
            redisFree( con );
            return cluster;
        }
        
        static void deleteReply (redisReply *reply) {
            freeReplyObject(reply);
        }
        
        static inline Reply AltCommand( typename Cluster::ptr_t cluster_p,
                                    string key,
                                    int argc,
                                    const char ** argv,
                                    const size_t *argvlen )
        {
            return Reply(HiredisCommand( cluster_p, key, argc, argv, argvlen ).process(), deleteReply);
        }
        
        static inline Reply AltCommand( typename Cluster::ptr_t cluster_p,
                                    string key,
                                    const char *format, ...)
        {
            va_list ap;
            va_start( ap, format );
            return Reply(HiredisCommand( cluster_p, key, format, ap ).process(), deleteReply);
            va_end(ap);
        }
        
        static inline Reply AltCommand( typename Cluster::ptr_t cluster_p,
                                    string key,
                                    const char *format, va_list ap)
        {
            return Reply(HiredisCommand( cluster_p, key, format, ap ).process(), deleteReply);
        }
        
        static inline void* Command( typename Cluster::ptr_t cluster_p,
                                   string key,
                                   int argc,
                                   const char ** argv,
                                   const size_t *argvlen )
        {
            return HiredisCommand( cluster_p, key, argc, argv, argvlen ).process();
        }
        
        static inline void* Command( typename Cluster::ptr_t cluster_p,
                                   string key,
                                   const char *format, ...)
        {
            va_list ap;
            va_start( ap, format );
            return HiredisCommand( cluster_p, key, format, ap ).process();
            va_end(ap);
        }
        
        static inline void* Command( typename Cluster::ptr_t cluster_p,
                                    string key,
                                    const char *format, va_list ap)
        {
            return HiredisCommand( cluster_p, key, format, ap ).process();
        }
        
    protected:
        
        HiredisCommand( typename Cluster::ptr_t cluster_p,
                       string key,
                       int argc,
                       const char ** argv,
                       const size_t *argvlen ) :
        cluster_p_( cluster_p ),
        key_( key ),
        cmd_{},
        len_{},
        type_( SDS )
        {
            if( cluster_p == NULL )
                throw InvalidArgument(nullptr);
            
            len_ = redisFormatSdsCommandArgv(&cmd_, argc, argv, argvlen);
        }
        
        HiredisCommand( typename Cluster::ptr_t cluster_p,
                       string key,
                       const char *format, va_list ap ) :
        cluster_p_( cluster_p ),
        key_( key ),
        cmd_{},
        len_{},
        type_( FORMATTED_STRING )
        {
            if( cluster_p == NULL )
                throw InvalidArgument(nullptr);

            len_ = redisvFormatCommand(&cmd_, format, ap);
        }
        
        ~HiredisCommand()
        {
            if( type_ == SDS )
            {
                sdsfree( (sds)cmd_ );
            }
            else
            {
                free( cmd_ );
            }
        }
        
        redisReply* processHiredisCommand( Connection *con ) {
            redisReply* reply;
            redisAppendFormattedCommand( con, cmd_, len_ );
            redisGetReply( con, (void**)&reply );
            return reply;
        }
        
        redisReply* asking( Connection *con  ) {
            return static_cast<redisReply*>( redisCommand( con, "ASKING" ) );
        }
        
        redisReply* process()
        {
            redisReply *reply = nullptr;
            typename Cluster::SlotConnection con = cluster_p_->getConnection( key_ );
            typename Cluster::HostConnection hcon = { "", NULL };
            string host, port;

            reply = processHiredisCommand( con.second );
            HiredisProcess::checkCritical(reply, false, true, std::string(), con.second);
            cluster_p_->releaseConnection( con );

            HiredisProcess::processState state = HiredisProcess::processResult( reply, host, port);
            
            switch ( state ) {
                case HiredisProcess::ASK:
                    freeReplyObject( reply );
                    hcon = cluster_p_->createNewConnection( host, port );
                    
                    if (hcon.second != NULL && hcon.second->err == 0) {
                        reply = asking( hcon.second );
                        HiredisProcess::checkCritical(reply, true, "asking error");
                    
                        freeReplyObject( reply );
                        reply = processHiredisCommand(hcon.second);
                        HiredisProcess::checkCritical(reply, false);
                    
                        cluster_p_->releaseConnection( hcon );
                    }
                    else if( hcon.second == NULL )
                        throw LogicError(nullptr, "Can't connect while resolving asking state");
                    else {
                        cluster_p_->releaseConnection( hcon );
                        throw LogicError(nullptr, hcon.second->errstr);
                    }
                    break;
                case HiredisProcess::MOVED:
                    freeReplyObject( reply );
                    hcon = cluster_p_->createNewConnection( host, port );
                    if( hcon.second != NULL && hcon.second->err == 0 ) {
                        reply = processHiredisCommand( hcon.second );
                        cluster_p_->moved();
                    }
                    else if( hcon.second == NULL )
                        throw LogicError(nullptr, "Can't connect while resolving asking state");
                    else
                        throw LogicError(nullptr, hcon.second->errstr );
                    cluster_p_->releaseConnection( hcon );
                    
                    break;
                case HiredisProcess::READY:
                    break;
                default:
                    throw LogicError(reply, "error in state processing" );
            }
            return reply;
        }
        
        static Connection* connectFunction( const char* host, int port, void * )
        {
            return redisConnect( host, port);
        }
        
        static void freeFunction( Connection* con )
        {
            redisFree( con );
        }
        
        typename Cluster::ptr_t cluster_p_;
        string key_;
        char *cmd_;
        int len_;
        CommandType type_;
    };
}

#endif /* defined(__libredisCluster__command__) */
