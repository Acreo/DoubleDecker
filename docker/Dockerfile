FROM ubuntu:latest
MAINTAINER Bertrand Pechenot berpec@acreo.se
RUN apt-get update 
RUN DEBIAN_FRONTEND=noninteractive apt-get install -yq apt-utils wget build-essential libtool
RUN DEBIAN_FRONTEND=noninteractive apt-get install -yq automake pkg-config libjson0-dev libzmq3-dev

# Broker default port 
EXPOSE 5555 

# start in /bus
WORKDIR /bus

# install libczmq
RUN wget https://github.com/zeromq/czmq/archive/v3.0.2.tar.gz
RUN tar xzf v3.0.2.tar.gz
WORKDIR /bus/czmq-3.0.2
RUN ./autogen.sh
RUN ./configure
RUN make install
WORKDIR /bus


# install urcu
RUN wget http://www.lttng.org/files/urcu/userspace-rcu-0.9.1.tar.bz2
RUN tar xjf userspace-rcu-0.9.1.tar.bz2
WORKDIR /bus/userspace-rcu-0.9.1
RUN ./configure
RUN make install
WORKDIR /bus


# install libsodium
RUN wget https://download.libsodium.org/libsodium/releases/libsodium-1.0.7.tar.gz
RUN tar xzf libsodium-1.0.7.tar.gz
WORKDIR /bus/libsodium-1.0.7
RUN ./configure
RUN make install
WORKDIR /bus


# TODO: write a run.sh script that handles ENV variables properly
ADD ./run.sh /bus/
RUN wget https://github.com/Acreo/DoubleDecker/archive/master.tar.gz -O dd.tar.gz
RUN tar xzf dd.tar.gz
WORKDIR /bus/DoubleDecker-master/c/
RUN mkdir -pv m4
RUN ls
RUN autoreconf --force --install
RUN ./configure
RUN make
RUN make install
RUN ldconfig

ENV BROKER_PORT=tcp://*:5555
ENV BROKER_SCOPE=0/0/0
WORKDIR /bus/
RUN rm -rf czmq-3.0.2 libsodium-1.0.3.tar.gz libsodium-1.0.7.tar.gz userspace-rcu-0.9.1 v3.0.2.tar.gz libsodium-1.0.3 libsodium-1.0.7 userspace-rcu-0.9.1.tar.bz2
RUN DEBIAN_FRONTEND=noninteractive apt-get remove -yq git apt-utils wget build-essential libtool git libtool-bin automake pkg-config
RUN DEBIAN_FRONTEND=noninteractive apt-get autoremove -yq
RUN DEBIAN_FRONTEND=noninteractive apt-get autoclean -yq
CMD ./run.sh




