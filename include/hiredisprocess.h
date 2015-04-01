//
//  hiredisprocess.h
//  libredisCluster
//
//  Created by Дмитрий on 11.03.15.
//  Copyright (c) 2015 shinberg. All rights reserved.
//

#ifndef __libredisCluster__hiredisprocess__
#define __libredisCluster__hiredisprocess__

#include <string>
#include "cluster.h"

extern "C"
{
#include <hiredis/hiredis.h>
}

namespace RedisCluster
{
    using std::string;
    
    class HiredisProcess
    {
    public:
        enum processState
        {
            MOVED,
            ASK,
            CLUSTERDOWN,
            READY
        };
        
        static void parsehostport( string error, string &host, string &port )
        {
            size_t hostPotision = error.find( " ", error.find( " " ) + 1) + 1;
            size_t portPosition = error.find( ":", hostPotision ) + 1;
            
            if( hostPotision != string::npos && portPosition != string::npos )
            {
                host = error.substr( hostPotision, portPosition - hostPotision - 1 );
                port = error.substr( portPosition );
            }
            else
            {
                throw LogicError("error while parsing host port in redis redirection reply");
            }
        }
        
        static processState processResult( redisReply* reply, string &result_host, string &result_port )
        {
            processState state = READY;
            if ( reply->type == REDIS_REPLY_ERROR )
            {
                string error ( reply->str, reply->len );
                
                if( error.find( "ASK" ) == 0 )
                {
                    parsehostport( error, result_host, result_port );
                    state = ASK;
                }
                else if ( error.find( "MOVED" ) == 0  )
                {
                    parsehostport( error , result_host, result_port );
                    state = MOVED;
                }
                else if ( error.find( "CLUSTERDOWN" ) == 0 )
                {
                    state = CLUSTERDOWN;
                }
                else
                {
                }
            }
            return state;
        }
        
        static void checkCritical( redisReply* reply, bool errorcritical, string error = "" )
        {
            if( reply == NULL )
                throw DisconnectedException();
            
            if( reply->type == REDIS_REPLY_ERROR )
            {
                if( errorcritical )
                {
                    freeReplyObject( reply );
                    throw LogicError( error );
                }
                else if( string(reply->str).find("CLUSTERDOWN") != string::npos )
                {
                    freeReplyObject( reply );
                    throw ClusterDownException();

                }
            }
        }
    };
}


#endif /* defined(__libredisCluster__hiredisprocess__) */
