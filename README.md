Double Decker messaging system
==============================
Double Decker is an hierarchical distributed message system based on ZeroMQ 
which can be used to provide messaging between processes running on a 
single machine and between processes running on multiple machines. 
It is hierarchcial in the sense that message brokers are connected to 
each-other in a tree topology and route messages upwards in case they don't 
have the destination client beneath themselves.

his work is carried out within the UNIFY FP7 EU project (http://www.fp7-unify.eu/)

CLI commands:
  * In the broker:
	reg     - prints registered clients and brokers
	topics  - print registered topics
        exit    - shut down the broker

  * In the clients:
	help                            - show the help
        send        client_id message   - send an encrypted message to client
        sendPT      client_id message   - send a plain text message to client
        sendpublic  public_client_id message - send message to a public client
        pub         topic message       - publish message on topic
        pubpublic   topic message       - publish message on public topic
        sub         topic               - subscribe to messages in topic
        unsub       topic               - subscribe to messages in topic
        exit                            - unregister and exit

INSTALL
Double Decker requires Python > 3.3, setuptools and aiozmq to install. To install in Ubuntu 14.04:

	apt-get install python3-setuptools python3-pip python3-nacl
	pip3 install aiozmq
	git clone http://gitlab.fp7-unify.eu/Pontus.Skoeldstroem/double-decker.git
	cd double-decker
	python3 setup.py install

