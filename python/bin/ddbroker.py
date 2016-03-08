# #!/usr/bin/python3
# #  -*- coding: utf-8 -*-
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

from doubledecker.broker import BrokerSafe
from doubledecker.broker import BrokerUnsafe

if '__main__' == __name__:
    logging.critical('\t\t\033[91m-----------------------------------------------\n\
                     \tThis version of the broker is deprecated and you\n\
                     \tshould use the C version\n\
                     \t-----------------------------------------------\033[0m')
    parser = argparse.ArgumentParser(description='Generic message broker')
    parser.add_argument('name', help='Identity of this broker')
    parser.add_argument('-r',
                        '--router',
                        help='URL to bind ROUTER socket to, e.g. "tcp://*:5555"',
                        nargs='?',
                        default='tcp://*:5555')
    parser.add_argument('-d',
                        '--dealer',
                        help='URL to connect DEALER socket to, "tcp://1.2.3.4:5558"',
                        nargs='?',
                        default='')
    parser.add_argument('-v',
                        '--verbose',
                        action='store_true',
                        help="Make the broker verbose (strongly inadvisalbe with Docker)",
                        default=False)
    parser.add_argument('-p',
                        '--ps',
                        action='store_false',
                        help="Disable pubsub",
                        default=True)
    parser.add_argument(
                        '-u',
                        "--unsafe",
                        action='store_true',
                        help='Unsecure client',
                        default=False)
    parser.add_argument(
                        '-s',
                        "--scope",
                        help='Scope of the broker, e.g. "1/2/3"',
                        nargs='?',
                        default='0/0/0')
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
                        "--keys",
                        help='Location of the broker keys file',
                        nargs='?',
                        default="broker-keys.json")

    args_list = parser.parse_args()
    numeric_level = getattr(logging, args_list.loglevel.upper(), None)
    if not isinstance(numeric_level, int):
        raise ValueError('Invalid log level: %s' % args_list.loglevel)

    if args_list.verbose:
        logging.warning("Verbose option is deprecated, use loglevel instead")

    if args_list.logfile:
        logging.basicConfig(format='%(levelname)s:%(message)s',filename=args_list.logfile, level=numeric_level)
    else:
        logging.basicConfig(format='%(levelname)s:%(message)s',filename=args_list.logfile, level=numeric_level)

    if args_list.unsafe==True:
        logging.info("Starting an unsecure broker")
        genbroker = BrokerUnsafe(name=args_list.name.encode('utf8'),
                                                routerurl=args_list.router,
                                                dealerurl=args_list.dealer,
                                                scope=args_list.scope)
    else:
        logging.info("Starting a secure broker")
        genbroker = BrokerSafe(name=args_list.name.encode('utf8'),
                                            routerurl=args_list.router,
                                            dealerurl=args_list.dealer,
                                            scope=args_list.scope,
                                            keys=args_list.keys)

    genbroker.start()
    logging.info("Broker stopped")
