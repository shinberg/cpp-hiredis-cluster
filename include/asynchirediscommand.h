//
//  asynchirediscommand.h
//  libredisCluster
//

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
    class AsyncHiredisCommand : public NonCopyable
    {
        enum CommandType
        {
            SDS,
            FORMATTED_STRING
        };
        
        enum ProcessingState
        {
            REDIRECTING,
            FINISHED,
            ASKING
        };
        
    public:
        
        typedef void (*pt2AsyncAttachFn)( redisAsyncContext *, void * );
        typedef void (redisCallbackFn)( typename Cluster<redisAsyncContext>::ptr_t cluster_p, void*, void* );
        
        static inline void Command( typename Cluster<redisAsyncContext>::ptr_t cluster_p,
                                        string key,
                                        redisCallbackFn userCb,
                                        void *userPrivData,
                                        int argc,
                                        const char ** argv,
                                        const size_t *argvlen )
        {
            // would be deleted in redis reply callback
            (new AsyncHiredisCommand( cluster_p, key, userCb, userPrivData, argc, argv, argvlen ))->process();
        }
        
        static inline void Command( typename Cluster<redisAsyncContext>::ptr_t cluster_p,
                                        string key,
                                        redisCallbackFn userCb,
                                        void *userPrivData,
                                        const char *format, ... )
        {
            va_list ap;
            va_start(ap, format);
            // would be deleted in redis reply callback
            (new AsyncHiredisCommand( cluster_p, key, userCb, userPrivData, format, ap ))->process();
            va_end(ap);
        }

        static Cluster<redisAsyncContext>::ptr_t createCluster( const char* host,
                                                                int port,
                                                                void* data = NULL,
                                                                Cluster<redisAsyncContext>::pt2RedisConnectFunc conn = libeventConnect,
                                                                Cluster<redisAsyncContext>::pt2RedisFreeFunc free = Disconnect,
                                                                const struct timeval &timeout = { 3, 0 } )
        {
            Cluster<redisAsyncContext>::ptr_t cluster(NULL);
            redisReply *reply;
            
            redisContext *con = redisConnectWithTimeout( host, port, timeout );
            if( con == NULL || con->err )
                throw ConnectionFailedException();
            
            reply = static_cast<redisReply*>( redisCommand( con, Cluster<redisAsyncContext>::CmdInit() ) );
            HiredisProcess::checkCritical( reply, true );

            cluster = new Cluster<redisAsyncContext>( reply, conn, free, data );
            
            freeReplyObject( reply );
            redisFree( con );
            return cluster;
        }
        
    protected:
        
        static void Disconnect(redisAsyncContext *ac)
        {
            redisAsyncDisconnect( ac );
        }
        
        AsyncHiredisCommand( typename Cluster<redisAsyncContext>::ptr_t cluster_p,
                            string key,
                            redisCallbackFn userCb,
                            void *userPrivData,
                            int argc,
                            const char ** argv,
                            const size_t *argvlen ) :
        cluster_p_( cluster_p ),
        userCallback_p_( userCb ),
        userPrivData_( userPrivData ),
        con_( NULL ),
        key_( key ),
        cmd_(NULL),
        type_( SDS )
        {
            // TODO: check it and check for correct distruction
            if( cluster_p == NULL )
                throw InvalidArgument();
            
            len_ = redisFormatSdsCommandArgv(&cmd_, argc, argv, argvlen);
        }
        
        AsyncHiredisCommand( typename Cluster<redisAsyncContext>::ptr_t cluster_p,
                            string key,
                            redisCallbackFn userCb,
                            void *userPrivData,
                            const char *format, va_list ap ) :

        cluster_p_( cluster_p ),
        userCallback_p_( userCb ),
        userPrivData_( userPrivData ),
        con_( NULL ),
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
            if( con_ != NULL )
            {
                redisAsyncDisconnect( con_ );
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
        
        inline void process()
        {
            redisAsyncContext *con = cluster_p_->getConnection( key_ );
            processHiredisCommand( con );
        }
        
        inline void processHiredisCommand( redisAsyncContext *con )
        {
            redisAsyncFormattedCommand( con, processCommandReply, static_cast<void*>( this ), cmd_, len_ );
        }
        
        static void askingCallback( redisAsyncContext *con, void *r, void *data )
        {
            redisReply *reply = static_cast<redisReply*>( r );
            AsyncHiredisCommand* that = static_cast<AsyncHiredisCommand*>( data );

            bool critical = false;
            try
            {
                HiredisProcess::checkCritical( reply, false );
            }
            catch ( const ClusterException &ce )
            {
                critical = true;
            }
            
            if( !critical && reply->type == REDIS_REPLY_STATUS && string(reply->str) == "OK" )
            {
                std::cout << "asking ok" << std::endl;
                that->processHiredisCommand( that->con_ );
            }
            else
            {
                // TODO: invoke user specified error handler
                that->userCallback_p_( that->cluster_p_, r, that->userPrivData_ );
                delete that;
            }
        }
        
        static void processCommandReply( redisAsyncContext *con, void *r, void *data )
        {
            redisReply *reply = static_cast< redisReply* >(r);
            AsyncHiredisCommand* that = static_cast<AsyncHiredisCommand*>( data );
            ProcessingState commandState = FINISHED;
            string host,port;
            
            bool critical = false;
            try
            {
                HiredisProcess::checkCritical( reply, false );
            }
            catch ( const ClusterException &ce )
            {
                critical = true;
            }
            
            if( !critical )
            {
                HiredisProcess::processState state = HiredisProcess::processResult( reply, host, port);
            
                switch ( state ) {
                        
                    case HiredisProcess::ASK:
                        // TODO: invoke user defined redirection handler
                        try
                        {
                            that->con_ = that->cluster_p_->createNewConnection( host, port );
                        } catch ( const ClusterException &e )
                        {
                            that->con_ = NULL;
                        }

                        if( that->con_ != NULL )
                        {
                            redisAsyncCommand( that->con_, askingCallback, that, "ASKING" );
                            commandState = ASKING;
                        }
                        break;
                        
                        
                    case HiredisProcess::MOVED:
                        // TODO: invoke user defined redirection handler
                        try
                        {
                            that->con_ = that->cluster_p_->createNewConnection( host, port );
                        } catch ( const ClusterException &e )
                        {
                            that->con_ = NULL;
                        }
                        
                        if( that->con_ != NULL )
                        {
                            that->processHiredisCommand( that->con_ );
                            that->cluster_p_->moved();
                            commandState = REDIRECTING;
                        }
                        break;
                        
                    case HiredisProcess::READY:
                        break;
                        
                    case HiredisProcess::CLUSTERDOWN:
                        // TODO: invoke user defined error handling function
                        break;
                        
                    default:
                        // Logic Error
                        // TODO: invoke user defined error handling function
                        break;
                }
            }
            
            if( commandState == FINISHED )
            {
                that->userCallback_p_( that->cluster_p_, r, that->userPrivData_ );
                delete that;
            }
        }
            
        static redisAsyncContext* libeventConnect( const char* host, int port, void *data )
        {
            redisAsyncContext *con = NULL;
            event_base *evbase = static_cast<event_base*>( data );
            
            if( evbase != NULL )
            {
                con = redisAsyncConnect( host, port );
            
                if( con != NULL && con->err == 0 )
                {
                    redisLibeventAttach( con, evbase );
                }
                else
                {
                    redisAsyncFree( con );
                    throw ConnectionFailedException();
                }
            }
            return con;
        }
        
        // pointer to shared cluster object ( cluster class is not threadsafe )
        typename Cluster<redisAsyncContext>::ptr_t cluster_p_;
        
        // user-defined callback to redis async command as usual
        redisCallbackFn *userCallback_p_;
        // user-defined callback data
        void* userPrivData_;
        
        // pointer to async context ( in case of redirection class creates new connection )
        redisAsyncContext *con_;

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
