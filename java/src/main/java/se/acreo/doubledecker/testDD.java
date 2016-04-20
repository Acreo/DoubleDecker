/*
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
*/
package se.acreo.doubledecker;

import asg.cliche.Command;
import asg.cliche.Param;
import asg.cliche.Shell;
import asg.cliche.ShellFactory;
import org.apache.commons.cli.*;

import java.io.IOException;
import java.util.HashMap;
import java.util.List;
import java.util.Observable;
import java.util.Observer;


public class testDD implements Observer, DDEvents {
    DDClient client = null;

    public testDD(String dealer, String keyfile, String name, String customer) {
        try {

            client = new DDClient(dealer, name, true, this, keyfile);
        } catch (IOException e) {
            e.printStackTrace();
            System.exit(1);
        }
        Thread thread = new Thread(client);
        thread.start();
    }

    public static void main(String[] args) throws IOException {
        String dealer = null;
        String keyfile = null;
        String name = null;
        String customer = null;

        CommandLineParser parser = new DefaultParser();

        Options options = new Options();
        options.addOption("d", "dealer", true, "Which dealer to connect to");
        options.addOption("k", "keyfile", true, "Keyfile to read");
        options.addOption("n", "name", true, "Client name");
        options.addOption("c", "customer", true, "Customer name");

        try {
            System.out.println("Parsing command like arguments..");
            CommandLine line = parser.parse(options, args);
            if (line.hasOption("dealer")) {
                dealer = line.getOptionValue("dealer");
            } else {
                System.out.println("Missing required parameter: dealer");
                System.exit(1);
            }
            if (line.hasOption("keyfile")) {
                keyfile = line.getOptionValue("keyfile");
            } else {
                System.out.println("Missing required parameter: keyfile");
                System.exit(1);
            }
            if (line.hasOption("name")) {
                name = line.getOptionValue("name");
            } else {
                System.out.println("Missing required parameter: name");
                System.exit(1);
            }
            if (line.hasOption("customer")) {
                customer = line.getOptionValue("customer");
            } else {
                System.out.println("Missing required parameter: customer");
                System.exit(1);
            }

        } catch (ParseException exp) {
            System.out.println("Unexpected exception:" + exp.getMessage());
        }


        Shell shell = ShellFactory.createConsoleShell("DD", "DoubleDecker java test client",
                new testDD(dealer, keyfile, name, customer));
        shell.commandLoop();
    }

    /*@Command(description = "Connect to a broker")
    public void connect(

    }
*/
    @Command(description = "Get status of the DD connection")
    public String status() {
        if (client == null) {
            return "Not started";
        }
        if (client.getStatus() == DDClient.CliState.REGISTERED) {
            return "Registered";
        } else {
            return "Connecting";
        }
    }

    @Command(description = "List subscriptions and their status")
    public String subscriptions() {
        if (client == null) {
            return "Not started";
        }
        StringBuilder sb = new StringBuilder();
        HashMap<List<String>, Boolean> list = client.sublistGet();
        for (List<String> l : list.keySet()) {
            System.out.println("Subscriptions: " + l);
            sb.append("Sub: " + l.get(0) + " " + l.get(1) + "\tActive: " + list.get(l) + "\n");
        }
        return sb.toString();
    }

    @Command(description = "Send a notifcation to a client")
    public String notify(
            @Param(name = "Client", description = "Destination of the notification message")
            String target,
            @Param(name = "Message", description = "The message to send")
            String message) {
        if (client != null) {
            client.sendmsg(target, message);
            return "Sent message!";
        }
        return "Connect first!";
    }

    @Command(description = "Subscribe to a topic")
    public String subscribe(
            @Param(name = "Topic", description = "The topic to subscribe to (place $ at the end for prefix topic)")
            String topic,
            @Param(name = "Scope", description = "all/region/cluster/node or /0/2/3/")
            String scope) {
        if (client != null) {
            client.subscribe(topic, scope);
            return "Subscribed!";
        }
        return "Connect first!";
    }

    @Command(description = "Unsubscribe from a topic")
    public String unsubscribe(
            @Param(name = "Topic", description = "The topic to subscribe to (place $ at the end for prefix topic)")
            String topic,
            @Param(name = "Scope", description = "all/region/cluster/node or /0/2/3/")
            String scope) {
        if (client != null) {
            if(client.unsubscribe(topic, scope))
                return "Subscribed!";
            else
                return "Failure";
        }
        return "Connect first!";
    }

    @Command(description = "Publish a message to a topic")
    public String publish(
            @Param(name = "Topic", description = "The topic to post to")
            String topic,
            @Param(name = "Message", description = "The message to post")
            String message) {
        if (client != null) {
            client.publish(topic, message);
            return "Published!";
        }
        return "Connect first!";
    }

    @Command(description = "Terminate the testclient")
    public void quit() {
        if (client != null) {
            System.out.println("Unregistering from broker");
            client.shutdown();
        }
        System.out.println("Bye!");
        System.exit(0);
        return;
    }


    // Go for the Observer pattern like this or with the DDEvents approach?
    @Override
    public void update(Observable obj, Object arg) {
        if (arg instanceof DDMsg) {
            System.out.println("testDD got message: " + arg.toString());
        }
    }

    @Override
    public void registered(String endpoint) {
        System.out.println("Test DD registered: " + endpoint);
    }

    @Override
    public void disconnected(String endpoint) {

        System.out.println("Disconnected: " + endpoint);
    }

    @Override
    public void publish(String source, String topic, byte[] data) {
        System.out.println("PUBLISH src: " + source + " topic: " + topic + " d: " + new String(data));
    }

    @Override
    public void data(String source, byte[] data) {
        System.out.println("DATA src: " + source + " d: " + new String(data));
    }

    @Override
    public void error(int code, String reason) {
        switch (code) {
            case DDClient.ERROR.NODST:
                System.out.println("Destination client: " + reason + " is not registered!");
                break;
            case DDClient.ERROR.REGFAIL:
                System.out.println("Registration failed, client with same name is connected " + reason);
                break;
            case DDClient.ERROR.VERSION:
                System.out.println("Fatal error, wrong doubledecker protocol version!");
                break;
            default:
                System.out.println("Unknown error code: " + code + " reason: " + reason);
                break;
        }
    }
}
