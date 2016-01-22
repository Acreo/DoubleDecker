C version of Double Decker broker and clients

Dependencies: 
   libzmq (libzmq3-dev on ubuntu)
   libczmq (git clone git://github.com/zeromq/czmq) 
   userspace-rcu + hashtable (in userspace-rcu folder)
   libsodium (https://github.com/jedisct1/libsodium) 
   	     Needs to be above version 1.0.3 
             (supporting precomputing keys and sodium_increment())
  For details on how to install them, see the Dockerfile in /docker/                                              


To build:

```bash
mkdir -pv m4
autoreconf --force --install
./configure
make
```

Running
#######
Generate tenant, public and broker keys using the ddkeys.py script.
Broker:
   Single-level broker:
   ddbroker -r <tcp://.. or ipc://file> -k <broker-keys> -s <scope> 
   Multi-level brokers:
   Add the  -d <tcp:// , ipc://> option to connect to a higher level broker.
   All brokers in the hierarchy must use the same broker-keys file in order
   to authenticate with each other. 

Client:
  Demo client using the library is provided as 'ddclient'

