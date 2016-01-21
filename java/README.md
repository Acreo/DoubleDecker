# Java client for DoubleDecker version 3


Initial Java client for DD version 3 (0x0d0d0003). Compile the testDD client using maven:

```
$ mvn compile
```

## Running the test client

To run the client using the maven exec plugin:

```
$ mvn exec:java 
```
This should start the CLI interface where you can use "?list", "?list-all", and  "?help" to see the available commands.

```
DD> ?list
abbrev	name	params
s	subscriptions	()               -- See subscriptions and status
p	publish	(Topic, Message)         -- Publish a message
n	notify	(Client, Message)        -- Send a notification
st	status	()                       -- See connectivity status
c	connect	(BrokerURL, ClientName)  -- Connect to a broker
u	unsubscribe	(Topic, Scope)       -- Unsubscribe from a topic
s	subscribe	(Topic, Scope)       -- Subscribe to a topic
q   quit    ()                       -- Gracefully shutdown the client 
```


To run a broker, compile the C version of the broker (https://github.com/Acreo/DoubleDecker/tree/master/c) and start it
in a terminal 

```
$ ddbroker -r tcp://*:5555  -k /etc/doubledecker/broker-keys.json -l d -s 0/0/0
```

Connect the Java client using the 'c' command: 
```
DD> c tcp://127.0.0.1:5555 testclient
```



