import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketTimeoutException;

import org.sics.coap.COAPException;
import org.sics.coap.COAPPacket;
import org.sics.coap.COAPPacket.Block;
import org.sics.coap.COAPPacket.Method;

public class COAPClient {

  public static final int PORT = 61616;
  public static final int TIMEOUT = 30000;
  
  private int port;

  public COAPClient(int port) {
    this.port = port;
  }
  
  public COAPClient() {
    this.port = PORT;
  }

  public void sendPacket(String payload) {
    try {
    	send("127.1", port, payload.getBytes(),TIMEOUT);
    } catch (Exception e) {
      e.printStackTrace();
    }
  }
  
  public static void test(int draft, String addr, int port, String method, String url, String payload) throws IOException {
	  test(draft, addr, port, method, url, payload,TIMEOUT);
  }
  
  static int tid=1;
  public static  boolean test(int draft, String addr, int port, String method, String url, String payload,int timeout) throws IOException {
  	COAPPacket request = new COAPPacket(draft);
    request.setTransactionId(tid++);
    request.setHeaderUri(url);
    request.setPayload(payload);
    
    if (method.equalsIgnoreCase("post")) {
    	request.setMethod(Method.POST);
    } else if (method.equalsIgnoreCase("put")) {
    	request.setMethod(Method.PUT);
    } else if (method.equalsIgnoreCase("delete")) {
    	request.setMethod(Method.DELETE);
    }
    
    if (url.equals("abc")) {
      try {
        request.setHeaderSubscriptionLifetime(300);
      } catch (COAPException e) {
        // TODO Auto-generated catch block
        e.printStackTrace();
      }
    }
    
    if (url.equals("market")) {
      Block block = request.new Block(2, true, 64);
      request.setHeaderBlock(block);
    }
    
  	try {
  		DatagramPacket reply = send(addr, port, COAPPacket.serialize(request),timeout);
  		if (reply != null){
  			COAPPacket coapPacket = new COAPPacket(draft, reply.getData(), reply.getLength());
  			System.out.println(coapPacket);
  			System.out.println("Payload: " + new String(coapPacket.getPayload()));
  			return true;
  		}else
  			return false;
    } catch (COAPException e) {
	    // TODO Auto-generated catch block
	    e.printStackTrace();
	    return false;
    }
   
  }

  public static DatagramPacket send(String host, int port, byte[] data, int timeout)
    throws IOException {
    DatagramSocket socket = new DatagramSocket();
    try {
      DatagramPacket packet = new DatagramPacket(data, data.length,
          InetAddress.getByName(host), port);
      socket.setSoTimeout(timeout);
      socket.send(packet);
      try {
        packet = new DatagramPacket(new byte[256], 256);
        socket.receive(packet);
        
        int len = packet.getLength();
	      if (len > 0) {
	      	byte[] responseData = packet.getData();
	      	String response = new String(responseData, packet.getOffset(), len);
//		    System.out.println("Reply: " + response);
	      }
	      
        return packet;
      } catch (SocketTimeoutException e) {
        System.err.println("No reply within " + (timeout / 1000) + " seconds.");
        System.exit(1);
        return null;
      } 
    }catch (Exception e) {
		e.printStackTrace();
		return null;
	}finally {		
      socket.close();     
    }
    
  }
  
  public static void main(String [] args)
  {
  	try {
  		int draft = 6;
  		String address = "aaaa::0212:7402:0002:0202";
  		int port = PORT;
  		String method = "GET";
  		String url = "light";
  		String payload = "";
  		
  		if (args.length < 1) {
  			System.out.println("Usage: COAPClient <draft> [<address>] [<port>] [<method>] [<uri-path>] [<payload>]");
			System.exit(1);
		}
  		
  		draft = Integer.parseInt(args[0]);
  		
  		if (args.length > 1) {
  		  address = args[1];
  		}
  		if (args.length > 2) {
  			port = Integer.parseInt(args[2]);
  		}
  		
  		if (args.length > 3) {
  			method = args[3];
  		}
  		if (args.length > 4) {
  			url = args[4];
  		}
  		if (args.length > 5) {
  			payload = args[5];
  		}
  		System.out.println("CoAP-0" + draft + " request: " + method + " " + address + ":" + port + "/" + url + ", payload: " + payload);
		long starttime = System.currentTimeMillis();
		COAPClient.test(draft, address, port, method, url, payload);
  		long endtime = System.currentTimeMillis();
  		System.out.println("Duration (ms): " + (endtime-starttime));
    } catch (IOException e) {
	    // TODO Auto-generated catch block
	    e.printStackTrace();
    }
  }

}
