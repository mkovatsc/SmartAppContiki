import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.SocketException;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

import org.sics.coap.COAPException;
import org.sics.coap.COAPPacket;
import org.sics.coap.COAPPacket.Block;
import org.sics.coap.COAPPacket.Code;
import org.sics.coap.COAPPacket.ContentType;

public class COAPAppstoreServer implements Runnable {
  
  //private static Logger logger = Logger.getLogger(Main.class.getName());
  ////default values
  //public static final int COAP_TIMEOUT = 500;
  //public static final int MAX_RETRANSMIT = 5;
  //public static final int COAP_SERVER_PORT = 61616;
  
  private DatagramSocket datagramSocket = null;
  private BlockingQueue<DatagramPacket> packetQueue = new LinkedBlockingQueue<DatagramPacket>();
  private String coapResource;
  private boolean repeat;
  
  public static byte[] getBytesFromFile(File file) throws IOException {
    InputStream is = new FileInputStream(file);

    // Get the size of the file
    long length = file.length();

    if (length > Integer.MAX_VALUE) {
        // File is too large
    }

    // Create the byte array to hold the data
    byte[] bytes = new byte[(int)length];

    // Read in the bytes
    int offset = 0;
    int numRead = 0;
    while (offset < bytes.length
           && (numRead=is.read(bytes, offset, bytes.length-offset)) >= 0) {
        offset += numRead;
    }

    // Ensure all the bytes have been read in
    if (offset < bytes.length) {
        throw new IOException("Could not completely read file "+file.getName());
    }

    // Close the input stream and return bytes
    is.close();
    return bytes;
}


  
  public COAPAppstoreServer(String coapResource, int port, boolean repeat) throws SocketException {
	  this.coapResource = coapResource;
	  this.repeat = repeat;
    datagramSocket = new DatagramSocket(port);
  }
  
  public void run() {
    Thread thCoapServer = new Thread(new COAPDatagramServer(), "coap-server receiver");
    thCoapServer.start();
    System.out.println("COAP_SERVER: started, port:" + this.datagramSocket.getLocalPort() + ", repeat: " + this.repeat);
    System.out.println("COAP_SERVER: Resource: " + this.coapResource);
    
    byte[] elf_binary = null;
    String elf_name = null; 
    
    while (true) {
    	try {
    		DatagramPacket p = packetQueue.take();
    		COAPPacket coapPacket = new COAPPacket(p.getData(), p.getLength());
    		System.out.println("COAP_SERVER: received " + coapPacket);

    		if (coapPacket.getType() == COAPPacket.MSG_TYPE_CONFIRMABLE || coapPacket.getType() == COAPPacket.MSG_TYPE_NON_CONFIRMABLE) {
    			COAPPacket request = coapPacket;
    			String uri = request.getHeaderUri();

    			COAPPacket response = null;
    			if (this.coapResource.equals(uri)){
    				if (this.coapResource.equals("notify")) {
    					response = request.createResponse();
    					response.setPayload("Well done!");
    					response.setCode(COAPPacket.Code._200_OK);
    					response.setHeaderContentType(ContentType.TEXT_PLAIN);
    				} else if (this.coapResource.equals("market")) {
    					String currElfName = new String(request.getPayload());
    					/*if(!currElfName.equals(elf_name))*/ {
    						elf_name = currElfName; 
    						File elf_file = new File(elf_name);
    						elf_binary = getBytesFromFile(elf_file);
    					}

    					response = request.createResponse();
    					Block incBlock = request.getHeaderBlock();
    					byte [] outPayload = null;
    					int blocknum = incBlock == null ? 0 : incBlock.number;
    					Block outBlock = null;

    					if (elf_binary != null && blocknum*64 < elf_binary.length) {

    						int payloadSize;
    						boolean islast = false;
    						if((blocknum+1)*64 < elf_binary.length) {
    							payloadSize = 64;
    							System.out.println("More" + payloadSize);
    						} else {
    							islast = true;
    							payloadSize = elf_binary.length % 64;
    							System.out.println("Last" + payloadSize);
    						}

    						outPayload = new byte[payloadSize];
    						for (int i=0;i<payloadSize ;i++) {
    							outPayload[i] = elf_binary[blocknum*64+i];
    							//                			outPayload[i] = (byte) blocknum;
    						}

    						if (islast) {
    							outBlock = response.new Block(blocknum, false, 64, incBlock.version);
    						} else {
    							outBlock = response.new Block(blocknum, true, 64, incBlock.version);
    						}
    						System.out.println("OUTBLOCK "+outBlock.size);

    						response.setCode(COAPPacket.Code._200_OK);
    						response.setHeaderContentType(ContentType.APPLICATION_OCTET_STREAM);
    						response.setHeaderBlock(outBlock);
    	    				System.out.println("COAP_SERVER: packing     " + response);
    						response.setPayload(outPayload);
    					} else {
    						outBlock = response.new Block(blocknum, false, 64, 6);
    						response = request.createResponse(Code._404_NOT_FOUND);
    						response.setHeaderBlock(outBlock);
    					}
    				} else {
    					response = request.createResponse(Code._404_NOT_FOUND);
    				}
    			} else {
    				response = request.createResponse(Code._404_NOT_FOUND);
    			}

    			if (coapPacket.getType() == COAPPacket.MSG_TYPE_CONFIRMABLE) {
    				if (response == null) {
    					response = request.createResponse(Code._500_INTERNAL_SERVER_ERROR);
    				}
    				System.out.println("SENDING");
    				System.out.println("COAP_SERVER: sent     " + response);
    				send(response, p);
    			}
    		}
    		if(!repeat) {
    			System.out.println("COAP_SERVER: exiting");
    			System.exit(0);
    		}
    	} catch (InterruptedException ex) {
    		System.out.println("InterruptedException: " + ex);
    	} catch (COAPException ex) {
    		System.out.println("COAPException: " + ex);
    	} catch (IOException ex) {
    		System.out.println("IOException: " + ex);
    	}
    }
  }
  
  private void send(COAPPacket coapPacket, DatagramPacket p) throws COAPException, IOException {
    byte[] data = coapPacket.toByteArray();
    p.setData(data);
    datagramSocket.send(p);
}
  
  private COAPPacket testResource(COAPPacket request) throws COAPException {
    COAPPacket response = request.createResponse();

    switch (request.getMethod()) {
        case GET:
            response.setPayload("TEST");
            response.setCode(COAPPacket.Code._200_OK);
            response.setHeaderContentType(ContentType.TEXT_PLAIN);
            break;
        default:
            response.setCode(Code._405_METHOD_NOT_ALLOWED);
        break;
    }

    return response;
  }

  
    
    private class COAPDatagramServer implements Runnable {
      public void run() {
        while (true) {
            try {
                DatagramPacket packet = new DatagramPacket(new byte[512], 512);
                datagramSocket.receive(packet);
                packetQueue.offer(packet);
            } catch (IOException ex) {
                System.out.println("IOEx: " + ex);
            }
        }
      }
    }
    
    private static void runNodeEmulator(String coapResource, int coapServerPort, boolean repeat) {
      COAPAppstoreServer nodeEmulator = null;
      try {
          nodeEmulator = new COAPAppstoreServer(coapResource, coapServerPort, repeat);
      } catch (SocketException ex){
        System.out.println("COAP_SERVER ERROR: Can not make UDP connection " + ex.getMessage());
          return;
      }
      //start coap emulator
      Thread emulator = new Thread(nodeEmulator, "coap-server");
      emulator.start();
  }
    
    public static void main(String[] args) {
      //default configuration
    	if(args.length != 3) {
    		System.out.println("Usage: COAPServer <resource> <port> <repeat: 0/1>");
    		System.exit(1);
    	}
      String coapResource = args[0];
      int coapServerPort = Integer.parseInt(args[1]);
      boolean repeat = Integer.parseInt(args[2]) > 0;

//      int coapTimeout = COAP_TIMEOUT;
//      int maxRetransmit = MAX_RETRANSMIT;

      runNodeEmulator(coapResource, coapServerPort, repeat);
  }
    
}