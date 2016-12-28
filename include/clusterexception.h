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

#ifndef __libredisCluster__clusterexception__
#define __libredisCluster__clusterexception__

#include <stdexcept>
#include <string.h>

namespace RedisCluster {
    using std::string;

    // Base class of all cluster library exceptions
    // so you are able to catch only that type of exceptions
    class ClusterException : public std::runtime_error {
    protected:
        ClusterException(redisReply *reply, const std::string &text) : runtime_error(text) {
            if (reply)
                freeReplyObject(reply);
        }
    };

    // Base class of exceptions group meaning that you can't send
    // request with this cluster at all and cluster need to be reinitialized
    class CriticalException : public ClusterException {
    protected:
        CriticalException(redisReply *reply, const std::string &text) : ClusterException(reply, text) {}
    };

    // Base class of exceptions group meaning that cluster better to be reinitialized
    // otherwize it could not be able to complete some requests
    class BadStateException : public ClusterException {
    protected:
        BadStateException(redisReply *reply, const std::string &text) : ClusterException(reply, text) {}
    };

    class AskingFailedException : public BadStateException {
    public:
        AskingFailedException(redisReply *reply) : BadStateException(reply, std::string(
                "error while processing asking command")) {}
    };

    class MovedFailedException : public BadStateException {
    public:
        MovedFailedException(redisReply *reply) : BadStateException(reply, std::string(
                "error while processing asking command")) {}
    };

    class ConnectionFailedException : public CriticalException {
    public:
        ConnectionFailedException(redisReply *reply) : CriticalException(reply,
                                                        std::string("cluster connect failed: ") + strerror(errno)) {}
    };

    class DisconnectedException : public CriticalException {
    public:
        std::string reportedError;

        DisconnectedException() : CriticalException(nullptr, std::string("cluster host disconnected")) {}

        DisconnectedException(const std::string &reportedError) :
                CriticalException(nullptr, std::string("cluster host disconnected")) {
            this->reportedError = reportedError;
        }
    };

    class NodeSearchException : public BadStateException {
    public:
        NodeSearchException() : BadStateException(nullptr, std::string("node not found in cluster")) {}
    };

    class NotInitializedException : public CriticalException {
    public:
        NotInitializedException() : CriticalException(nullptr,
                                                      std::string("cluster have not been properly initialized")) {}
    };

    class ClusterDownException : public CriticalException {
    public:
        ClusterDownException(redisReply *reply) : CriticalException(reply, std::string("cluster is going down")) {}
    };

    class LogicError : public BadStateException {
    public:
        LogicError(redisReply *reply) : BadStateException(reply, std::string("cluster logic error")) {}

        LogicError(redisReply *reply, string reason) : BadStateException(reply, reason) {}
    };

    // exception meaning that you had not properly passed arguments cluster or command invocation
    class InvalidArgument : public ClusterException {
    public:
        InvalidArgument(redisReply *reply) : ClusterException(reply, std::string("cluster invalid argument")) {}
    };
}

#endif // defined(__libredisCluster__clusterexception__)