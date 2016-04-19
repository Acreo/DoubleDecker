C implementation of DoubleDecker broker and clients
=====================================

Dependencies: 

 * libzmq (libzmq3-dev on ubuntu)
 * libczmq (git clone git://github.com/zeromq/czmq) 
 * userspace-rcu + hashtable (in userspace-rcu folder)
 * libsodium (https://github.com/jedisct1/libsodium) 
  	     Needs to be above version 1.0.3,  supporting precomputed keys and sodium_increment()
 *  **For details on how to install these dependencies, see the Dockerfile in /docker/**                                              

To build:

```bash
mkdir -pv m4
autoreconf --force --install
./configure
make
```

Running
-------

First generate tenant, public, and broker keys using the ddkeys.py script (in the python subdirectory).

To start a single-level broker:
```ddbroker -r <tcp://.. or ipc://file> -k <broker-keys> -s <scope>```
For running a set of interconnected, hierarchical, multi-level brokers, connect one broker to another as a client by
```Adding the  -d <tcp:// , ipc://> option ``` 
 Note that all brokers in the hierarchy must use the same broker-keys file in order to authenticate with each other. 

To run a client:
 Demo client using the library is provided as ```ddclient -c <customer> -n <name> -k <keyfile> -d <dealer>```
