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

import com.google.gson.FieldNamingStrategy;
import com.google.gson.Gson;
import com.google.gson.reflect.TypeToken;
import org.abstractj.kalium.NaCl;
import org.abstractj.kalium.crypto.Box;
import org.zeromq.ZContext;
import org.zeromq.ZFrame;
import org.zeromq.ZMQ;
import org.zeromq.ZMsg;
import sun.misc.BASE64Decoder;

import java.io.FileReader;
import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.Type;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.*;

public class DDClient extends Thread {
    private final String keyfile;
    private CliState cliState = CliState.UNREG;
    private String broker, hash, name;
    private ZContext ctx;
    private ZMQ.Socket socket = null;
    private Formatter log;
    private int timeout = 0;
    private Box tenantBox, brokerBox, publicBox;
    private Registration registrationThread;
    private HeartBeat heartBeatThread;
    private int cookie;
    private byte[] bcookie;
    private DDEvents callback;
    private byte[] nonce;
    private byte[] pubkey, privkey, ddpubkey, publicpubkey;
    private HashMap<List<String>, Boolean> sublist = new HashMap<>();


    public DDClient(String broker, String name, boolean verbose, DDEvents callback, String keyfile) throws IOException {
        this.cliState = CliState.UNREG;

        this.broker = broker;
        this.callback = callback;
        this.name = name;
        this.keyfile = keyfile;
        org.abstractj.kalium.crypto.Random rnd = new org.abstractj.kalium.crypto.Random();
        this.nonce = rnd.randomBytes(org.abstractj.kalium.NaCl.Sodium.NONCE_BYTES);

        if (verbose) {
            log = new Formatter(System.out);
        } else {
            log = new Formatter();
            log.format("");
        }

        Gson gson = new Gson();
        Type stringStringMap = new TypeToken<Map<String, String>>() {
        }.getType();
        Map<String, String> map = gson.fromJson(new FileReader(this.keyfile), stringStringMap);
        BASE64Decoder b64 = new BASE64Decoder();
        privkey = b64.decodeBuffer(map.get("privkey"));
        pubkey = b64.decodeBuffer(map.get("pubkey"));
        ddpubkey = b64.decodeBuffer(map.get("ddpubkey"));
        publicpubkey = b64.decodeBuffer(map.get("publicpubkey"));
        hash = map.get("hash");
        this.tenantBox = new Box(pubkey, privkey);
        this.brokerBox = new Box(ddpubkey, privkey);
        this.publicBox = new Box(publicpubkey, privkey);

        ctx = new ZContext();
        socket = ctx.createSocket(ZMQ.DEALER);
        socket.connect(broker);
        this.registrationThread = new Registration(this.hash);
        registrationThread.start();
    }

    private void sublistAdd(String topic, String scope, boolean active) {

        if (sublist.containsKey(Arrays.asList(topic, scope))) {
            log.format("DD: subscription for " + topic + scope + " already in list!\n");
            return;
        }
        sublist.put(Arrays.asList(topic, scope), active);
    }

    private void sublistDel(String topic, String scope) {
        if (sublist.containsKey(Arrays.asList(topic,scope))) {
            log.format("DD: removing subscription for " + topic + scope + "\n");
            sublist.remove(Arrays.asList(topic, scope));
        } else {
            log.format("DD: trying to unsubscribe from unexisting " + topic + scope + "\n");
        }
    }

    private void sublistActivate(String topic, String scope) {
        if (sublist.containsKey(Arrays.asList(topic, scope))) {
            sublist.put(Arrays.asList(topic, scope), true);
        } else {
            log.format("DD: Trying to activate non-existing " + topic + scope + "\n");
        }
    }

    private void sublistInactivateAll() {
        Set<List<String>> subscriptions = sublist.keySet();
        for (List l : subscriptions) {
            sublist.put(l, false);
        }
    }
    public HashMap<List<String>, Boolean> sublistGet(){
        return (HashMap<List<String>, Boolean>) sublist.clone();
    }

    private void incrementNonce() {
        for (int i = this.nonce.length - 1; i >= 0; --i) {
            if (this.nonce[i] == -1) {// -1 is all 1-bits, which is the unsigned maximum
                this.nonce[i] = 0;
            } else {
                ++this.nonce[i];
                return;
            }
        }
        // we maxed out the array
        for (int i = this.nonce.length - 1; i >= 0; --i) {
            this.nonce[i] = 0;
        }
    }

    public synchronized boolean sendmsg(String target, byte[] message) {

        boolean srcpublic = false;
        boolean dstpublic = false;

        // TODO: fix public tenants
        // srcpublic = this.tenant.equals("public");

        dstpublic = target.startsWith("public.");

        /* TODO special cases for different public tenants
        char *dot = strchr(target, '.');
        int retval;
        if (dot && srcpublic) {
            *dot = '\0';
            precalck = zhash_lookup(dd->keys->clientkeys, target);
            if (precalck) {
            }
            *dot = '.';
        }
        */
        byte[] ciphertext;

        if (dstpublic) {
            incrementNonce();
            byte[] res = this.publicBox.encrypt(this.nonce, message);
            ciphertext = Arrays.copyOf(this.nonce, this.nonce.length + res.length );
            int j = 0;
            for (int i = this.nonce.length; i < this.nonce.length+res.length; i++){
                ciphertext[i] = res[j];
                j++;
            }

        } else {
            incrementNonce();
            byte[] res = this.tenantBox.encrypt(this.nonce, message);
            ciphertext = Arrays.copyOf(this.nonce, this.nonce.length + res.length );
            int j = 0;
            for (int i = this.nonce.length; i < this.nonce.length+res.length; i++){
                ciphertext[i] = res[j];
                j++;
            }
        }

        if (this.cliState == cliState.REGISTERED) {
            ZMsg tosend = new ZMsg();
            tosend.addFirst(CMD.bprotoVersion);
            tosend.add(CMD.bSEND);
            tosend.add(this.bcookie);
            tosend.add(target);
            tosend.add(ciphertext);
            tosend.send(socket);
            return true;
        } else {
            log.format("DD: Couldn't send, not registered!");
            return false;
        }
    }

    public synchronized boolean sendmsg(String target, String message) {
        return sendmsg(target, message.getBytes());
    }

    public synchronized boolean publish(String topic, byte[] message) {

        boolean srcpublic = false;
        boolean dstpublic = false;

        // TODO: fix public tenants
        // srcpublic = this.tenant.equals("public");

        dstpublic = topic.startsWith("public.");

        /* TODO special cases for different public tenants
        char *dot = strchr(target, '.');
        int retval;
        if (dot && srcpublic) {
            *dot = '\0';
            precalck = zhash_lookup(dd->keys->clientkeys, target);
            if (precalck) {
            }
            *dot = '.';
        }
        */
        byte[] ciphertext;
        if (dstpublic) {
            incrementNonce();
            byte[] res = this.publicBox.encrypt(this.nonce, message);
            ciphertext = Arrays.copyOf(this.nonce, this.nonce.length + res.length );
            int j = 0;
            for (int i = this.nonce.length; i < this.nonce.length+res.length; i++){
                ciphertext[i] = res[j];
                j++;
            }
        } else {
            incrementNonce();
            byte[] res = this.tenantBox.encrypt(this.nonce, message);
            ciphertext = Arrays.copyOf(this.nonce, this.nonce.length + res.length );
            int j = 0;
            for (int i = this.nonce.length; i < this.nonce.length+res.length; i++){
                ciphertext[i] = res[j];
                j++;
            }
        }

        if (this.cliState == cliState.REGISTERED) {
            ZMsg tosend = new ZMsg();
            tosend.addFirst(CMD.bprotoVersion);
            tosend.add(CMD.bPUB);
            tosend.add(this.bcookie);
            tosend.add(topic);
            tosend.add("");
            tosend.add(ciphertext);
            log.format("DD: Publishing on topic " + topic + " : " + tosend.toString() + "\n");
            tosend.send(socket);
            return true;
        } else {
            log.format("DD: Trying to publish while not connected");
            return false;
        }
    }

    public synchronized boolean publish(String topic, String message) {
        return publish(topic, message.getBytes());
    }

    public synchronized CliState getStatus() {
        return this.cliState;
    }

    public synchronized boolean subscribe(String topic, String scope) {
        String scopestr;
        if (scope.equals("all")) {
            scopestr = "/";
        } else if (scope.equals("region")) {
            scopestr = "/*/";
        } else if (scope.equals("cluster")) {
            scopestr = "/*/*/";
        } else if (scope.equals("node")) {
            scopestr = "/*/*/*/";
        } else if (scope.equals("noscope")) {
            scopestr = "noscope";
        } else {
            // TODO
            // check that scope follows re.fullmatch("/((\d)+/)+", scope):
            scopestr = scope;
        }
        sublistAdd(topic, scopestr, false);
        if (this.cliState == CliState.REGISTERED) {
            ZMsg tosend = new ZMsg();
            tosend.addFirst(CMD.bprotoVersion);
            tosend.add(CMD.bSUB);
            tosend.add(this.bcookie);
            tosend.add(topic);
            tosend.add(scopestr);
            tosend.send(socket);
            return true;
        } else {
            log.format("DD: Couldn't subscribe, not connected!");
            return false;
        }
    }

    public synchronized boolean unsubscribe(String topic, String scope) {
        String scopestr;
        if (scope.equals("all")) {
            scopestr = "/";
        } else if (scope.equals("region")) {
            scopestr = "/*/";
        } else if (scope.equals("cluster")) {
            scopestr = "/*/*/";
        } else if (scope.equals("node")) {
            scopestr = "/*/*/*/";
        } else if (scope.equals("noscope")) {
            scopestr = "noscope";
        } else {
            // TODO
            // check that scope follows re.fullmatch("/((\d)+/)+", scope):
            scopestr = scope;
        }
        sublistDel(topic, scopestr);
        if (this.cliState == CliState.REGISTERED) {
            ZMsg tosend = new ZMsg();
            tosend.addFirst(CMD.bprotoVersion);
            tosend.add(CMD.bUNSUB);
            tosend.add(this.bcookie);
            tosend.add(topic);
            tosend.add(scopestr);
            tosend.send(socket);
            return true;
        } else {
            log.format("DD: Couldn't unsubscribe, not connected.");
            return false;
        }
    }

    public synchronized void shutdown(){
        ZMsg tosend = new ZMsg();
        tosend.addFirst(CMD.bprotoVersion);
        tosend.add(CMD.bUNREG);
        tosend.add(this.bcookie);
        tosend.add(this.name);
        tosend.send(socket);
        log.format("DD: Unregistering..");
        if(this.registrationThread != null) {
            if (this.registrationThread.isAlive()) {
                this.registrationThread.interrupt();
            }
        }

        if(this.heartBeatThread != null) {
            if (this.heartBeatThread.isAlive()) {
                this.heartBeatThread.interrupt();
            }
        }
    }

    @Override
    protected void finalize() throws Throwable {
        super.finalize();
        log.format("DD: Cleaning up before closing\n");
        socket.close();
        ctx.destroy();
    }

    @Override
    public void run() {
        // Wait for new messages, receive them, and process
        while (!Thread.currentThread().isInterrupted()) {
            ZMQ.Poller items = new ZMQ.Poller(1);
            items.register(socket, ZMQ.Poller.POLLIN);

            if (items.poll(1000) == -1) {
                log.format("items.poll() returned -1\n");
                break;
            }
            if (items.pollin(0)) {
                processMessage(ZMsg.recvMsg(socket));
            }
        }
    }

    private void processMessage(ZMsg msg) {
        if (msg == null) {
            log.format("DD: received null message!\n");
            return;
        }
        // check number of frames
        if (msg.size() < 2) {
            log.format("DD: Message length less than 2, error!\n");
            return;
        }

        ZFrame protoVersion = msg.pop();
        if (!Arrays.equals(protoVersion.getData(), CMD.bprotoVersion)) {
            log.format("DD: different protocols in use :\nExpected :"
                    + CMD.bprotoVersion + "\n");
            return;
        }

        int commandFrame = ByteBuffer.wrap(msg.pop().getData()).order(ByteOrder.LITTLE_ENDIAN).getInt();

        if (commandFrame < 0) {
            log.format("DD: Unknown command received: " + commandFrame);
            return;
        }
        // Timeout is updated only on valid messages
        switch (commandFrame) {
            // Expected commands
            case CMD.REGOK:
                cmd_cb_regok(msg);
                timeout = 0;
                break;
            case CMD.DATA:
                cmd_cb_data(msg);
                timeout = 0;
                break;
            case CMD.ERROR:
                cmd_cb_error(msg);
                timeout = 0;
                break;
            case CMD.PONG:
                cmd_cb_pong(msg);
                timeout = 0;
                break;
            case CMD.CHALL:
                cmd_cb_chall(msg);
                timeout = 0;
                break;
            case CMD.PUB:
                cmd_cb_pub(msg);
                timeout = 0;
                break;
            case CMD.SUBOK:
                cmd_cb_subok(msg);
                timeout = 0;
                break;
            // Unexpected commands
            case CMD.SEND:
                log.format("DD: Received unexpected SEND\n");
                break;
            case CMD.FORWARD:
                log.format("DD: Received unexpected FORWARD\n");
                break;
            case CMD.PING:
                log.format("DD: Received unexpected PING\n");
                break;
            case CMD.ADDLCL:
                log.format("DD: Received unexpected ADDLCL\n");
                break;
            case CMD.ADDBR:
                log.format("DD: Received unexpected ADDBR\n");
                break;
            case CMD.ADDDCL:
                log.format("DD: Received unexpected ADDDCL\n");
                break;
            case CMD.UNREG:
                log.format("DD: Received unexpected UNREG\n");
                break;
            case CMD.UNREGBR:
                log.format("DD: Received unexpected UNREGBR\n");
                break;
            case CMD.UNREGDCLI:
                log.format("DD: Received unexpected UNREGDCLI\n");
                break;
            case CMD.SUB:
                log.format("DD: Received unexpected SUB\n");
                break;
            case CMD.UNSUB:
                log.format("DD: Received unexpected UNSUB\n");
                break;
            case CMD.SENDPUBLIC:
                log.format("DD: Received unexpected SENDPUBLIC\n");
                break;
            case CMD.PUBPUBLIC:
                log.format("DD: Received unexpected PUBPUBLIC\n");
                break;
            case CMD.SENDPT:
                log.format("DD: Received unexpected SENDPT\n");
                break;
            case CMD.FORWARDPT:
                log.format("DD: Received unexpected FORWARDPT\n");
                break;
            case CMD.DATAPT:
                log.format("DD: Received unexpected DATAPT\n");
                break;
            default:
                log.format("DD: Got unknown command: " + commandFrame);
                break;
        }
    }

    private void cmd_cb_regok(ZMsg msg) {
        ZFrame cookieFrame = msg.pop();
        if (cookieFrame == null) {
            log.format("DD: REGOK message malformed, missing cookie!\n");
            return;
        }
        this.cookie = ByteBuffer.wrap(cookieFrame.getData()).getInt();
        this.bcookie = cookieFrame.getData().clone();
        this.cliState = CliState.REGISTERED;
        log.format("DD: New cookie: " + this.bcookie + "\n");
        // Start the heartbeat with the new cookie
        registrationThread.interrupt();
        heartBeatThread = new HeartBeat(this.bcookie);
        heartBeatThread.start();

        resubscribe();
//        log.format("DD: Registered with broker: " + this.broker + "\n");
        this.callback.registered(this.broker);
    }

    private void resubscribe() {
        for (List<String> l : sublist.keySet()){
            log.format("resubscribe("+l.get(0) + " " +l.get(1)+")\n");

          /*  ZMsg tosend = new ZMsg();
            tosend.addFirst(CMD.bprotoVersion);
            tosend.add(CMD.bSUB);
            tosend.add(this.bcookie);
            tosend.add(topic);
            tosend.add(scopestr);
            tosend.send(socket);
            */
        }
    }

    private void cmd_cb_data(ZMsg msg) {
        int retval;
        String source = msg.popString();
        ZFrame encrypted = msg.pop();


        /* TODO: Special case for public clients with multiple keys
        int enclen = zframe_size(encrypted);
        unsigned char *decrypted =
                calloc(1, enclen - crypto_box_NONCEBYTES - crypto_box_MACBYTES);
        unsigned char *precalck = NULL;
        char *dot = strchr(source, '.');
        if (dot) {
            *dot = '\0';
            precalck = zhash_lookup(dd->keys->clientkeys, source);
            if (precalck) {
                // printf("decrypting with tenant key:%s\n", source);
            }
            *dot = '.';
        }
        */

        byte[] plaintext;
        byte[] enc = encrypted.getData();
        int enclen = enc.length;
        if (enclen < NaCl.Sodium.NONCE_BYTES) {
            log.format("DD: Challenge smaller than NONCE, error!\n");
            return;
        }

        byte[] nonce = Arrays.copyOfRange(enc, 0, org.abstractj.kalium.NaCl.Sodium.NONCE_BYTES);
        byte[] ciphertext = Arrays.copyOfRange(enc, org.abstractj.kalium.NaCl.Sodium.NONCE_BYTES, enclen);

        if (source.startsWith("public.")) {
            plaintext = publicBox.decrypt(nonce, ciphertext);
        } else {
            plaintext = tenantBox.decrypt(nonce, ciphertext);
        }

        callback.data(source, plaintext);
    }

    private void cmd_cb_pub(ZMsg msg) {
     //   log.format("Got message: " + msg);
        String source = msg.popString();
        String topic = msg.popString();
        ZFrame encrypted = msg.pop();

        byte[] plaintext;
        byte[] enc = encrypted.getData();
        int enclen = enc.length;
        if (enclen < NaCl.Sodium.NONCE_BYTES) {
            log.format("DD: Challenge smaller than NONCE, error!\n");
            return;
        }


        byte[] nonce = Arrays.copyOfRange(enc, 0, org.abstractj.kalium.NaCl.Sodium.NONCE_BYTES);
        byte[] ciphertext = Arrays.copyOfRange(enc, org.abstractj.kalium.NaCl.Sodium.NONCE_BYTES, enclen);

/* TODO: Special case for public clients
        char *dot = strchr(source, '.');
        if (dot) {
            *dot = '\0';
            precalck = zhash_lookup(dd->keys->clientkeys, source);
            if (precalck) {
                //	printf("decrypting with tenant key:%s\n", source);
            }
            *dot = '.';
        }
*/

        if (source.startsWith("public.")) {
            plaintext = publicBox.decrypt(nonce, ciphertext);
        } else {
            plaintext = tenantBox.decrypt(nonce, ciphertext);
        }
        callback.publish(source, topic, plaintext);
    }

    private void cmd_cb_chall(ZMsg msg) {
        ZFrame encrypted = msg.pop();
        if (encrypted == null) {
            log.format("DD: Error, empty CHALL!\n");
            return;
        }
        byte[] enc = encrypted.getData();
        int enclen = enc.length;
        if (enclen < NaCl.Sodium.NONCE_BYTES) {
            log.format("DD: Challenge smaller than NONCE, error!\n");
            return;
        }

        byte[] nonce = Arrays.copyOfRange(enc, 0, org.abstractj.kalium.NaCl.Sodium.NONCE_BYTES);
        byte[] ciphertext = Arrays.copyOfRange(enc, org.abstractj.kalium.NaCl.Sodium.NONCE_BYTES, enclen);
        byte[] plaintext = brokerBox.decrypt(nonce, ciphertext);
        // TODO, how to check if decryption failed?

        ZMsg tosend = new ZMsg();
        tosend.addFirst(CMD.bprotoVersion);
        tosend.add(CMD.bCHALLOK);
        tosend.add(plaintext);
        tosend.add(this.hash);
        tosend.add(this.name);
        tosend.send(socket);
    }

    private void cmd_cb_pong(ZMsg msg) {
      //  log.format("DD: cmd_cb_pong called\n");
    }

    private void cmd_cb_error(ZMsg msg) {
    //    log.format("DD: cmd_cb_error called\n");
        int code = ByteBuffer.wrap(msg.pop().getData()).order(ByteOrder.LITTLE_ENDIAN).getInt();
        String reason = msg.popString();
        switch (code){
            case ERROR.NODST:
                callback.error(ERROR.NODST, reason);
                break;
            case ERROR.REGFAIL:
                callback.error(ERROR.REGFAIL, reason);
               // log.format("DD: Registration failed: " + reason + "\n");
               // log.format("DD: Terminating...\n");
               // shutdown();
                break;
            case ERROR.VERSION:
                callback.error(ERROR.VERSION, reason);
                log.format("DD: Version mismatch: " + reason+ "\n");
                log.format("DD: Terminating...\n");
                shutdown();
                break;
            default:
                log.format("DD: Unknown error code " + code + ". Message: " + reason);
        }
    }

    private void cmd_cb_subok(ZMsg msg) {
        String topic = msg.popString();
        String scope = msg.popString();
        sublistActivate(topic, scope);
    }
    protected void listThreads(){
        if(1==1)
            return;

        Set<Thread> threadSet = Thread.getAllStackTraces().keySet();
        for (Thread t : threadSet){
            log.format("Thread " + t.getName() + " id " + t.getId() + " state " + t.getState() + "\n");
        }
    }

    protected enum CliState {
        REGISTERED,
        UNREG
    }
    public static class ERROR {
        protected final static int REGFAIL  = 1;
        protected final static int NODST  = 2;
        protected final static int VERSION  = 3;
    }
    private static class CMD {
        protected final static int SEND = 0;
        protected final static int FORWARD = 1;
        protected final static int PING = 2;
        protected final static int ADDLCL = 3;
        protected final static int ADDDCL = 4;
        protected final static int ADDBR = 5;
        protected final static int UNREG = 6;
        protected final static int UNREGDCLI = 7;
        protected final static int UNREGBR = 8;
        protected final static int DATA = 9;
        protected final static int ERROR = 10;
        protected final static int REGOK = 11;
        protected final static int PONG = 12;
        protected final static int CHALL = 13;
        protected final static int CHALLOK = 14;
        protected final static int PUB = 15;
        protected final static int SUB = 16;
        protected final static int UNSUB = 17;
        protected final static int SENDPUBLIC = 18;
        protected final static int PUBPUBLIC = 19;
        protected final static int SENDPT = 20;
        protected final static int FORWARDPT = 21;
        protected final static int DATAPT = 22;
        protected final static int SUBOK = 23;


        protected final static byte[] bprotoVersion = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(0x0d0d0003).array();
        protected final static byte[] bSEND = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(SEND).array();
        protected final static byte[] bFORWARD = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(FORWARD).array();
        protected final static byte[] bADDLCL = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(ADDLCL).array();
        protected final static byte[] bADDDCL = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(ADDDCL).array();
        protected final static byte[] bADDBR = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(ADDBR).array();
        protected final static byte[] bUNREG = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(UNREG).array();
        protected final static byte[] bUNREGDCLI = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(UNREGDCLI).array();
        protected final static byte[] bUNREGBR = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(UNREGBR).array();
        protected final static byte[] bDATA = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(DATA).array();
        protected final static byte[] bREGOK = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(REGOK).array();
        protected final static byte[] bPONG = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(PONG).array();
        protected final static byte[] bPING = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(PING).array();
        protected final static byte[] bCHALL = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(CHALL).array();
        protected final static byte[] bCHALLOK = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(CHALLOK).array();
        protected final static byte[] bPUB = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(PUB).array();
        protected final static byte[] bSUB = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(SUB).array();
        protected final static byte[] bUNSUB = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(UNSUB).array();
        protected final static byte[] bSENDPUBLIC = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(SENDPUBLIC).array();
        protected final static byte[] bPUBPUBLIC = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(PUBPUBLIC).array();
        protected final static byte[] bSENDPT = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(SENDPT).array();
        protected final static byte[] bFORWARDPT = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(FORWARDPT).array();
        protected final static byte[] bDATAPT = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(DATAPT).array();
        protected final static byte[] bSUBOK = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(SUBOK).array();
        protected final static byte[] bERROR = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(ERROR).array();
    }

    private class MyFieldNamingStrategy implements FieldNamingStrategy {
        //Translates the Java field name into its JSON element name representation.
        @Override
        public String translateName(Field field) {
            String name = field.getName();
            char newFirstChar = Character.toLowerCase(name.charAt(1));
            return newFirstChar + name.substring(2);
        }
    }


        /* old stuff
        if (cmdFrame == CMD.REGOK() && cliState == CliState.UNREG) {
            log.format("Registered with broker!\n");
            cliState = CliState.REGISTERED;
            registrationThread.interrupt();
            if(registrationThread.isInterrupted())
                log.format("okay\n");
            else log.format("not okay at all\n");
            if(heartBeatThread.isAlive())
                heartBeatThread.run();
            else
                    heartBeatThread.start();
        }else if (cmdFrame == CMD.PONG()) {
        } */

    private class Subscription {
        private String topic, scope;
        private boolean active;
    }

    private class HeartBeat extends Thread {
        byte[] bcookie;

        public HeartBeat(byte[] bcookie) {
            this.bcookie = bcookie;
        }

        public void setBcookie(byte[] bcookie) {
            this.bcookie = bcookie;
        }

        public void run() {
            timeout = 0;
            listThreads();
            Thread.currentThread().setName("heartbeat-thread");

            while (!Thread.currentThread().isInterrupted()) {
                timeout += 1;
                if (timeout <= 3) {
                    ZMsg tosend = new ZMsg();
                    tosend.addFirst(CMD.bprotoVersion);
                    tosend.add(CMD.bPING);
                    tosend.add(this.bcookie);
                    tosend.send(socket);
                    try {
                        this.sleep(1500);
                    } catch (InterruptedException e) {
                        log.format(e.toString());
                        this.interrupt();
                        return;
                    }
                } else {
                    log.format("Broker did not respond, trying to reconnect\n");
                    cliState = CliState.UNREG;
                    callback.disconnected(broker);
                    sublistInactivateAll();
                    registrationThread = new Registration(hash);
                    registrationThread.start();
                    Thread.currentThread().interrupt();
                }
            }
        }
    }

    private class Registration extends Thread {
        String hash;

        public Registration(String hash) {
            this.hash = hash;
        }

        public void run() {
            Thread.currentThread().setName("registration-thread");
            listThreads();
            log.format("Trying to connect to broker at "+broker+"...\n");
            while (!Thread.currentThread().isInterrupted()) {
                socket.close();
                socket = ctx.createSocket(ZMQ.DEALER);
                socket.connect(broker);
                ZMsg tosend = new ZMsg();
                tosend.addFirst(CMD.bprotoVersion);
                tosend.add(CMD.bADDLCL);
                tosend.add(this.hash);
                tosend.send(socket);
                try {
                    this.sleep(3000);
                } catch (InterruptedException e) {
                    this.interrupt();
                    return;
                }
            }
        }
    }

}
