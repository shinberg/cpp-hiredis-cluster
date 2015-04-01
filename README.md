# cpp-hiredis-cluster
c++ cluster wrapper for hiredis with async and unix sockets features

#Installing:
This is a header only library!
No need to install, just include headers in your project
If you have link errors for hiredis sds functions just wrap all hiredis headers in extern C in your project

#Features:
- redis cluster support (since redis 3 version)
- follow moved redirections
- follow ask redirections
- async hiredis functions are supported
- non-native (there is no native support) but very usefull support of clustering through unix sockets (see examples)
- understandable sources
