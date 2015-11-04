# DoubleDecker messaging system
DoubleDecker is an hierarchical distributed message system based on ZeroMQ 
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



### INSTALLATION
DoubleDecker requires Python > 3.3. To install on Ubuntu 15.10, run the install.sh script which performs these actions: 
```bash
#install dependencies 
apt-get update
apt-get install python3-setuptools python3-nacl python3-zmq python3-urwid python3-tornado git
# clone the code
git clone https://github.com/Acreo/DoubleDecker.git
# install the doubledecker module and scripts
cd DoubleDecker/python
sudo python3 setup.py install
# generate public/private keys
cd /etc
mkdir doubledecker
cd doubledecker
# create keys for 4 tenants, public, tenant a, b, and c
ddkeys.py (input "a,b,c")
```

### USAGE
```bash
# start a broker listening for TCP connections on *:5555, using the keys
# from broker-keys.json, called 'broker0'
ddbroker.py -r tcp://*:5555 -k /etc/doubledecker/broker-keys.json broker0
# start a client from tentant A, called cli1, connect to broker0
ddclient.py -d tcp://127.0.0.1:5555 -k /etc/doubledecker/a-keys.json cli1 a
# start a second client tentant A, called cli2, connect to broker0
ddclient.py -d tcp://127.0.0.1:5555 -k /etc/doubledecker/a-keys.json cli2 a
# now you can use the CLI interface to send notifications
# from cli1 to cli2 or to subscribe and publish messages
# to send a notification from cli1 to cli2
Send Notification -> Destination: cli2 -> Message: hello -> Notify
should result in message "hello" appearing at cli2
# to subscribe to topic "alarms"
Subscribe to topic -> Topic: alarms -> Scope: region -> Subscribe
# to publish on topic alarms
Publish message -> Topic: alarms -> Message: warning -> Publish
```

### LICENSE

DoubleDecker is licensed under LGPLv2, for more details see COPYING.LESSER.

The file 'python/doubledecker/trie.py' is from the [Google PyTrie module](https://github.com/google/pytrie), licensed under version 2.0 of the Apache License.


