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

#include <assert.h>
#include <functional>  // for function<>
#include <iostream>

#include "adapters/adapter.h"  // for Adapter
#include "cluster.h"
#include "hiredisprocess.h"

extern "C"
{
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
}

namespace RedisCluster
{
    using std::string;
    
    // Asynchronous command class. Use Adapter to adapt different event library.
    template < typename Cluster = Cluster<redisAsyncContext> >
    class AsyncHiredisCommand
    {
        typedef redisAsyncContext Connection;

        struct ConnectContext {
            Adapter *adapter;
            typename Cluster::ptr_t pcluster;
            int lifetime;
        };
        
        AsyncHiredisCommand(const AsyncHiredisCommand&) = delete;
        AsyncHiredisCommand& operator=(const AsyncHiredisCommand&) = delete;
        
    public:
        
        enum Action
        {
            REDIRECT,
            FINISH,
            ASK,
            RETRY
        };
        
        typedef std::function<void (const redisReply& reply)> RedisCallback;
        typedef Action (userErrorCallbackFn)( const AsyncHiredisCommand<Cluster> &,
                                                      const ClusterException &,
                                                      HiredisProcess::processState );
        
        
        static inline AsyncHiredisCommand<Cluster>& Command(
            typename Cluster::ptr_t cluster_p,
            string key,  // todo: const
            int argc,
            const char ** argv,
            const size_t *argvlen,
            const RedisCallback& redisCallback = RedisCallback())
        {
            // would be deleted in redis reply callback or in case of error
            AsyncHiredisCommand<Cluster> *c = new AsyncHiredisCommand<Cluster>(
                cluster_p, key, argc, argv, argvlen, redisCallback );
            if( c->process() != REDIS_OK )
            {
                delete c;
                throw DisconnectedException();
            }
            return *c;
        }
        
        static inline AsyncHiredisCommand<Cluster>& Command(
            typename Cluster::ptr_t cluster_p,
            string key,
            const RedisCallback& redisCallback,
            const char *format, ... )
        {
            va_list ap;
            va_start(ap, format);
            // would be deleted in redis reply callback or in case of error
            AsyncHiredisCommand<Cluster> *c = new AsyncHiredisCommand<Cluster>(
                cluster_p, key, format, ap, redisCallback );
            if( c->process() != REDIS_OK )
            {
                delete c;
                throw DisconnectedException();
            }
            va_end(ap);
            return *c;
        }
        
        static inline AsyncHiredisCommand<Cluster>& Command(
            typename Cluster::ptr_t cluster_p,
            string key,
            const char *format, va_list ap,
            const RedisCallback& redisCallback = RedisCallback())
        {
            // would be deleted in redis reply callback or in case of error
            AsyncHiredisCommand<Cluster> *c = new AsyncHiredisCommand<Cluster>(
                cluster_p, key, format, ap, redisCallback );
            if( c->process() != REDIS_OK )
            {
                delete c;
                throw DisconnectedException();
            }
            return *c;
        }

        // Todo: Allow hosts
        static typename Cluster::ptr_t createCluster(
            const char* host,
            int port,
            Adapter& adapter,
            const struct timeval &timeout = { 3, 0 } )
        {
            typename Cluster::ptr_t cluster(NULL);
            redisReply *reply = nullptr;
            
            redisContext *con = redisConnectWithTimeout(host, port, timeout);
            if( con == NULL || con->err )
                throw ConnectionFailedException(nullptr);
            
            reply = static_cast<redisReply*>( redisCommand( con, Cluster::CmdInit() ) );
            HiredisProcess::checkCritical( reply, true );
            
            ConnectContext *cc = new ConnectContext({ &adapter, nullptr, 0});
            cluster = new Cluster(reply, connect, disconnect, (void*)cc, clusterDestructCB, static_cast<void*>(cc));
            cc->pcluster = cluster;
            
            freeReplyObject( reply );
            redisFree( con );
            
            return cluster;
        }
        
        inline void setUserErrorCb( userErrorCallbackFn *userErrorCb )
        {
            userErrorCb_ = userErrorCb;
        }
        
    protected:
        
        AsyncHiredisCommand( typename Cluster::ptr_t cluster_p,
            string key,
            int argc,
            const char ** argv,
            const size_t *argvlen,
            const RedisCallback& redisCallback = RedisCallback()) :
        cluster_p_( cluster_p ),
        redisCallback_( redisCallback ),
        userErrorCb_( NULL ),
        con_( {"",  NULL} ),
        key_( key ),
        cmd_{} {
            if(!cluster_p)
                throw InvalidArgument(nullptr);
            sds buf = nullptr;
            int len = redisFormatSdsCommandArgv(&buf, argc, argv, argvlen);
            cmd_ = string(static_cast<char*>(buf), len);
            sdsfree(buf);
        }
        
        AsyncHiredisCommand( typename Cluster::ptr_t cluster_p,
            string key,
            const char *format, va_list ap,
            const RedisCallback& redisCallback = RedisCallback()) :
        cluster_p_( cluster_p ),
        redisCallback_( redisCallback ),
        userErrorCb_( NULL ),
        con_( {"", NULL} ),
        key_( key ),
        cmd_{} {
            if(!cluster_p)
                throw InvalidArgument(nullptr);
            char * buf = nullptr;
            int len = redisvFormatCommand(&buf, format, ap);
            cmd_ = string(buf,len);
            free(buf);
        }
         
        ~AsyncHiredisCommand()
        {
            if( con_.second != NULL )
            {
                redisAsyncDisconnect( con_.second );
            }
        }
        
        static void clusterDestructCB(void *data) {
            ConnectContext *context = static_cast<ConnectContext*>(data);
            delete context;
        }
        
        static void disconnect(Connection *ac) {
            redisAsyncDisconnect( ac );
        }
        
        inline int process()
        {
            typename Cluster::SlotConnection con = cluster_p_->getConnection( key_ );
            return processHiredisCommand( con.second );
        }
        
        inline int processHiredisCommand( Connection* con )
        {
            return redisAsyncFormattedCommand( con, processCommandReply,
                static_cast<void*>( this ), cmd_.data(), cmd_.size() );
        }
        
        static void runRedisCallback( Connection* con, void *r, void *data )
        {
            redisReply *reply = static_cast<redisReply*>(r);
            AsyncHiredisCommand<Cluster>* that = static_cast<AsyncHiredisCommand<Cluster>*>( data );
            Action commandState = ASK;

            try
            {
                HiredisProcess::checkCritical(reply, false, false);
                if( reply->type == REDIS_REPLY_STATUS && !strcmp( reply->str, "OK" ) )
                {
                    if( that->processHiredisCommand( that->con_.second ) != REDIS_OK )
                    {
                        throw AskingFailedException(nullptr);
                    }
                }
                else
                {
                    throw AskingFailedException(nullptr);
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
                that->runRedisCallback( *reply );
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
            
            try {
                HiredisProcess::checkCritical( reply, false, false );
                state = HiredisProcess::processResult( reply, host, port);
                switch (state) {
                    case HiredisProcess::ASK:
                        if( that->con_.second == NULL )
                            that->con_ = that->cluster_p_->createNewConnection( host, port );
                        if ( redisAsyncCommand( that->con_.second, runRedisCallback, that, "ASKING" ) == REDIS_OK )
                            commandState = ASK;
                        else
                            throw AskingFailedException(nullptr);
                        break;
                    case HiredisProcess::MOVED:
                        that->cluster_p_->moved();
                        if( that->con_.second == NULL )
                            that->con_ = that->cluster_p_->createNewConnection( host, port );
                        if( that->processHiredisCommand( that->con_.second ) == REDIS_OK )
                            commandState = REDIRECT;
                        else
                            throw MovedFailedException(nullptr);
                        break;
                    case HiredisProcess::READY:
                        break;
                    case HiredisProcess::CLUSTERDOWN:
                        throw ClusterDownException(nullptr);

                    default:
                        throw LogicError(nullptr);
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
                that->runRedisCallback( *reply );
                if( !( con->c.flags & ( REDIS_SUBSCRIBED ) ) )
                    delete that;
            }
        }
        
        static void retry( Connection *con, void *r, void *data )
        {
            AsyncHiredisCommand<Cluster>* that = static_cast<AsyncHiredisCommand<Cluster>*>( data );
            
            if( that->processHiredisCommand( con ) != REDIS_OK )
            {
                // XXX check NULL
                that->userErrorCb_( *that, DisconnectedException(), HiredisProcess::FAILED );
                assert(r);
                redisReply *reply = static_cast< redisReply* >(r);
                that->runRedisCallback( *reply );
                delete that;
            }
        }
        
        static void disconnectCb(const struct redisAsyncContext*ctx, int /*status*/) {
            ConnectContext *context = static_cast<ConnectContext*>(ctx->data);
            context->lifetime--;
            context->pcluster->deleteConnection(ctx);
        }
        
        static Connection* connect( const char* host, int port, void *data )
        {
            ConnectContext *context = static_cast<ConnectContext*>(data);
            if ( context == NULL || context->adapter == NULL )
                throw ConnectionFailedException(nullptr);

            Connection *con = redisAsyncConnect( host, port );
            if( con == NULL || con->err != 0 ||
                context->adapter->attachContext( *con ) != REDIS_OK )
                throw ConnectionFailedException(nullptr);

            context->lifetime++;
            con->data = static_cast<void*>(context);
            redisAsyncSetDisconnectCallback(con, disconnectCb);
            return con;
        }

    private:
        void runRedisCallback( const redisReply& reply ) const
        {
            if (redisCallback_)
                redisCallback_( reply );
        }

    private:
        // pointer to shared cluster object ( cluster class is not threadsafe )
        typename Cluster::ptr_t cluster_p_;
        
        // user-defined callback to redis async command
        RedisCallback redisCallback_;
        // user error handler
        userErrorCallbackFn *userErrorCb_;
        
        // pointer to async context ( in case of redirection class creates new connection )
        typename Cluster::HostConnection con_;

        // key of redis command to find proper cluster node
        string key_;
        string cmd_;
    };
}

#endif /* defined(__libredisCluster__asynchirediscommand__) */
