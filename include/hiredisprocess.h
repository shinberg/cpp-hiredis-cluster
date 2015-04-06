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
            READY,
            FAILED
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
