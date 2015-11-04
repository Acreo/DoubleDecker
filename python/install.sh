#!/bin/bash
apt-get update
apt-get install python3-setuptools python3-nacl python3-zmq python3-urwid python3-tornado git
pip3 install urwid
git clone https://github.com/Acreo/DoubleDecker.git
cd DoubleDecker/python
sudo python3 setup.py install
cd /etc
mkdir doubledecker
cd doubledecker
echo "1. Run ddkeys.py with the input a,b,c"
echo "2. You can then start a broker using this command:"
echo "ddbroker.py -r tcp://*:5555 -k /etc/doubledecker/broker-keys.json broker0"
echo "3. Clients can be started using:"
echo "ddclient.py -d tcp://127.0.0.1:5555 -k /etc/doubledecker/a-keys.json cli1 a"
