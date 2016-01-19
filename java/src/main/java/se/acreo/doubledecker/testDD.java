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

import java.io.IOException;
import java.util.Observable;
import java.util.Observer;
import asg.cliche.Command;
import asg.cliche.Param;
import asg.cliche.ShellFactory;


public class testDD implements Observer, DDEvents {
    DDClient client = null;
    public testDD(){
    }
    @Command(description="Connect to a broker")
    public void connect(
            @Param(name="BrokerURL", description="URL the broker is listening on (e.g. tcp://localhost:5555")
            String brokerUrl,
            @Param(name="ClientName", description="Name of of the client (e.g. bob)")
            String name){
        try {
            client = new DDClient(brokerUrl, name, true, this, "/etc/doubledecker/a-keys.json");
        } catch (IOException e) {
            e.printStackTrace();
            System.exit(1);
        }

        // we're the observer, subscribe us to the event source
        // can be more observers per client
        // starts the event thread

        Thread thread = new Thread(client);
        thread.start();
    }


    @Command(description = "Get status of the DD connection")
    public String status(){
        if(client == null){
            return "Not started";
        }
        if(client.getStatus() == DDClient.CliState.REGISTERED){
            return "Registered";
        } else {
            return "Connecting";
        }
    }

    @Command(description = "Send a notifcation to a client")
    public String notify(
            @Param(name = "Client",description = "Destination of the notification message")
            String target,
            @Param(name = "Message", description = "The message to send")
            String message){
        if(client != null) {
            client.sendmsg(target, message);
            return "Sent message!";
        }
        return "Connect first!";
    }

    @Command
    public String subscribe(String topic, String scope){
        if(client != null) {
            client.subscribe(topic, scope);
            return "Subscribed!";
        }
        return "Connect first!";
    }

    @Command
    public String unsubscribe(String topic, String scope){
        if(client != null) {
            client.unsubscribe(topic, scope);
            return "Subscribed!";
        }
        return "Connect first!";
    }

    @Command
    public String publish(String topic, String message){
        if(client != null) {
            client.publish(topic, message);
            return "Published!";
        }
        return "Connect first!";
    }

    // Go for the Observer pattern like this or with the DDEvents approach?
    @Override
    public void update(Observable obj, Object arg) {
         if (arg instanceof DDMsg) {
            System.out.println("testDD got message: " + arg.toString());
        }
    }

    public static void main(String[] args) throws IOException {
        ShellFactory.createConsoleShell("hello", "", new testDD())
                .commandLoop(); // and three.
    }

    @Override
    public void registered(String endpoint) {
        System.out.println("Test DD registered: " + endpoint);
    }

    @Override
    public void disconnected(String endpoint) {

        System.out.println("disconnected: "+ endpoint);
    }

    @Override
    public void publish(String source, String topic, byte[] data) {
        System.out.println("publish s: " + source + " t: " + topic + " d: " + new String(data));
    }

    @Override
    public void data(String source, byte[] data) {
        System.out.println("Data s: " + source + " d: " + new String(data));
    }

    @Override
    public void error(int code, String reason) {
        System.out.println("error c: " + code + " reason: " + reason);
    }
}
