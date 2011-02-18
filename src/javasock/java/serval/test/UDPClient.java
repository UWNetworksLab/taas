/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
package serval.test;

import java.io.IOException;
import java.lang.System;
import serval.net.*;

public class UDPClient {
    private ServalDatagramSocket sock;
    
    public UDPClient() {

    }
    private void sendMessage(String msg) {
		if (msg.length() == 0) {
			return;
		}

		if (sock != null) {
			byte[] data = msg.getBytes();
			
			try {
				ServalDatagramPacket pack = 
                    new ServalDatagramPacket(data, data.length);
				sock.send(pack);
				// FIXME: Should not do a blocking receive in this function
				sock.receive(pack);

				String rsp = new String(pack.getData(), 0, pack.getLength());
				//System.out.println("response length=" + pack.getLength());
				System.out.println("Response: " + rsp);
			} catch (IOException e) {
				// TODO Auto-generated catch block
                System.out.println("Error: " + e.getMessage());
				if (sock != null) {
					sock.close();
					sock = null;
				}
				//msg += " - failed!";
			}
		}
    }
    private void run() {
        try {
            sock = new ServalDatagramSocket(new ServiceID((short) 32769));
            sock.connect(new ServiceID((short) 16385));
        } catch (Exception e) {
            System.out.println("failure: " + e.getMessage());
            
            if (sock != null)
                sock.close();
            
            return;
        }

        String msg = "Hello World!";

        System.out.println("Sending: " + msg);
 
        sendMessage(msg);

        if (sock != null)
            sock.close();
    }
    public static void main(String args[]) {
        System.out.println("UDPClient starting");
        UDPClient c = new UDPClient();

        c.run();
    }
}
