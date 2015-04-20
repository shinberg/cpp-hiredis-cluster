#include <iostream>
#include <event2/event.h>
#include <signal.h>

#include "asynchirediscommand.h"

using namespace RedisCluster;
using namespace std;

// This error is demonstrating error handling
// Since error handling in way of try catch in setCallback function is not acceptable
// so you can set your error callback, which will be able to check the Exception
// type and command processing state and make a decision to write a log file and
// in some cases to retry send a command
// This examples shows how to do this

AsyncHiredisCommand<>::Action errorHandler(const AsyncHiredisCommand<> &cmd,
                                         const ClusterException &exception,
                                         HiredisProcess::processState state )
{
    AsyncHiredisCommand<>::Action action = AsyncHiredisCommand<>::FINISH;
    
    // Check the exception type, you can check any type of exceptions
    // This examples shows simple retry behaviour in case of exceptions is not
    // from criticals exceptions group
    if( dynamic_cast<const CriticalException*>(&exception) == NULL )
    {
        // here can be a log writing function
        cerr << "Exception in processing async redis callback: " << exception.what() << endl;
        cerr << "Retrying" << endl;
        // retry to send a command to redis node
        action = AsyncHiredisCommand<>::RETRY;
    }
    else
    {
        // here can be a log writing function
        cerr << "Critical exception in processing async redis callback: " << exception.what();
    }
    return action;
}

static void setCallback( typename Cluster<redisAsyncContext>::ptr_t cluster_p, void *r, void *data )
{
    redisReply * reply = static_cast<redisReply*>( r );
    string *demoData = static_cast<string*>( data );
    
    if( reply == NULL )
    {
        cerr << "Error: reply object is NULL" << endl;
    }
    else if( reply->type == REDIS_REPLY_STATUS  || reply->type == REDIS_REPLY_ERROR )
    {
        cout << " Reply to SET FOO BAR " << endl;
        cout << reply->str << endl;
    }
    
    cout << *demoData << endl;
    delete demoData;
    cluster_p->disconnect();
}

void processAsyncCommand()
{
    Cluster<redisAsyncContext>::ptr_t cluster_p;
    
    signal(SIGPIPE, SIG_IGN);
    struct event_base *base = event_base_new();
    string *demoData = new string("Demo data is ok");
    
    cluster_p = AsyncHiredisCommand<>::createCluster( "192.168.33.10", 7000, static_cast<void*>( base ) );
    
    AsyncHiredisCommand<> &cmd = AsyncHiredisCommand<>::Command( cluster_p,
                                 "FOO5",
                                 setCallback,
                                 static_cast<void*>( demoData ),
                                 "SET %s %s",
                                 "FOO",
                                 "BAR1" );
    
    // set error callback function
    cmd.setUserErrorCb( errorHandler );
    
    event_base_dispatch(base);
    delete cluster_p;
    event_base_free(base);
}

int main(int argc, const char * argv[])
{
    try
    {
        processAsyncCommand();
    } catch ( const RedisCluster::ClusterException &e )
    {
        cout << "Cluster exception: " << e.what() << endl;
    }
    return 0;
}

