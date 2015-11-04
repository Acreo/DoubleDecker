#!/usr/bin/python3
# -*- coding: utf-8 -*-
__license__ = """
  Copyright (c) 2015 Pontus Sköldström, Bertrand Pechenot

  This file is part of libdd, the DoubleDecker hierarchical
  messaging system DoubleDecker is free software; you can
  redistribute it and/or modify it under the terms of the GNU Lesser
  General Public License (LGPL) version 2.1 as published by the Free
  Software Foundation.

  As a special exception, the Authors give you permission to link this
  library with independent modules to produce an executable,
  regardless of the license terms of these independent modules, and to
  copy and distribute the resulting executable under terms of your
  choice, provided that you also meet, for each linked independent
  module, the terms and conditions of the license of that module. An
  independent module is a module which is not derived from or based on
  this library.  If you modify this library, you must extend this
  exception to your version of the library.  DoubleDecker is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
  License for more details.  You should have received a copy of the
  GNU Lesser General Public License along with this program.  If not,
  see <http://www.gnu.org/licenses/>.
"""
__author__ = 'Bertrand Pechenot'
__email__ = 'berpec@acreo.se'

import logging
import sys
import abc
import json
import re

import zmq
import zmq.eventloop.ioloop
import zmq.eventloop.zmqstream
import nacl.utils
import nacl.public
import nacl.encoding

from . import proto as DD


class Client(metaclass=abc.ABCMeta):
    def __init__(self, name, dealerurl, customer):
        self._ctx = zmq.Context()
        self._IOLoop = zmq.eventloop.ioloop.IOLoop.instance()
        self._verbose = False
        self._dealerurl = ''
        self._dealer = self._ctx.socket(zmq.DEALER)
        self._dealer.setsockopt(zmq.LINGER, 1000)
        self._state = DD.S_UNREG
        self._timeout = 0
        self._pubsub = False
        self._sublist = list()
        self._customer = ''
        self._pubkey = ''
        self._privkey = ''
        self._hash = ''
        self._cookie = ''
        self._safe = True
        if type(name) is str:
            name = name.encode()
        if type(dealerurl) is str:
            dealerurl = dealerurl.encode()
        if type(customer) is str:
            customer = customer.encode()

        self._name = name
        self._customer = customer
        self._customer_name = '.'.join([customer.decode(), name.decode()])  # e.g.: A.client1
        self._customer_name = self._customer_name.encode('utf8')

        self._dealerurl = dealerurl
        self._dealer.connect(self._dealerurl)
        self._stream = zmq.eventloop.zmqstream.ZMQStream(self._dealer, self._IOLoop)
        self._stream.on_recv(self._on_message)

        self._IOLoop.add_handler(sys.stdin.fileno(), self.on_cli, self._IOLoop.READ)

        self._register_loop = zmq.eventloop.ioloop.PeriodicCallback(self._ask_registration, 1000)
        self._register_loop.start()
        logging.debug('Trying to register')

        self._heartbeat_loop = zmq.eventloop.ioloop.PeriodicCallback(self._heartbeat, 1500)

        logging.debug("Configured: name = %s, Dealer = %s, Customer = %s"
                      % (name.decode('utf8'), dealerurl, customer.decode('utf8')))

    def start(self):
        try:
            self._IOLoop.start()
        except KeyboardInterrupt:
            if self._state != DD.S_EXIT:
                self.shutdown()

    @abc.abstractmethod
    def on_cli(self, dummy, other_dummy):
        # implemented in sub classes
        pass

    @abc.abstractmethod
    def on_pub(self, src, topic, msg):
        pass

    @abc.abstractmethod
    def on_data(self, src, msg):
        pass

    @abc.abstractmethod
    def on_reg(self, src, msg):
        pass

    @abc.abstractmethod
    def on_discon(self):
        pass

    @abc.abstractmethod
    def subscribe(self, topic, scope):
        pass

    @abc.abstractmethod
    def unsubscribe(self, topic, scope):
        pass

    @abc.abstractmethod
    def publish(self, topic, message):
        pass

    @abc.abstractmethod
    def publish_public(self, topic, message):
        pass

    @abc.abstractmethod
    def sendmsg(self, dst, msg):
        pass

    def shutdown(self):
        logging.info('Shutting down')
        if self._state == DD.S_REGISTERED:
            for topic in self._sublist:
                logging.debug('Unsubscribing from %s' % str(topic))
                self._dealer.send_multipart([DD.bPROTO_VERSION, DD.bCMD_UNSUB, topic.encode()])

            if self._safe:
                logging.debug('Unregistering from broker, safe')
                self._send(DD.bCMD_UNREG, [self._customer, self._name, self._cookie])
            else:
                logging.debug('Unregistering from broker')
                self._send(DD.bCMD_UNREG)
        else:
            logging.debug('Stopping register loop')
            self._register_loop.stop()
        self._state = DD.S_EXIT
        logging.debug('Stopping heartbeat loop')
        self._heartbeat_loop.stop()
        logging.debug('Closing stream')
        self._stream.close()
        logging.debug('Stopping IOloop')
        self._IOLoop.stop()
        self._IOLoop.close()
        logging.debug('Terminating context')
        self._ctx.term()
        logging.debug('Calling sys.exit')
        sys.exit(0)

    @abc.abstractmethod
    def _ask_registration(self):
        # implemented in sub classes
        return

    @abc.abstractmethod
    def _on_message(self, msg):
        # implemented in sub classes
        pass

    def _send(self, command=DD.bCMD_SEND, msg=None):
        """

        :param command:
        :param msg:
        """
        if not msg:
            msg = []

        self._dealer.send_multipart([DD.bPROTO_VERSION] + [command] + msg)

    def _heartbeat(self):
        self._timeout += 1
        if self._timeout > 3:
            logging.info('Lost connection with broker')
            self._state = DD.S_UNREG
            self._heartbeat_loop.stop()
            self._stream.close()
            self._dealer.close()
            self.on_discon()
            # delete the subscriptions list
            del self._sublist[:]

            self._dealer = self._ctx.socket(zmq.DEALER)
            self._dealer.setsockopt(zmq.LINGER, 1000)
            if not self._safe:
                self._dealer.setsockopt(zmq.IDENTITY, self._customer_name)
            self._dealer.connect(self._dealerurl)
            self._stream = zmq.eventloop.zmqstream.ZMQStream(self._dealer, self._IOLoop)
            self._stream.on_recv(self._on_message)

            self._register_loop.start()
            logging.debug('Trying to register')

    @abc.abstractmethod
    def _cli_usage(self):
        # implemented in sub classes
        pass

    def _ping(self):
        self._send(DD.bCMD_PING)


class ClientSafe(Client):
    def __init__(self, name, dealerurl, customer,keyfile):
        super().__init__(name, dealerurl, customer)

        if not keyfile:
            filename = self._customer.decode() + '-keys.json'
        else:
            filename = keyfile
        try:
            f = open(filename, 'r')
        except:
            logging.critical("Could not find key for customer, file: %s" % filename)
            self.shutdown()
            raise RuntimeError("Keyfile not found!")
        key = json.load(f)
        f.close()
        if self._customer.decode() == 'public':
            self._privkey = nacl.public.PrivateKey(key['public']['privkey'], encoder=nacl.encoding.Base64Encoder)
            self._pubkey = nacl.public.PublicKey(key['public']['pubkey'], encoder=nacl.encoding.Base64Encoder)
            self._cust_box = nacl.public.Box(self._privkey, self._pubkey)
            ddpubkey = nacl.public.PublicKey(key['public']['ddpubkey'], encoder=nacl.encoding.Base64Encoder)
            self._dd_box = nacl.public.Box(self._privkey, ddpubkey)
            publicpubkey = nacl.public.PublicKey(key['public']['publicpubkey'], encoder=nacl.encoding.Base64Encoder)
            self._hash = key['public']['hash'].encode()
            del key['public']
            # create a nacl.public.Box for each customers in a dict, e.g. self.cust_boxes[a] for customer a
            self._cust_boxes = dict()
            for hash in key:
                cust_public_key = nacl.public.PublicKey(key[hash]['pubkey'], encoder=nacl.encoding.Base64Encoder)
                self._cust_boxes[key[hash]['r']] = nacl.public.Box(self._privkey, cust_public_key)
        else:
            self._privkey = nacl.public.PrivateKey(key['privkey'], encoder=nacl.encoding.Base64Encoder)
            self._pubkey = nacl.public.PublicKey(key['pubkey'], encoder=nacl.encoding.Base64Encoder)
            self._cust_box = nacl.public.Box(self._privkey, self._pubkey)
            ddpubkey = nacl.public.PublicKey(key['ddpubkey'], encoder=nacl.encoding.Base64Encoder)
            self._dd_box = nacl.public.Box(self._privkey, ddpubkey)
            publicpubkey = nacl.public.PublicKey(key['publicpubkey'], encoder=nacl.encoding.Base64Encoder)
            self._pub_box = nacl.public.Box(self._privkey, publicpubkey)
            self._hash = key['hash'].encode()

        self._nonce = bytearray(nacl.utils.random(nacl.public.Box.NONCE_SIZE))
        self._safe = True

    def subscribe(self, topic, scope):
        if self._state != DD.S_REGISTERED:
            raise (ConnectionError("Not registered"))

        scope = scope.strip().lower()
        if scope == 'all':
            scopestr = "/"
        elif scope == 'region':
            scopestr = "/*/"
        elif scope == "cluster":
            scopestr = "/*/*/"
        elif scope == "node":
            scopestr = "/*/*/*/"
        elif scope == "noscope":
            scopestr = "noscope"
        elif re.fullmatch("/((\d)+/)+", scope):
            # check that scope only contains numbers and slashes
            scopestr = scope
        else:
            raise SyntaxError("Scope supports ALL/REGION/CLUSTER/NODE/NOSCOPE, or specific values,e.g. /1/2/3/")
        if scopestr == "noscope":
            logging.debug("Subscribing to %s" % str(topic))
        else:
            logging.debug("Subscribing to %s" % str(topic + scopestr))
        self._send(DD.bCMD_SUB, [self._cookie, topic.encode(), scopestr.encode()])

    def unsubscribe(self, topic, scope):
        if self._state != DD.S_REGISTERED:
            raise (ConnectionError("Not registered"))

        scope = scope.strip().lower()
        if scope == 'all':
            scopestr = "/"
        elif scope == 'region':
            scopestr = "/*/"
        elif scope == "cluster":
            scopestr = "/*/*/"
        elif scope == "node":
            scopestr = "/*/*/*/"
        elif scope == "noscope":
            scopestr = "noscope"
        elif re.fullmatch("/((\d)+/)+", scope):
            # check that scope only contains numbers and slashes
            scopestr = scope
        else:
            raise SyntaxError("Scope supports ALL/REGION/CLUSTER/NODE/NOSCOPE, or specific values,e.g. /1/2/3/")
        if scopestr == "noscope":
            logging.debug("Subscribing to %s" % str(topic))
        else:
            logging.debug("Subscribing to %s" % str(topic + scopestr))
        self._send(DD.bCMD_UNSUB, [self._cookie, topic.encode(), scopestr.encode()])


    def publish(self, topic, message):
        if self._state != DD.S_REGISTERED:
            raise (ConnectionError("Not registered"))
        if type(topic) is str:
            topic = topic.encode('utf8')
        if type(message) is str:
            message = message.encode('utf8')

        encryptmsg = self._cust_box.encrypt(message, self._get_nonce())
        self._dealer.send_multipart([DD.bPROTO_VERSION, DD.bCMD_PUB, self._cookie, topic, b'', encryptmsg])

    def publish_public(self, topic, message):
        if self._state != DD.S_REGISTERED:
            raise (ConnectionError("Not registered"))
        if type(topic) is str:
            topic = topic.encode('utf8')
        if type(message) is str:
            message = message.encode('utf8')

        encryptmsg = self._pub_box.encrypt(message, self._get_nonce())
        self._dealer.send_multipart([DD.bPROTO_VERSION, DD.bCMD_PUB, self._cookie, topic, b'', encryptmsg])

    def sendmsg(self, dst, msg):
        if self._state != DD.S_REGISTERED:
            raise (ConnectionError("Not registered"))

        if self._customer == b'public':
            dst_is_public = True
            try:
                split = dst.split('.')
                customer_dst = split[0]
                if customer_dst in self._cust_boxes:
                    dst_is_public = False
            except:
                pass

            if type(dst) is str:
                dst = dst.encode('utf8')
            if type(msg) is str:
                msg = msg.encode('utf8')

            if dst_is_public:
                # public --> public
                msg = self._cust_box.encrypt(msg, self._get_nonce())
                # print("Sending encrypted data to %s" % dst.decode('utf8'))
                self._send(DD.bCMD_SEND, [self._cookie, dst, msg])
            else:
                # public --> non-public
                msg = self._cust_boxes[customer_dst].encrypt(msg, self._get_nonce())
                # print("Sending encrypted data to %s" % dst.decode('utf8'))
                self._send(DD.bCMD_SEND, [self._cookie, dst, msg])
        else:
            # send to a public or not ?
            dst_is_public = False
            try:
                split = dst.split('.')
                dst_is_public = split[0] == 'public'
            except:
                pass

            if type(dst) is str:
                dst = dst.encode('utf8')
            if type(msg) is str:
                msg = msg.encode('utf8')

            if dst_is_public:
                # non-public --> public
                msg = self._pub_box.encrypt(msg, self._get_nonce())
                # print("Sending encrypted data to %s" % dst.decode('utf8'))
                self._send(DD.bCMD_SEND, [self._cookie, dst, msg])
            else:
                # non-public --> non-public
                msg = self._cust_box.encrypt(msg, self._get_nonce())
                # print("Sending encrypted data to %s" % dst.decode('utf8'))
                # print("self.R: ", type(self.R), " dst: ", type(dst), " msg:", type(msg) )
                self._send(DD.bCMD_SEND, [self._cookie, dst, msg])

    @abc.abstractmethod
    def on_reg(self):
        # print('Safe client got registered correctly')
        pass

    @abc.abstractmethod
    def on_discon(self):
        pass

    def on_cli(self, dummy, other_dummy):
        cmd = sys.stdin.readline().split(maxsplit=2)
        if len(cmd) == 0:
            self._cli_usage()
        elif 'send' == cmd[0]:
            # send an encrypted message
            if len(cmd) < 3:
                self._cli_usage()
                return
            dst = cmd[1]
            msg = cmd[2].strip().encode('utf8')
            self.sendmsg(dst, msg)

        elif 'sendPT' == cmd[0]:
            if len(cmd) < 3:
                self._cli_usage()
                return
            dst = cmd[1].encode('utf8')
            msg = cmd[2].strip().encode('utf8')
            logging.debug("Sending \"%s\" to %s" %
                          (msg, dst.decode('utf8')))
            self._send(DD.bCMD_SENDPT, [self._cookie, dst, msg])
        elif 'exit' == cmd[0]:
            self.shutdown()
        elif 'sub' == cmd[0]:
            if len(cmd) > 2:
                # print("Subscribing to",cmd[1]+cmd[2])
                self.sub_scope(cmd[1], cmd[2])
            else:
                print("usage: sub [topic] [scope], where scope is ALL, REGION, CLUSTER, NODE, /1/2/3/, NOSCOPE")
        elif 'unsub' == cmd[0]:
            if len(cmd) > 1:
                if cmd[1] in self._sublist:
                    logging.info("Unsubscribing from %s*" % cmd[1])
                    for sub in self._sublist:
                        if(sub.startswith(cmd[1]+"/")):
                            self._dealer.send_multipart([DD.bPROTO_VERSION, DD.bCMD_UNSUB, sub.encode()])
                            self._sublist.remove(sub)

                else:
                    logging.info("You are not subscribed to this topic: %s" % cmd[1])
            else:
                print("usage: unsub [topic]")
        elif 'pub' == cmd[0]:
            if len(cmd) > 2:
                logging.info("Publishing message on %s*" % cmd[1])
                if self._customer == b'public':
                    # public --> public
                    self.publish(cmd[1].encode(), cmd[2].encode())
                else:
                    topic_is_public = False
                    topic = cmd[1]
                    try:
                        split = topic.split('.')
                        customer_dst = split[0]
                        if customer_dst == 'public':
                            topic_is_public = True
                            topic = split[1]
                    except:
                        pass
                    if topic_is_public:
                        # non-public --> public
                        self.publish_public(topic.encode(), cmd[2].encode())
                    else:
                        # public --> public
                        self.publish(topic.encode(), cmd[2].encode())
            else:
                print("usage: pub [topic] [message]")
        elif 'pubpublic' == cmd[0]:
            if len(cmd) > 2 and self._customer != b'public':
                logging.info("Publishing message on public %s*" % cmd[1])
                self.publish_public(cmd[1].encode(), cmd[2].encode())
            else:
                print("usage: pubpublic [topic] [message]")
        else:
            self._cli_usage()

    def _ask_registration(self):
        self._dealer.setsockopt(zmq.LINGER, 0)
        self._stream.close()
        self._dealer.close()
        self._dealer = self._ctx.socket(zmq.DEALER)
        self._dealer.setsockopt(zmq.LINGER, 1000)
        self._dealer.connect(self._dealerurl)
        self._stream = zmq.eventloop.zmqstream.ZMQStream(self._dealer, self._IOLoop)
        self._stream.on_recv(self._on_message)
        self._send(DD.bCMD_ADDLCL, [self._hash])

    def _on_message(self, msg):
        self._timeout = 0
        if msg.pop(0) != DD.bPROTO_VERSION:
            logging.warning('Different protocols in use, message discarded')
            return
        cmd = msg.pop(0)
        if cmd == DD.bCMD_REGOK:
            logging.debug('Registered correctly')
            self._state = DD.S_REGISTERED
            self._register_loop.stop()
            self._cookie = msg.pop(0)
            if type(self._cookie) is str:
                self._cookie = self._cookie.encode('utf8')

            try:
                scope = msg.pop(0)
                self._scope = scope
            except:
                pass
            self._heartbeat_loop.start()
            self._send(DD.bCMD_PING)
            self.on_reg()
        elif cmd == DD.bCMD_DATA:
            source = msg.pop(0)
            if self._customer == b'public':
                customer_source = source.decode().split('.')[0]
                if customer_source in self._cust_boxes:
                    # non-public --> public
                    msg = self._cust_boxes[customer_source].decrypt(msg.pop())
                else:
                    # public --> public
                    msg = self._cust_box.decrypt(msg.pop())
            else:
                customer_source = source.decode().split('.')[0]
                if customer_source == 'public':
                    # public --> non-public
                    msg = self._pub_box.decrypt(msg.pop())
                else:
                    # non-public --> non-public
                    msg = self._cust_box.decrypt(msg.pop())
            self.on_data(source, msg)
        elif cmd == DD.bCMD_DATAPT:
            self.on_data(msg.pop(0), msg)
        elif cmd == DD.bCMD_PONG:
            ioloop = zmq.eventloop.ioloop.IOLoop.current()
            ioloop.add_timeout(ioloop.time() + 1.5, self._ping)
        elif cmd == DD.bCMD_CHALL:
            logging.debug("Got challenge...")
            self._state = DD.S_CHALLENGED
            encryptednumber = msg.pop(0)
            decryptednumber = self._dd_box.decrypt(encryptednumber)
            # Send the decrypted number, his hash and his name for the registration
            self._send(DD.bCMD_CHALLOK, [decryptednumber, self._hash, self._name])
        elif cmd == DD.bCMD_PUB:
            src = msg.pop(0)
            topic = msg.pop(0)
            encryptmsg = msg.pop(0)
            if self._customer == b'public':
                src_customer = src.decode().split('.')[0]
                if src_customer in self._cust_boxes:
                    # non-public --> public
                    decryptmsg = self._cust_boxes[src_customer].decrypt(encryptmsg)
                else:
                    # public --> public
                    decryptmsg = self._cust_box.decrypt(encryptmsg)
            else:
                # non-public --> non-public
                decryptmsg = self._cust_box.decrypt(encryptmsg)
            self.on_pub(src, topic, decryptmsg)
        elif cmd == DD.bCMD_PUBPUBLIC:
            src = msg.pop(0)
            topic = msg.pop(0)
            self.on_pub(src, topic, msg)
        elif cmd == DD.bCMD_SUBOK:
            topic = msg.pop(0).decode()
            scope = msg.pop(0).decode()
            tt = "%s%s"%(topic,scope)
            if tt not in self._sublist:
                self._sublist.append(tt)
            else:
                logging.error("Already subscribed to topic %s" % str(topic))
                self._dealer.send_multipart([DD.bPROTO_VERSION, DD.bCMD_UNSUB, topic.encode()])
        elif cmd == DD.bCMD_NODST:
            logging.warning("Unknown client %s" % str(msg.pop(0)))
        else:
            logging.warning("Unknown message, got: %d %s" % (int(cmd), str(msg)))

    def _get_nonce(self):
        index = 23
        while True:
            try:
                self._nonce[index] += 1
                return bytes(self._nonce)
            except ValueError as e:
                self._nonce[index] = 0
                index -= 1

    def _cli_usage(self):
        print("Commands: ")
        print("help                     - show this help")
        print("send        [client] [message] - send an encrypted message to client")
        print("sendPT      [client] [message] - send a plain text message to client")
        print("sendpublic  [public client] [message] - send message to a public client")
        print("pub         [topic]  [message] - publish message on topic")
        print("pubpublic   [topic]  [message] - publish message on public topic")
        print("sub         [topic]            - subscribe to messages in topic")
        print("unsub       [topic]            - subscribe to messages in topic")
        print('exit                           - unregister and exit')


class ClientUnsafe(Client):
    def __init__(self, name, dealerurl, customer):
        super().__init__(name, dealerurl, customer)
        self._dealer.setsockopt(zmq.IDENTITY, self._customer_name)
        self._hash = b'unsafe'
        self._safe = False

    @abc.abstractmethod
    def on_reg(self):
        pass

    def on_cli(self, dummy, other_dummy):
        cmd = sys.stdin.readline().split(maxsplit=2)
        if len(cmd) == 0:
            self._cli_usage()
        elif 'send' == cmd[0]:
            if len(cmd) < 3:
                self._cli_usage()
                return
            dst = cmd[1].encode("utf8")
            msg = cmd[2].strip().encode('utf8')
            logging.debug("Sending \"%s\" to %s" %
                          (msg, dst.decode('utf8')))
            self.sendmsg(dst, msg)
        elif 'exit' == cmd[0]:
            self.shutdown()
        elif 'sub' == cmd[0]:
            if len(cmd) > 2:
                # print("Subscribing to",cmd[1]+'/'+cmd[2])
                self.sub_scope(cmd[1], cmd[2])
            else:
                print("usage: sub [topic] [scope], where scope is ALL, REGION, CLUSTER, NODE, 1/2/3, NOSCOPE")
        elif 'unsub' == cmd[0]:
            if len(cmd) > 1:
                if (cmd[1] in self._sublist):
                    logging.info("Unsubscribing from %s*" % cmd[1])
                    for sub in self._sublist:
                        if(sub.startswith(cmd[1]+"/")):
                            self._dealer.send_multipart([DD.bPROTO_VERSION, DD.bCMD_UNSUB, sub.encode()])
                            self._sublist.remove(sub)

                else:
                    logging.info("You are not subscribed to this topic: %s" % cmd[1])
            else:
                print("usage: unsub [topic]")
        elif 'pub' == cmd[0]:
            if len(cmd) > 2:
                logging.info("Publishing message on %s*" % cmd[1])
                self.publish(cmd[1].encode(), cmd[2].encode())
            else:
                print("usage: pub [topic] [message]")
        else:
            self._cli_usage()

    def subscribe(self, topic, scope):
        scope = scope.strip().lower()
        if scope == 'all':
            scopestr = "/"
        elif scope == 'region':
            scopestr = "/*/"
        elif scope == "cluster":
            scopestr = "/*/*/"
        elif scope == "node":
            scopestr = "/*/*/*/"
        elif scope == "noscope":
            scopestr = "noscope"
        elif re.match("/(\d/)+", scope):
            # check that scope only contains numbers and slashes
            if scope[-1] == '/':
                scopestr = scope
            else:
                scopestr = scope + '/'
        else:
            print("The scope should be like: '/1/2/3/'")
            return
        if scopestr == "noscope":
            logging.info("Subscribing to %s" % topic)
        else:
            logging.info("Subscribing to %s" % (topic + scopestr))
        self._send(DD.bCMD_SUB, [topic.encode(), scopestr.encode()])

    def publish(self, topic, message):
        if self._state != DD.S_REGISTERED:
            raise (ConnectionError("Not registered"))
        if type(topic) is str:
            topic = topic.encode('utf8')
        if type(message) is str:
            message = message.encode('utf8')

        self._dealer.send_multipart([DD.bPROTO_VERSION, DD.bCMD_PUB, b'0', topic, b'', message])

    def publish_public(self, topic, message):
        if self._state != DD.S_REGISTERED:
            raise (ConnectionError("Not registered"))
        if type(topic) is str:
            topic = topic.encode('utf8')
        if type(message) is str:
            message = message.encode('utf8')

        self._dealer.send_multipart([DD.bPROTO_VERSION, DD.bCMD_PUB, b'1', topic, b'', message])

    def sendmsg(self, dst, msg):
        if self._state != DD.S_REGISTERED:
            raise (ConnectionError("Not registered"))

        logging.info("client-sendmsg called!\n")
        if self._customer == b'public':
            dst_is_public = True
            try:
                split = dst.split('.')
                customer_dst = split[0]
            except:
                pass

            if type(dst) is str:
                dst = dst.encode('utf8')
            if type(dst) is str:
                msg = msg.encode('utf8')

            if dst_is_public:
                # public --> public
                self._send(DD.bCMD_SEND, [b'0', dst, msg])
            else:
                # public --> non-public
                self._send(DD.bCMD_SEND, [b'1', dst, msg])
        else:
            # send to a public or not ?
            dst_is_public = False
            try:
                split = dst.split('.')
                dst_is_public = split[0] == 'public'
            except:
                pass

            if type(dst) is str:
                dst = dst.encode('utf8')
            if type(msg) is str:
                msg = msg.encode('utf8')

            if dst_is_public:
                # non-public --> public
                self._send(DD.bCMD_SEND, [b'1', dst, msg])
            else:
                # non-public --> non-public
                self._send(DD.bCMD_SEND, [b'0', dst, msg])

    def _ask_registration(self):
        self._dealer.setsockopt(zmq.LINGER, 0)
        self._stream.close()
        self._dealer.close()
        self._dealer = self._ctx.socket(zmq.DEALER)
        self._dealer.setsockopt(zmq.IDENTITY, self._customer_name)
        self._dealer.connect(self._dealerurl)
        self._stream = zmq.eventloop.zmqstream.ZMQStream(self._dealer, self._IOLoop)
        self._stream.on_recv(self._on_message)
        self._send(DD.bCMD_ADDLCL, [self._hash, self._customer, self._name])

    def _on_message(self, msg):
        self._timeout = 0
        if msg.pop(0) != DD.bPROTO_VERSION:
            logging.warning('different protocols in use, message discarded')
            return
        cmd = msg.pop(0)
        if cmd == DD.bCMD_REGOK:
            self._state = DD.S_REGISTERED
            self._register_loop.stop()
            try:
                scope = msg.pop(0)
                self.scope = scope
            except:
                pass
            self._heartbeat_loop.start()
            self._send(DD.bCMD_PING)
            self.on_reg()
        elif cmd == DD.bCMD_DATA:
            # print(msg.pop(0), 'sent', msg)
            self.on_data(msg.pop(0), msg)
        elif cmd == DD.bCMD_PONG:
            ioloop = zmq.eventloop.ioloop.IOLoop.current()
            ioloop.add_timeout(ioloop.time() + 1.5, self._ping)
        elif cmd == DD.bCMD_PUB:
            src = msg.pop(0)
            topic = msg.pop(0)
            self.on_pub(src, topic, msg)
        elif cmd == DD.bCMD_SUBOK:
            topic = msg.pop(0).decode()
            scope = msg.pop(0).decode()
            tt = "%s%s"%(topic,scope)
            if tt not in self._sublist:
                self._sublist.append(tt)
            else:
                logging.error("You are already subscribed to this topic")
                self._dealer.send_multipart([DD.bPROTO_VERSION, DD.bCMD_UNSUB, topic.encode()])
        elif cmd == DD.bCMD_NODST:
            logging.warning("Unknown client %s" % msg.pop(0))
        else:
            logging.warning("Unknown command, got: %d %s" % (cmd, msg))

    def _cli_usage(self):
        print("Commands: ")
        print("help                     - show this help")
        print("send  [client] [message] - send message to client")
        print("pub   [topic]  [message] - publish message on topic")
        print("sub   [topic]            - subscribe to messages in topic")
        print("unsub [topic]            - subscribe to messages in topic")
        print('exit                     - unregister and exit')
