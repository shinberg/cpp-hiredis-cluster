//
//  command.h
//  libredisCluster
//
//  Created by Дмитрий on 11.03.15.
//  Copyright (c) 2015 shinberg. All rights reserved.
//

#ifndef __libredisCluster__command__
#define __libredisCluster__command__

#include <iostream>
#include "cluster.h"
#include "hiredisprocess.h"

extern "C"
{
#include "hiredis/hiredis.h"
}

namespace RedisCluster
{
    using std::string;
    class HiredisCommand : public NonCopyable
    {
        enum CommandType
        {
            SDS,
            FORMATTED_STRING
        };
        
    public:
        
        static Cluster<redisContext>::ptr_t createCluster(const char* host,
                                                          int port,
                                                          void* data = NULL,
                                                          Cluster<redisContext>::pt2RedisConnectFunc conn = ConnectFunction,
                                                          Cluster<redisContext>::pt2RedisFreeFunc free = redisFree,
                                                          const struct timeval &timeout = { 3, 0 } )
        {
            Cluster<redisContext>::ptr_t cluster(NULL);
            redisReply *reply;
            
            redisContext *con = redisConnectWithTimeout( host, port, timeout );
            if( con == NULL || con->err )
                throw ConnectionFailedException();
            
            reply = static_cast<redisReply*>( redisCommand( con, Cluster<redisContext>::CmdInit() ) );
            HiredisProcess::checkCritical( reply, true );

            cluster = new Cluster<redisContext>( reply, conn, free, data );
            
            freeReplyObject( reply );
            redisFree( con );
            return cluster;
        }
        
        static inline void* Command( typename Cluster<redisContext>::ptr_t cluster_p,
                                   string key,
                                   int argc,
                                   const char ** argv,
                                   const size_t *argvlen )
        {
            return HiredisCommand( cluster_p, key, argc, argv, argvlen ).process();
        }
        
        static inline void* Command( typename Cluster<redisContext>::ptr_t cluster_p,
                                   string key,
                                   const char *format, ...)
        {
            va_list ap;
            va_start( ap, format );
            return HiredisCommand( cluster_p, key, format, ap ).process();
            va_end(ap);
        }
        

        
    protected:
        
        HiredisCommand( typename Cluster<redisContext>::ptr_t cluster_p,
                       string key,
                       int argc,
                       const char ** argv,
                       const size_t *argvlen ) :
        cluster_p_( cluster_p ),
        key_( key ),
        type_( SDS )
        {
            if( cluster_p == NULL )
                throw InvalidArgument();
            
            len_ = redisFormatSdsCommandArgv(&cmd_, argc, argv, argvlen);
        }
        
        HiredisCommand( typename Cluster<redisContext>::ptr_t cluster_p,
                       string key,
                       const char *format, va_list ap ) :
        cluster_p_( cluster_p ),
        key_( key ),
        type_( FORMATTED_STRING )
        {
            if( cluster_p == NULL )
                throw InvalidArgument();

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
        
        redisReply* processHiredisCommand( redisContext *con )
        {
            redisReply* reply;
            redisAppendFormattedCommand( con, cmd_, len_ );
            redisGetReply( con, (void**)&reply );
            return reply;
        }
        
        redisReply* asking( redisContext *con  )
        {
            return static_cast<redisReply*>( redisCommand( con, "ASKING" ) );
        }
        
        void* process()
        {
            redisReply *reply;
            redisContext *con = cluster_p_->getConnection( key_ );
            string host, port;
            
            reply = processHiredisCommand( con );
            HiredisProcess::checkCritical( reply, false );
            HiredisProcess::processState state = HiredisProcess::processResult( reply, host, port);
            
            switch ( state ) {
                case HiredisProcess::ASK:
                    
                    freeReplyObject( reply );
                    con = cluster_p_->createNewConnection( host, port );
                    reply = asking( con );
                    HiredisProcess::checkCritical( reply, true, "asking error" );
                    
                    freeReplyObject( reply );
                    reply = processHiredisCommand( con );
                    HiredisProcess::checkCritical( reply, false );
                    
                    redisFree( con );
                    
                    break;
                    
                case HiredisProcess::MOVED:
                    
                    freeReplyObject( reply );
                    con = cluster_p_->createNewConnection( host, port );
                    reply = processHiredisCommand( con );
                    
                    redisFree( con );
                    cluster_p_->moved();
                    break;
                case HiredisProcess::READY:
                    break;
                default:
                    throw LogicError( "error in state processing" );
                    break;
            }
            return reply;
        }
        
        static redisContext* ConnectFunction( const char* host, int port, void * )
        {
            return redisConnect( host, port);
        }
        
        typename Cluster<redisContext>::ptr_t cluster_p_;
        string key_;
        char *cmd_;
        int len_;
        CommandType type_;
    };
}

#endif /* defined(__libredisCluster__command__) */
