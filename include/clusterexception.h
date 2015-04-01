#include <stdexcept>

namespace RedisCluster
{
    using std::string;
    
    // Base class of all cluster library exceptions
    // so you are able to catch only that type of exceptions
    class ClusterException : public std::runtime_error {
    public:
        ClusterException( const std::string &text) : runtime_error( text )
        {}
    };
    
    // Base class of exceptions group meaning that you can't send
    // request with this cluster at all and cluster need to be reinitialized
    class CriticalException : public ClusterException {
        public:
        CriticalException( const std::string &text) : ClusterException( text )
        {}
    };
    
    // Base class of exceptions group meaning that cluster better to be reinitialized
    // otherwize it could not be able to complete some requests
    class BadStateException : public ClusterException {
        public:
        BadStateException( const std::string &text) : ClusterException( text )
        {}
    };
    
    class ConnectionFailedException : public CriticalException {
    public:
        ConnectionFailedException() : CriticalException( std::string("cluster connect failed: ") + strerror(errno) )
        {}
    };
    
    class DisconnectedException : public CriticalException {
    public:
        DisconnectedException() : CriticalException( std::string("cluster host disconnected") )
        {}
    };
    
    class NodeSearchException : public BadStateException {
    public:
        NodeSearchException() : BadStateException( std::string("node not found in cluster") )
        {}
    };
    
    class NotInitializedException : public CriticalException {
    public:
        NotInitializedException() : CriticalException( std::string("cluster have not been properly initialized") )
        {}
    };
    
    class ClusterDownException : public CriticalException {
    public:
        ClusterDownException() : CriticalException( std::string("cluster is going down") )
        {}
    };
    
    class LogicError : public BadStateException {
    public:
        LogicError() : BadStateException( std::string("cluster logic error") )
        {}
        LogicError( string reason ) : BadStateException( reason )
        {}
    };
    
    // exception meaning that you had not properly passed arguments cluster or command invocation
    class InvalidArgument : public ClusterException {
    public:
        InvalidArgument() : ClusterException( std::string("cluster invalid argument") )
        {}
    };
}
