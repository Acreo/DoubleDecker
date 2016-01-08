# coding=utf-8
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
from .clientInterface import Client


class ClientSafe(Client):
    """
    DoubleDecker client with encryption and authentication

    :param name: Client name
    :param dealerurl: URL to connect to
    :param customer: Customer name
    :param keyfile: Location of JSON file containing the keys
    :raise RuntimeError:
    """

    def __init__(self, name, dealerurl, customer, keyfile):
        super().__init__(name, dealerurl, customer)

        if not keyfile:
            filename = self._customer.decode() + '-keys.json'
        else:
            filename = keyfile
        try:
            f = open(filename)
        except:
            logging.critical("Could not find key for customer, file: %s", filename)
            self.shutdown()
            raise RuntimeError("Keyfile not found!")
        key = json.load(f)
        f.close()
        if self._customer.decode() == 'public':
            self._privkey = nacl.public.PrivateKey(
                key['public']['privkey'],
                encoder=nacl.encoding.Base64Encoder)
            self._pubkey = nacl.public.PublicKey(
                key['public']['pubkey'],
                encoder=nacl.encoding.Base64Encoder)
            self._cust_box = nacl.public.Box(
                self._privkey,
                self._pubkey)
            ddpubkey = nacl.public.PublicKey(
                key['public']['ddpubkey'],
                encoder=nacl.encoding.Base64Encoder)
            self._dd_box = nacl.public.Box(self._privkey, ddpubkey)
            publicpubkey = nacl.public.PublicKey(
                key['public']['publicpubkey'],
                encoder=nacl.encoding.Base64Encoder)
            self._hash = key['public']['hash'].encode()
            del key['public']
            # create a nacl.public.Box for each customers in a dict, e.g. self.cust_boxes[a] for customer a
            self._cust_boxes = dict()
            for hash_ in key:
                cust_public_key = nacl.public.PublicKey(
                    key[hash_]['pubkey'],
                    encoder=nacl.encoding.Base64Encoder)
                self._cust_boxes[key[hash_]['r']] = nacl.public.Box(
                    self._privkey, cust_public_key)
        else:
            self._privkey = nacl.public.PrivateKey(
                key['privkey'],
                encoder=nacl.encoding.Base64Encoder)
            self._pubkey = nacl.public.PublicKey(
                key['pubkey'],
                encoder=nacl.encoding.Base64Encoder)
            self._cust_box = nacl.public.Box(self._privkey, self._pubkey)
            ddpubkey = nacl.public.PublicKey(
                key['ddpubkey'],
                encoder=nacl.encoding.Base64Encoder)
            self._dd_box = nacl.public.Box(
                self._privkey,
                ddpubkey)
            publicpubkey = nacl.public.PublicKey(
                key['publicpubkey'],
                encoder=nacl.encoding.Base64Encoder)
            self._pub_box = nacl.public.Box(self._privkey, publicpubkey)
            self._hash = key['hash'].encode()

        self._nonce = bytearray(nacl.utils.random(nacl.public.Box.NONCE_SIZE))
        self._subscriptions = list()

    def subscribe(self, topic, scope):
        """
        Subscribe to a topic with a given scope
        :param topic: Name of the topic
        :param scope: all, region, cluster, node or noscope
        :raise SyntaxError:
        """
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

        if (topic, scopestr) in self._subscriptions:
            logging.warning("Already subscribed to %s %s", topic, scopestr)
            return
        else:
            self._subscriptions.append((topic,scopestr))
        if scopestr == "noscope":
            logging.debug("Subscribing to %s", topic)
        else:
            logging.debug("Subscribing to %s %s", topic, scopestr)

        self._send(DD.bCMD_SUB, [self._cookie, topic.encode(), scopestr.encode()])


    def unsubscribe(self, topic, scope):
        """
        Unsubscribe from a partiuclar topic and scope
        :param topic: Topic to unsubscribe from
        :param scope: all, region, cluster, node or noscope
        :raise SyntaxError:
        """
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
            logging.debug("Unsubscribing from %s", topic)
        else:
            logging.debug("Unsubscribing from %s", topic, scopestr)
        if (topic, scopestr) in self._subscriptions:
            self._subscriptions.remove((topic, scopestr))
        else:
            logging.warning("Not subscribed to %s %s !", topic, scopestr)
            return

        self._send(DD.bCMD_UNSUB,[self._cookie, topic.encode(), scopestr.encode()])

    def publish(self, topic, message):
        """
        Publish a message on a topic
        :param topic: Which topic to publish to
        :param message: The message to publish
        :raise (ConnectionError("Not registered")):
        """
        if self._state != DD.S_REGISTERED:
            raise (ConnectionError("Not registered"))
        if isinstance(topic, str):
            topic = topic.encode('utf8')
        if isinstance(message, str):
            message = message.encode('utf8')

        encryptmsg = self._cust_box.encrypt(message, self._get_nonce())
        self._dealer.send_multipart(
            [DD.bPROTO_VERSION, DD.bCMD_PUB, self._cookie, topic, b'', encryptmsg])

    def publish_public(self, topic, message):
        """
        Publish a message to a public topic (uses different encryption key)
        :param topic: Which topic to publish to
        :param message: The message to publish
        :raise (ConnectionError("Not registered")):
        """
        if self._state != DD.S_REGISTERED:
            raise (ConnectionError("Not registered"))
        if isinstance(topic, str):
            topic = topic.encode('utf8')
        if isinstance(message, str):
            message = message.encode('utf8')

        encryptmsg = self._pub_box.encrypt(message, self._get_nonce())
        self._dealer.send_multipart(
            [DD.bPROTO_VERSION, DD.bCMD_PUB, self._cookie, topic, b'', encryptmsg])

    def sendmsg(self, dst, msg):
        """
        Send a notification
        :param dst: Destination for the notification
        :param msg: Data to send
        :raise (ConnectionError("Not registered")):
        """
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

            if isinstance(dst, str):
                dst = dst.encode('utf8')
            if isinstance(msg, str):
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

            if isinstance(dst, str):
                dst = dst.encode('utf8')
            if isinstance(msg, str):
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
    def on_reg(self,src,msg):
        # print('Safe client got registered correctly')
        """
        Function called when client registration succeeded
        :param src:
        :param msg:
        """
        pass

    @abc.abstractmethod
    def on_discon(self):
        """

        Called when the client was disconnected from the broker
        """
        pass

    def on_cli(self, dummy, other_dummy):
        """
        Called when the CLI is updated, if running interactively
        :param dummy:
        :param other_dummy:
        :return:
        """
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
            logging.debug("Sending \"%s\" to %s", msg, dst.decode('utf8'))
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
                    logging.info("Unsubscribing from %s*", cmd[1])
                    for sub in self._sublist:
                        if sub.startswith(cmd[1] + "/"):
                            self._dealer.send_multipart(
                                [DD.bPROTO_VERSION, DD.bCMD_UNSUB, sub.encode()])
                            self._sublist.remove(sub)
                else:
                    logging.info("You are not subscribed to this topic: %s", cmd[1])
            else:
                print("usage: unsub [topic]")
        elif 'pub' == cmd[0]:
            if len(cmd) > 2:
                logging.info("Publishing message on %s*", cmd[1])
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
                logging.info("Publishing message on public %s*", cmd[1])
                self.publish_public(cmd[1].encode(), cmd[2].encode())
            else:
                print("usage: pubpublic [topic] [message]")
        else:
            self._cli_usage()

    def _ping(self):
        self._send(DD.bCMD_PING, [self._cookie])

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
            if isinstance(self._cookie, str):
                self._cookie = self._cookie.encode('utf8')

            # try:
            #     scope = msg.pop(0)
            #     self._scope = scope
            # except:
            #     pass
            self._heartbeat_loop.start()
            self._send(DD.bCMD_PING, [self._cookie])
            for (topic, scopestr) in self._subscriptions:
                self._send(DD.bCMD_SUB, [self._cookie, topic.encode(), scopestr.encode()])

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
            tt = "%s%s" % (topic, scope)
            if tt not in self._sublist:
                self._sublist.append(tt)
            else:
                logging.error("Already subscribed to topic %s", topic)
                self._dealer.send_multipart([DD.bPROTO_VERSION, DD.bCMD_UNSUB, topic.encode()])
        elif cmd == DD.bCMD_NODST:
            logging.warning("Unknown client %s", msg.pop(0))
        else:
            logging.warning("Unknown message, got: %i %s", cmd, msg)

    def _get_nonce(self):
        index = nacl.public.Box.NONCE_SIZE - 1
        while True:
            try:
                self._nonce[index] += 1
                return bytes(self._nonce)
            except ValueError:
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
