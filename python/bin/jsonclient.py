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
from doubledecker.clientSafe import ClientSafe
import json
import sys

from collections import deque

# Example client taking JSON commands as input on STDIN
# And printing incoming messages as JSON to STDOUT

class SecureCli(ClientSafe):
    def __init__(self, name, dealerurl, customer, keyfile,topics):
        super().__init__(name, dealerurl, customer, keyfile)
        self.mytopics = list()

        try:
            for top in topics.split(","):
                if len(top) == 0:
                    return
                (topic,scope) = top.split("/")
                self.mytopics.append((topic,scope))
        except ValueError:
            logging.error("Could not parse topics!")
            sys.exit(1)


    def on_data(self, src, data):
        msg = dict()
        msg["type"] = "data"
        msg["src"] = src.decode()
        msg["data"] = data.decode()
        print(json.dumps(msg))

    def on_reg(self):
        msg = dict()
        msg["type"] = "reg"
        print(json.dumps(msg))
        for topic in self.mytopics:
            self.subscribe(*topic)


    def on_discon(self):
        msg = dict()
        msg["type"] = "discon"
        print(json.dumps(msg))


    def on_error(self, code, error):
        msg = dict()
        msg["type"] = "error"
        msg["code"] = code.decode()
        msg["error"] = error.decode()
        print(json.dumps(msg))

    def on_pub(self, src, topic, data):
        msg = dict()
        msg["type"] = "pub"
        msg["src"] = src.decode()
        msg["topic"] = topic.decode()
        msg["data"] = data.decode()
        print(json.dumps(msg))

    def on_stdin(self,fp,*kargs):
        data = fp.readline()
        try:
            res = json.loads(data)
        except ValueError as e:
            res = dict()

        if "type" in res:
            if res['type'] == "notify":
                self.sendmsg(res['dst'],res['data'])
            elif res['type'] == "pub":
                self.publish(res['topic'], res['data'])
            elif res['type'] == "sub":
                self.subscribe(res['topc'],res['scope'])
            else:
                print(json.dumps({"type":"error", "data":"Command '%s' not implemented"%res['type']}))
        else:
            print(json.dumps({"type":"error", "data":"Couldn't parse JSON"}))

    def run(self):
        self.start()


    def exit_program(self,button):
        self.shutdown()


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

    parser.add_argument(
        '-t',
        "--topics",
        help='Comma separated list of topics/scopes, e.g. "topic1/all,topic2/node"',
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

    logging.info("Safe client")
    genclient = SecureCli(name=args.name, dealerurl=args.dealer, customer=args.customer,keyfile=args.keyfile, topics=args.topics)

    logging.info("Starting DoubleDecker example client")
    logging.info("See ddclient.py for how to send/recive and publish/subscribe")
    genclient._IOLoop.add_handler(sys.stdin,genclient.on_stdin,genclient._IOLoop.READ)
    genclient.run()
