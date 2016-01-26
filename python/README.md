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

