FROM ubuntu:xenial
MAINTAINER Pontus Skoldstrom ponsko@acreo.se
RUN DEBIAN_FRONTEND=noninteractive apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -yq apt-utils wget\
	 build-essential libtool  automake pkg-config libjson0-dev libczmq-dev \ 
	 liburcu-dev libsodium-dev git 	


# start in /bus
WORKDIR /bus
ARG CACHEBUST=1
RUN git clone https://github.com/Acreo/DoubleDecker.git
WORKDIR /bus/DoubleDecker
RUN mkdir -pv m4 && autoreconf --force --install && ./configure && make && make install && ldconfig

ADD ./run.sh /bus/

ENV BROKER_PORT=tcp://*:5555
ENV BROKER_SCOPE=0/0/0
WORKDIR /bus/
EXPOSE 5555
EXPOSE 5556
EXPOSE 5557
CMD ./run.sh

# VOLUMES
#########

# by default the broker tries to read keys from /keys/broker-keys.json
# the /keys volume has to be provided when starting the container 
# eg -v /etc/doubledecker:/keys

# Environmental variables
#########################

# BROKER_PORT  
# tcp/ipc where the broker is listening for clients/southern
# brokers. separated by comma if multiple

#BROKER_SCOPE
# scope of the broker, e.g. 0/0/1

#DEALER_PORT
# the BROKER_PORT of another broker, when building a hierarchy

#DEBUG
# Debuglevel

#KEYS
# filename and path of the broker-keys.json 

#REST
# string for opening a management REST API, e.g. tcp://*:8080