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



import argparse
import logging
import sys
from doubledecker.client import ClientSafe
from doubledecker.client import ClientUnsafe
import urwid
from collections import deque
import time
import datetime
import json

from pprint import pprint
# Inherit ClientSafe and implement the abstract classes
# ClientSafe does encryption and authentication using ECC (libsodium/nacl)
class SecureCli(ClientSafe):
    def __init__(self, name, dealerurl, customer,keyfile):
        super().__init__(name, dealerurl, customer,keyfile)
        self.msg_list = deque(maxlen=10)
        self.messages = ""

        self.pub_msg = ""
        self.pub_topic = ""

        self.notify_dest = ""
        self.notify_msg = ""

        self.sub_scope = ""
        self.sub_topic = ""

        self.unsub_scope = ""
        self.unsub_topic = ""

        self.subscriptions = []
        self.registered = False

    def add_msg(self,msg):
        st = datetime.datetime.fromtimestamp(time.time()).strftime('%H:%M:%S')
        self.msg_list.append("%s:%s"%(st,msg))
        self.messages = ""

        for n in self.msg_list:
            self.messages = self.messages + n + "\n"
        self.body[-1].set_text(self.messages)

    def on_data(self, src, msg):
        self.add_msg("DATA from %s: %s" % (str(src), str(msg)))

    def on_reg(self):
        self.registered = True
        self.update_main_text()

    def on_discon(self):
        self.registered = False
        self.update_main_text()

    def on_pub(self, src, topic, msg):
        msgstr = "PUB %s from %s: %s" % (str(topic), str(src), str(msg))
        self.add_msg(msgstr)

    def update_main_text(self):
        if self.registered:
            state = "Connected"
        else:
            state = "Disconnected"

        substr = json.dumps(self.subscriptions)
        self.main_text = "DoubleDecker %s Dealer: %s State: %s\nSubscriptions: %s"%(
            self._name.decode(),self._dealerurl.decode(),state,substr)
        try:
            self.body[0].set_text(self.main_text)
        except AttributeError:
            pass


    def on_cli(self, dummy, other_dummy):
        cmd = sys.stdin.readline().split(maxsplit=2)
        print("got %s"%cmd)

    def run(self):
        self.choices = ["Subscribe","Unsubscribe", "Exit"]
        substring = json.dumps(self.subscriptions)
        self.update_main_text()
        self.main = urwid.Padding(self.menu(self.main_text, self.choices), left=2, right=2)
        urwid.MainLoop(self.main,  palette=[('reversed', 'standout', '')],
                       event_loop=urwid.TornadoEventLoop(genclient._IOLoop)).run()

    def menu(self,title, choices):
        self.body = [urwid.Text(title), urwid.Divider(div_char='~')]

        notify_button = urwid.Button("Send Notifcation")
        urwid.connect_signal(notify_button, 'click', self.notify_handler, "notify")
        self.body.append(urwid.AttrMap(notify_button, None, focus_map='reversed'))

        publish_button = urwid.Button("Publish message")
        urwid.connect_signal(publish_button, 'click', self.publish_handler, "publish")
        self.body.append(urwid.AttrMap(publish_button, None, focus_map='reversed'))

        subscribe_button = urwid.Button("Subscribe to topic")
        urwid.connect_signal(subscribe_button, 'click', self.subscribe_handler, "subscribe")
        self.body.append(urwid.AttrMap(subscribe_button, None, focus_map='reversed'))

        unsubscribe_button = urwid.Button("Unsubscribe from topic")
        urwid.connect_signal(unsubscribe_button, 'click', self.unsubscribe_handler, "unsubscribe")
        self.body.append(urwid.AttrMap(unsubscribe_button, None, focus_map='reversed'))

        exit_button = urwid.Button("Quit DoubleDecker demo client")
        urwid.connect_signal(exit_button, 'click', self.exit_program)
        self.body.append(urwid.AttrMap(exit_button, None, focus_map='reversed'))
        self.body.append(urwid.Divider(div_char='~'))
        self.body.append(urwid.Text("Messages:"))
        self.body.append(urwid.Text(self.messages))

        return urwid.ListBox(urwid.SimpleFocusListWalker(self.body))

    def on_not_dest_change(self,edit, new_edit_text):
        self.notify_dest = new_edit_text

    def on_not_msg_change(self,edit, new_edit_text):
        self.notify_msg = new_edit_text

    def notify_handler(self,button, choice):
        palette = [('I say', 'default,bold', 'default', 'bold'),]
        destination = urwid.Edit(('I say', u"Destination: "))
        self.reply_notify_dest = urwid.Text(u"")
        message = urwid.Edit(('I say', u"Message: "))
        self.reply_notify_msg = urwid.Text(u"")
        ret = urwid.Button(u'Notify')
        div = urwid.Divider()
        urwid.connect_signal(destination, 'change', self.on_not_dest_change)
        urwid.connect_signal(message, 'change', self.on_not_msg_change)
        urwid.connect_signal(ret, 'click', self.return_main, "notify")
        pile = urwid.Pile([destination, message,  ret])
        top = urwid.Filler(pile, valign='top')
        self.main.original_widget = top

    def on_pub_topic_change(self,edit, new_edit_text):
        self.pub_topic = new_edit_text

    def on_pub_msg_change(self,edit, new_edit_text):
        self.pub_msg = new_edit_text

    def publish_handler(self,button, choice):
        palette = [('I say', 'default,bold', 'default', 'bold'),]
        topic = urwid.Edit(('I say', u"Topic: "))
        message = urwid.Edit(('I say', u"Message: "))
        ret = urwid.Button(u'Publish')
        div = urwid.Divider()
        urwid.connect_signal(topic, 'change', self.on_pub_topic_change)
        urwid.connect_signal(message, 'change', self.on_pub_msg_change)
        urwid.connect_signal(ret, 'click', self.return_main, "publish")
        pile = urwid.Pile([topic, message,  ret])
        top = urwid.Filler(pile, valign='top')
        self.main.original_widget = top

    def on_sub_topic_change(self,edit, new_edit_text):
        self.sub_topic = new_edit_text

    def on_sub_scope_change(self,edit, new_edit_text):
        self.sub_scope = new_edit_text

    def subscribe_handler(self,button, choice):
        palette = [('I say', 'default,bold', 'default', 'bold'),]
        topic = urwid.Edit(('I say', u"Topic: "))
        scope = urwid.Edit(('I say', u"Scope(all/region/cluster/node/noscope: "))
        ret = urwid.Button(u'Subscribe')
        div = urwid.Divider()
        urwid.connect_signal(topic, 'change', self.on_sub_topic_change)
        urwid.connect_signal(scope, 'change', self.on_sub_scope_change)
        urwid.connect_signal(ret, 'click', self.return_main, "subscribe")
        pile = urwid.Pile([topic, scope,  ret])
        top = urwid.Filler(pile, valign='top')
        self.main.original_widget = top

    def on_unsub_topic_change(self,edit, new_edit_text):
        self.unsub_topic = new_edit_text

    def on_unsub_scope_change(self,edit, new_edit_text):
        self.unsub_scope = new_edit_text

    def unsubscribe_handler(self,button, choice):
        palette = [('I say', 'default,bold', 'default', 'bold'),]
        topic = urwid.Edit(('I say', u"Topic: "))
        scope = urwid.Edit(('I say', u"Scope(all/region/cluster/node/noscope: "))
        ret = urwid.Button(u'Unsubscribe')
        div = urwid.Divider()
        urwid.connect_signal(topic, 'change', self.on_unsub_topic_change)
        urwid.connect_signal(scope, 'change', self.on_unsub_scope_change)
        urwid.connect_signal(ret, 'click', self.return_main, "unsubscribe")
        pile = urwid.Pile([topic, scope,  ret])
        top = urwid.Filler(pile, valign='top')
        self.main.original_widget = top


    def return_main(self,button, cmd='None'):
        dest = 'none'
        msg = 'none'
        if cmd == 'notify':
            self.sendmsg(self.notify_dest,self.notify_msg)

        if cmd == 'publish':
            self.publish(self.pub_topic,self.pub_msg)

        if cmd == 'subscribe':
            substr = "%s/%s"%(self.sub_topic, self.sub_scope)
            self.subscriptions.append(substr)
            self.subscribe(self.sub_topic,self.sub_scope)

        if cmd == 'unsubscribe':
            substr = "%s/%s"%(self.sub_topic, self.sub_scope)
            self.subscriptions.remove(substr)
            self.unsubscribe(self.unsub_topic,self.unsub_scope)

        self.update_main_text()
        self.main.original_widget = self.menu(self.main_text, self.choices)


    def exit_program(self,button):
        self.shutdown()
        raise urwid.ExitMainLoop()


# ClientUnsafe does no encryption nor authentication
class PlainCli(ClientUnsafe):
    def __init__(self, name, dealerurl, customer):
        super().__init__(name, dealerurl, customer)

    def on_data(self, src, msg):
        logging.info("Got message from %s: %s" % (str(src), str(msg)))

    def on_reg(self):
        logging.info('Registered, subscribing to customer topic "test-topic/all"')
        self.sub_scope("test-topic", "all")
        logging.info('Publishing message on "test-topic"')
        self.publish("test-topic", "hello customer clients!")
        logging.info('Publishing message on public topic "test-topic"')
        self.publish("public.test-topic", "hello public clients!")
        logging.info('Sending direct message to client (same customer) "clientA"')
        self.sendmsg("clientA", "Hello clientA!")
        logging.info('Sending direct message to client (public) "clientA"')
        self.sendmsg("public.clientA", "Hello clientA!")

    def on_discon(self):
        logging.info('Lost connection to broker')

    def on_pub(self, src, topic, msg):
        logging.info("Got message on topic %s from %s: %s" % (str(topic), str(src), str(msg)))


    def on_exit_clicked(button):
        raise urwid.ExitMainLoop()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Generic message client")
    parser.add_argument('name', help="Identity of this client")
    parser.add_argument('customer', help="Name of the customer to get the keys (i.e. 'a' for the customer-a.json file)")
    parser.add_argument(
        '-d',
        "--dealer",
        help='URL to connect DEALER socket to, "tcp://1.2.3.4:5555"',
        nargs='?',
        default='tcp://127.0.0.1:5555')
    parser.add_argument(
        '-u',
        "--unsafe",
        action='store_true',
        help='Secure client',
        default=False)
    parser.add_argument(
        '-f',
        "--logfile",
        help='File to write logs to',
        nargs='?',
        default=None)
    parser.add_argument(
        '-l',
        "--loglevel",
        help='Set loglevel (DEBUG, INFO, WARNING, ERROR, CRITICAL)',
        nargs='?',
        default="INFO")

    parser.add_argument(
        '-k',
        "--keyfile",
        help='File containing the encryption/authentication keys)',
        nargs='?',
        default='')

    args = parser.parse_args()

    numeric_level = getattr(logging, args.loglevel.upper(), None)
    if not isinstance(numeric_level, int):
        raise ValueError('Invalid log level: %s' % args.loglevel)
    if args.logfile:
        logging.basicConfig(format='%(levelname)s:%(message)s', filename=args.logfile, level=numeric_level)
    else:
        logging.basicConfig(format='%(levelname)s:%(message)s', filename=args.logfile, level=numeric_level)

    if args.unsafe == True:
        logging.info("Unsafe client")
        genclient = PlainCli(name=args.name, dealerurl=args.dealer, customer=args.customer)

    else:
        logging.info("Safe client")
        genclient = SecureCli(name=args.name, dealerurl=args.dealer, customer=args.customer,keyfile=args.keyfile)

    logging.info("Starting DoubleDecker example client")
    logging.info("See ddclient.py for how to send/recive and publish/subscribe")
    genclient.run()
