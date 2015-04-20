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

#ifndef __libredisCluster__asynchirediscommand__
#define __libredisCluster__asynchirediscommand__

#include <iostream>
#include <assert.h>

#include "cluster.h"
#include "hiredisprocess.h"

extern "C"
{
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>
}

namespace RedisCluster
{
    using std::string;
    
    // asynchronous libevent async command class, can be rewrited to another
    // async library by a few modifications
    template < typename Cluster = Cluster<redisAsyncContext> >
    class AsyncHiredisCommand : public NonCopyable
    {
        typedef redisAsyncContext Connection;
        enum CommandType
        {
            SDS,
            FORMATTED_STRING
        };
        
    public:
        
        enum Action
        {
            REDIRECT,
            FINISH,
            ASK,
            RETRY
        };
        
        typedef void (*pt2AsyncAttachFn)( Connection*, void * );
        typedef void (redisCallbackFn)( typename Cluster::ptr_t cluster_p, void*, void* );
        typedef Action (userErrorCallbackFn)( const AsyncHiredisCommand<Cluster> &,
                                                      const ClusterException &,
                                                      HiredisProcess::processState );
        
        
        static inline AsyncHiredisCommand<Cluster>& Command( typename Cluster::ptr_t cluster_p,
                                        string key,
                                        redisCallbackFn userCb,
                                        void *userPrivData,
                                        int argc,
                                        const char ** argv,
                                        const size_t *argvlen )
        {
            // would be deleted in redis reply callback or in case of error
            AsyncHiredisCommand<Cluster> *c = new AsyncHiredisCommand<Cluster>( cluster_p, key, userCb, userPrivData, argc, argv, argvlen );
            if( c->process() != REDIS_OK )
            {
                delete c;
                throw DisconnectedException();
            }
            return *c;
        }
        
        static inline AsyncHiredisCommand<Cluster>& Command( typename Cluster::ptr_t cluster_p,
                                        string key,
                                        redisCallbackFn userCb,
                                        void *userPrivData,
                                        const char *format, ... )
        {
            va_list ap;
            va_start(ap, format);
            // would be deleted in redis reply callback or in case of error
            AsyncHiredisCommand<Cluster> *c = new AsyncHiredisCommand<Cluster>( cluster_p, key, userCb, userPrivData, format, ap );
            if( c->process() != REDIS_OK )
            {
                delete c;
                throw DisconnectedException();
            }
            va_end(ap);
            return *c;
        }
        
        static inline AsyncHiredisCommand<Cluster>& Command( typename Cluster::ptr_t cluster_p,
                                                   string key,
                                                   redisCallbackFn userCb,
                                                   void *userPrivData,
                                                   const char *format, va_list ap )
        {
            // would be deleted in redis reply callback or in case of error
            AsyncHiredisCommand<Cluster> *c = new AsyncHiredisCommand<Cluster>( cluster_p, key, userCb, userPrivData, format, ap );
            if( c->process() != REDIS_OK )
            {
                delete c;
                throw DisconnectedException();
            }
            return *c;
        }

        static typename Cluster::ptr_t createCluster( const char* host,
                                                                int port,
                                                                void* data = NULL,
                                                                typename Cluster::pt2RedisConnectFunc conn = libeventConnect,
                                                                typename Cluster::pt2RedisFreeFunc free = Disconnect,
                                                                const struct timeval &timeout = { 3, 0 } )
        {
            typename Cluster::ptr_t cluster(NULL);
            redisReply *reply;
            
            redisContext *con = redisConnectWithTimeout( host, port, timeout );
            if( con == NULL || con->err )
                throw ConnectionFailedException();
            
            reply = static_cast<redisReply*>( redisCommand( con, Cluster::CmdInit() ) );
            HiredisProcess::checkCritical( reply, true );

            cluster = new Cluster( reply, conn, free, data );
            
            freeReplyObject( reply );
            redisFree( con );
            return cluster;
        }
        
        inline void* getUserPrivData()
        {
            return userPrivData_;
        }
        
        inline void setUserErrorCb( userErrorCallbackFn *userErrorCb )
        {
            userErrorCb_ = userErrorCb;
        }
        
    protected:
        
        static void Disconnect(Connection *ac)
        {
            redisAsyncDisconnect( ac );
        }
        
        AsyncHiredisCommand( typename Cluster::ptr_t cluster_p,
                            string key,
                            redisCallbackFn userCb,
                            void *userPrivData,
                            int argc,
                            const char ** argv,
                            const size_t *argvlen ) :
        cluster_p_( cluster_p ),
        userCallback_p_( userCb ),
        userPrivData_( userPrivData ),
        userErrorCb_( NULL ),
        con_( {"",  NULL} ),
        key_( key ),
        cmd_(NULL),
        type_( SDS )
        {
            // TODO: check it and check for correct distruction
            if( cluster_p == NULL )
                throw InvalidArgument();
            
            len_ = redisFormatSdsCommandArgv(&cmd_, argc, argv, argvlen);
        }
        
        AsyncHiredisCommand( typename Cluster::ptr_t cluster_p,
                            string key,
                            redisCallbackFn userCb,
                            void *userPrivData,
                            const char *format, va_list ap ) :

        cluster_p_( cluster_p ),
        userCallback_p_( userCb ),
        userPrivData_( userPrivData ),
        userErrorCb_( NULL ),
        con_( {"", NULL} ),
        key_( key ),
        cmd_(NULL),
        type_( FORMATTED_STRING )
        {
            // TODO: check it and check for correct distruction
            if( cluster_p == NULL )
                throw InvalidArgument();

            len_ = redisvFormatCommand(&cmd_, format, ap);
        }
        
        ~AsyncHiredisCommand()
        {
            if( con_.second != NULL )
            {
                redisAsyncDisconnect( con_.second );
            }
            
            if( type_ == SDS )
            {
                sdsfree( (sds)cmd_ );
            }
            else
            {
                free( cmd_ );
            }
        }
        
        inline int process()
        {
            typename Cluster::SlotConnection con = cluster_p_->getConnection( key_ );
            return processHiredisCommand( con.second );
        }
        
        inline int processHiredisCommand( Connection* con )
        {
            return redisAsyncFormattedCommand( con, processCommandReply, static_cast<void*>( this ), cmd_, len_ );
        }
        
        static void askingCallback( Connection* con, void *r, void *data )
        {
            redisReply *reply = static_cast<redisReply*>( r );
            AsyncHiredisCommand<Cluster>* that = static_cast<AsyncHiredisCommand<Cluster>*>( data );
            Action commandState = ASK;

            try
            {
                HiredisProcess::checkCritical( reply, false );
                if( reply->type == REDIS_REPLY_STATUS && string(reply->str) == "OK" )
                {
                    if( that->processHiredisCommand( that->con_.second ) != REDIS_OK )
                    {
                        throw AskingFailedException();
                    }
                }
                else
                {
                    throw AskingFailedException();
                }
            }
            catch ( const ClusterException &ce )
            {
                if ( that->userErrorCb_ != NULL && that->userErrorCb_( *that, ce, HiredisProcess::ASK ) == RETRY )
                {
                    commandState = RETRY;
                }
                else
                {
                    commandState = FINISH;
                }
            }
            
            if( commandState == RETRY )
            {
                retry( con, r, data );
            }
            else if( commandState == FINISH )
            {
                that->userCallback_p_( that->cluster_p_, r, that->userPrivData_ );
                
                if( !( con->c.flags & ( REDIS_SUBSCRIBED ) ) )
                    delete that;
            }
        }
        
        static void processCommandReply( Connection* con, void *r, void *data )
        {
            redisReply *reply = static_cast< redisReply* >(r);
            AsyncHiredisCommand<Cluster>* that = static_cast<AsyncHiredisCommand<Cluster>*>( data );
            Action commandState = FINISH;
            HiredisProcess::processState state = HiredisProcess::FAILED;
            string host, port;
            
            try
            {
                HiredisProcess::checkCritical( reply, false );
                state = HiredisProcess::processResult( reply, host, port);
                
                switch ( state ) {
                        
                    case HiredisProcess::ASK:
                        
                        if( that->con_.second == NULL )
                        {
                            that->con_ = that->cluster_p_->createNewConnection( host, port );
                        }
                        
                        if ( redisAsyncCommand( that->con_.second, askingCallback, that, "ASKING" ) == REDIS_OK )
                        {
                            commandState = ASK;
                        }
                        else
                        {
                            throw AskingFailedException();
                        }
                        break;
                        
                        
                    case HiredisProcess::MOVED:
                        that->cluster_p_->moved();
                        
                        if( that->con_.second == NULL )
                        {
                            that->con_ = that->cluster_p_->createNewConnection( host, port );
                        }
                        
                        if( that->processHiredisCommand( that->con_.second ) == REDIS_OK )
                        {
                            commandState = REDIRECT;
                        }
                        else
                        {
                            throw MovedFailedException();
                        }
                        break;
                        
                    case HiredisProcess::READY:
                        break;
                        
                    case HiredisProcess::CLUSTERDOWN:
                        throw ClusterDownException();
                        break;
                        
                    default:
                        throw LogicError();
                        break;
                }
            }
            catch ( const ClusterException &ce )
            {
                if ( that->userErrorCb_ != NULL && that->userErrorCb_( *that, ce, state ) == RETRY )
                {
                    commandState = RETRY;
                }
            }
            
            if( commandState == RETRY )
            {
                retry( con, r, data );
            }
            else if( commandState == FINISH )
            {
                that->userCallback_p_( that->cluster_p_, r, that->userPrivData_ );
                
                if( !( con->c.flags & ( REDIS_SUBSCRIBED ) ) )
                    delete that;
            }
        }
        
        static void retry( Connection *con, void *r, void *data )
        {
            AsyncHiredisCommand<Cluster>* that = static_cast<AsyncHiredisCommand<Cluster>*>( data );
            
            if( that->processHiredisCommand( con ) != REDIS_OK )
            {
                that->userErrorCb_( *that, DisconnectedException(), HiredisProcess::FAILED );
                that->userCallback_p_( that->cluster_p_, r, that->userPrivData_ );
                delete that;
            }
        }
            
        static Connection* libeventConnect( const char* host, int port, void *data )
        {
            Connection *con = NULL;
            event_base *evbase = static_cast<event_base*>( data );
            
            if( evbase != NULL )
            {
                con = redisAsyncConnect( host, port );
            
                if( con != NULL && con->err == 0 )
                {
                    if ( con->err != 0 || redisLibeventAttach( con, evbase ) != REDIS_OK )
                    {
                        redisAsyncFree( con );
                        throw ConnectionFailedException();
                    }
                }
                else
                {
                    throw ConnectionFailedException();
                }
            }
            else
            {
                throw ConnectionFailedException();
            }
            return con;
        }
        
        // pointer to shared cluster object ( cluster class is not threadsafe )
        typename Cluster::ptr_t cluster_p_;
        
        // user-defined callback to redis async command as usual
        redisCallbackFn *userCallback_p_;
        // user-defined callback data
        void* userPrivData_;
        // user error handler
        userErrorCallbackFn *userErrorCb_;
        
        // pointer to async context ( in case of redirection class creates new connection )
        typename Cluster::HostConnection con_;

        // key of redis command to find proper cluster node
        string key_;
        // pointer to formatted hiredis object
        char *cmd_;
        // length of formatted hiredis object
        int len_;
        // type of formatted hiredis object
        CommandType type_;
    };
}

#endif /* defined(__libredisCluster__asynchirediscommand__) */
