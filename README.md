[![Build Status](https://api.travis-ci.org/shinberg/cpp-hiredis-cluster.png)](https://travis-ci.org/shinberg/cpp-hiredis-cluster/)

# cpp-hiredis-cluster
c++ cluster wrapper for hiredis with async and unix sockets features
## Features:
- redis cluster support
- async hiredis functions are supported
- support of clustering through unix sockets (see examples)
- threaded safe connection pool support
- maximum hiredis compliance in functions invocations (easy to migrate from existing hiredis source code)
- follow moved redirections
- follow ask redirections
- understandable sources
- best performance (see performance test result [here](https://github.com/shinberg/cpp-hiredis-cluster/wiki/Performance))

## Dependencies:
* library "hiredis" versioned >= 0.12.0
* installed redis server versioned >= 3.0.0
* configured cluster, see [cluster tutorial](http://redis.io/topics/cluster-tutorial/) on how to setup cluster
* its better for you to know about "moved" and "asking" redirections [clusterspec](http://redis.io/topics/cluster-spec) (not necessary for quick start)
* libevent-2.0 (only if you choose asynchronous client, see "Asynchronous client example"), the latest stable version is [libevent-2.0.22](https://github.com/libevent/libevent/tree/release-2.0.22-stable)

## Examples

### Synchronous client example

~~~c++
    // Declare cluster pointer
    Cluster<redisContext>::ptr_t cluster_p;
    // Declare pointer to simple hiredis reply structure
    redisReply * reply;
    // Create cluster passing acceptable address and port of one node of the cluster nodes 
    cluster_p = HiredisCommand<>::createCluster( "127.0.0.1", 7000 );
    // send command to redis passing created cluster pointer, key which you wish to access in the command
    // and command itself with parameters with printf like syntax
    reply = static_cast<redisReply*>( HiredisCommand<>::Command( cluster_p, "FOO", "SET %s %s", "FOO", "BAR1" ) );
    // Check reply state and type
    if( reply->type == REDIS_REPLY_STATUS  || reply->type == REDIS_REPLY_ERROR )
    {
       // Process reply
        cout << " Reply to SET FOO BAR " << endl;
        cout << reply->str << endl;
    }
    // free hiredis reply structure
    freeReplyObject( reply );
    // delete cluster by its pointer
    delete cluster_p;
~~~
> source code is available in src/examples/example.cpp

### Asynchronous client example

~~~c++
// declare a callback to process reply to redis command
static void setCallback( typename Cluster<redisAsyncContext>::ptr_t cluster_p, void *r, void *data )
{
    // declare local pointer to work with reply
    redisReply * reply = static_cast<redisReply*>( r );
    // cast data that you pass as callback parameter below (not necessary)
    string *demoData = static_cast<string*>( data );
    // check redis reply usual
    if( reply->type == REDIS_REPLY_STATUS  || reply->type == REDIS_REPLY_ERROR )
    {
        // process reply
        cout << " Reply to SET FOO BAR " << endl;
        cout << reply->str << endl;
    }
    // process callback parameter if you want (not necessary)
    cout << *demoData << endl;
    delete demoData;
    // disconnecting cluster will brake the event loop
    cluster_p->disconnect();
}
// declare a functions that invokes redis commanf
void processAsyncCommand()
{
    // Declare cluster pointer with redisAsyncContext as template parameter
    Cluster<redisAsyncContext>::ptr_t cluster_p;
    // ignore sigpipe as we use libevent
    signal(SIGPIPE, SIG_IGN);
    // create libevent base
    struct event_base *base = event_base_new();
    // create custom data that will be passed to callback (not necessary)
    string *demoData = new string("Demo data is ok");
    // Create cluster passing acceptable address and port of one node of the cluster nodes
    cluster_p = AsyncHiredisCommand<>::createCluster( "127.0.0.1", 7000, static_cast<void*>( base ) );
    // send command to redis passing created cluster pointer, key which you wish to access in the command
    // callback function, that just already declared above, pointer to any user defined data
    // and command itself with parameters with printf like syntax
    AsyncHiredisCommand<>::Command( cluster_p,                      // cluster pointer
                                     "FOO",                             // key accessed in current command
                                 setCallback,                       // callback to process reply
                                 static_cast<void*>( demoData ),    // custom user data pointer
                                 "SET %s %s",                       // command
                                 "FOO",                             // paramener - key
                                 "BAR1" );                          // parameter - value
    // process event loop
    event_base_dispatch(base);
    // delete cluster object
    delete cluster_p;
    // free event base
    event_base_free(base);
}
~~~
> source code is available in src/examples/asyncexample.cpp

> it's easy to modify asynchronous client for use with another event loop libraries

### Other examples

* example showing how to create a threaded connection pool (src/examples/threadpool.cpp)
* example showing how to user unix sockets (src/examples/unixsocketexample.cpp)
* example showing how to process errors in case of asynchronous operation (src/examples/asyncerrorshandling.cpp)

## Installing:
* This is a header only library! No need to install, just include headers in your project
* Run cmake if you want to build examples

### Issuses:
* If you have link errors for hiredis sds functions just wrap all hiredis headers in extern C in your project

* If you used this library before  note that usage of AsyncHiredisCommand and HiredisCommand little. AsyncHiredisCommand and HiredisCommand change to AsyncHiredisCommand<> HiredisCommand<>
~~~c++
AsyncHiredisCommand -> AsyncHiredisCommand<>
HiredisCommand -> HiredisCommand<>
~~~


#Mail
shinmail at gmail dot com
