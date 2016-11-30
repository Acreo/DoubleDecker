# DoubleDecker messaging system
[![Build Status](https://travis-ci.org/Acreo/DoubleDecker.svg?branch=master)](https://travis-ci.org/Acreo/DoubleDecker)
DoubleDecker is a hierarchical distributed message system based on ZeroMQ 
which can be used to provide messaging between processes running on a 
single machine and between processes running on multiple machines. 
It is hierarchical in the sense that message brokers are connected to 
each-other in a tree topology and route messages upwards in case they don't 
have the destination client beneath themselves.

DoubleDecker currently supports two types of messaging, Notifications, i.e. point-to-point messages from one client to another, and Pub/Sub on a topic. The Pub/Sub mechanism furthermore allows scoping when subscribing to a topic. This means that a client can restrict the subscription to messages published only within a certain scope, such as to clients connected to the same broker, a specific broker, or different groups of brokers.

DoubleDecker currently supports multiple tenants by authenticating clients using public/private keys, encrypting messages, and enforcing that messages cannot cross from one tenant to another. Additionally there is a special tenant called 'public' that can cross tenant boundaries. This can be used in order to connect clients that are intended to provide a public service, such as a registration service for all tenants, a name-lookup service, or similar. 

This initial release contains only the Python version of the DoubleDecker broker and client library (and a demo client). A version in C is planned to be released shortly. 

This work is carried out within the [UNIFY FP7 EU project](http://www.fp7-unify.eu/). For more information about the purpose and how we use DoubleDecker, see:
* [UNIFY Deliverable 4.2, Section 5.1](http://fp7-unify.eu/files/fp7-unify-eu-docs/UNIFY-WP4-D4.2%20Proposal%20for%20SP-DevOps%20network%20capabilities%20and%20tools.pdf)
* Paper and presentation at the EWSDN'15 conference, see:
  * [Scalable Software-Defined Monitoring for Service Provider DevOps](http://www.ewsdn.eu/files/Documents/EWSDN2015/04_03.pdf)
* Demonstratoions validating the functionality
  1. Demo at IFIP/IEEE IM'15
  2. Paper and Demo at EWSDN'15:
     "Monitoring Transport and Cloud for Network Functions Virtualization"

### C version ; broker and client


Dependencies: 

 * libzmq (libzmq3-dev on ubuntu)
 * libczmq (git clone git://github.com/zeromq/czmq) 
 * userspace-rcu + hashtable (in userspace-rcu folder)
 * libsodium (https://github.com/jedisct1/libsodium) 
  	     Needs to be above version 1.0.3,  supporting precomputed keys and sodium_increment()
 *  **For details on how to install these dependencies, see the Dockerfile in /docker or /.travis.yml 

To build:

```bash
./boot.sh
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
These command line options can also be provided in a configuration file, see br0.cfg.

To run a client:
 Demo client using the library is provided as ```ddclient -c <customer> -n <name> -k <keyfile> -d <dealer>```

### Python version ; client only

Has now moved to its [own Python repository](https://github.com/Acreo/DoubleDecker-py)

### Java version ; client only

Has now moved to its [own Java repository](https://github.com/Acreo/DoubleDecker-java)

### LICENSE

DoubleDecker is licensed under LGPLv2, for more details see LICENSE.

 - The file 'src/trie.c' is from [nanomsg](http://nanomsg.org/), licensed under MIT/X11 license.
 - The file 'src/cli_parser/*' are from [cli_parser](http://sourceforge.net/projects/cliparser/), licensed under a modified BSD license.
 - The file 'src/hash/xxhash.c' is from [xxHash](http://sourceforge.net/projects/cliparser/) by Yann Collet, licensed under BSD 2-clause license.
 - The file 'src/hash/murmurhash.c' is from [murmurhash.c](https://github.com/jwerle/murmurhash.c) by Joseph Werle, licensed under MIT license.
 - The file 'src/lib/b64/cdecode.c' is from the [libb64 project](http://sourceforge.net/projects/libb64/) by Chris Venter, placed in the public domain.


