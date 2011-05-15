package org.sics.coap;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.Map;

public class COAPPacket {

  // public static final Charset DEFAULT_CHARSET = Charset.forName("US-ASCII");
  // public static final byte MSG_TYPE_REQUEST = 0x0;
  // public static final byte MSG_TYPE_RESPONSE = 0x1;
  // public static final byte MSG_TYPE_NOTIFY = 0x2;

  public static final byte MSG_TYPE_CONFIRMABLE = 0x0;
  public static final byte MSG_TYPE_NON_CONFIRMABLE = 0x1;
  public static final byte MSG_TYPE_ACKNOWLEDGMENT = 0x2;
  public static final byte MSG_TYPE_RESET = 0x3;

  public static final int COAP_HEADER_VERSION_MASK = 0xC0;
  public static final int COAP_HEADER_TYPE_MASK = 0x30;
  public static final int COAP_HEADER_OPTION_COUNT_MASK = 0x0F;
  public static final int COAP_HEADER_OPTION_DELTA_MASK = 0xF0;
  public static final int COAP_HEADER_OPTION_SHORT_LENGTH_MASK = 0x0F;

  public static final int COAP_HEADER_VERSION_POSITION = 6;
  public static final int COAP_HEADER_TYPE_POSITION = 4;
  public static final int COAP_HEADER_OPTION_DELTA_POSITION = 4;

  public static final byte OPTION_CONTENT_TYPE = 1;
  public static final byte OPTION_MAX_AGE = 2;
  public static final byte OPTION_URI_SCHEME = 3;
  public static final byte OPTION_ETAG = 4;
  public static final byte OPTION_URI_AUTHORITY = 5;
  public static final byte OPTION_TYPE_LOCATION = 6;
  public static final byte OPTION_URI_PATH = 9;
  public static final byte OPTION_SUBSCRIPTION_LIFETIME = 10;
  public static final byte OPTION_TOKEN = 11;
  public static final byte OPTION_BLOCK = 13;
  public static final byte OPTION_URI_QUERY = 15;
  public static final byte OPTION_BLOCK2 = 17;
  
  

  private byte version = 1;
  private byte type = MSG_TYPE_CONFIRMABLE;
  // private byte mustAcknowladge = 1;
  private int transationId = -1;
  private Code code = Code._200_OK;
  private Method method = Method.GET;
  private Map<Byte, byte[]> headerOptions = new HashMap<Byte, byte[]>();
  private byte[] payload = new byte[0];

  public enum Code {

    _200_OK(80), _201_CREATED(81), _304_NOT_MODIFIED(124), _400_BAD_REQUEST(160), _404_NOT_FOUND(
        164), _405_METHOD_NOT_ALLOWED(165), _409_CONFLICT(29), _415_UNSUPPORTED_MADIA_TYPE(
        175), _500_INTERNAL_SERVER_ERROR(200), _502_BAD_GATEWAY(202), _504_GATEWAY_TIMEOUT(
        204);

    private int coapCode;

    private Code(int coapCode) {
      this.coapCode = coapCode;
    }

    public int getHttpCode() {
      return Integer.parseInt(name().substring(1, 4));
    }

    public int getCoapCode() {
      return coapCode;
    }

    public static Code valueOf(int code) {
      for (Code c : values()) {
        if (c.getCoapCode() == code) {
          return c;
        }
      }
      return null;
    }
  }

  public enum Method {

    GET(1), POST(2), PUT(3), DELETE(4);

    private int method;

    private Method(int method) {
      this.method = method;
    }

    public static Method valueOf(int methodCode) throws COAPException {
      if (methodCode < 1 || methodCode > 4) {
        throw new COAPException("Wrong method code");
      }
      return Method.values()[methodCode - 1];
    }

    public int getCode() {
      return method;
    }
  }

  public enum ContentType {

    TEXT_PLAIN(0), TEXT_XML(1), TEXT_CSV(2), TEXT_HTML(3), IMAGE_GIF(21), IMAGE_JPEG(
        22), IMAGE_PNG(23), IMAGE_TIFF(24), AUDIO_RAW(25), VIDEO_RAW(26), APPLICATION_LINK_FORMAT(
        40), APPLICATION_XML(41), APPLICATION_OCTET_STREAM(42), APPLICATION_RDF_XML(
        43), APPLICATION_SOAP_XML(44), APPLICATION_ATOM_XML(45), APPLICATION_XMPP_XML(
        46), APPLICATION_EXI(47), APPLICATION_X_BXML(48), APPLICATION_FASTINFOSET(
        49), APPLICATION_SOAP_FASTINFOSET(50), APPLICATION_JSON(51);

    private int val;

    private ContentType(int val) {
      this.val = val;
    }

    // private ContentType(int topId, int subId) {
    // if (subId <= 0x1F) {
    // this.val = (topId << 5 | (subId & 0x1F));
    // } else {
    // this.val = (topId << 13 | (subId & 0x1FFF));
    // }
    // }

    public int getValue() {
      return val;
    }

    @Override
    public String toString() {
      return super.toString().replaceFirst("_", "/").replaceFirst("__", "-")
          .replaceFirst("_", "+").toLowerCase();
    }

    public static ContentType valueOf(int identifier) {
      for (ContentType ct : values()) {
        if (ct.getValue() == identifier) {
          return ct;
        }
      }
      return null;
    }
  }

  public COAPPacket() {
  }

  // public COAPPacket(byte[] rawData) throws COAPException {
  // ByteArrayInputStream inputStream = new ByteArrayInputStream(rawData);
  // readFrom(inputStream);
  // }

  public COAPPacket(byte[] rawData, int length) throws COAPException {
    ByteArrayInputStream inputStream = new ByteArrayInputStream(rawData, 0,
        length);
    readFrom(inputStream);
  }

  // /**
  // * Deserialize CoAP message (draft-shelby-core-coap-01)
  // */
  // public static COAPPacket deserialize(InputStream inputStream) throws
  // COAPException {
  // COAPPacket coapPacket = new COAPPacket();
  // coapPacket.readFrom(inputStream);
  // return coapPacket;
  // }

  public static byte[] serialize(COAPPacket coapPacket) throws COAPException {
    ByteArrayOutputStream outputStream = new ByteArrayOutputStream();

    coapPacket.writeTo(outputStream);
    return outputStream.toByteArray();
  }

  protected void readFrom(InputStream inputStream) throws COAPException {
    try {
      byte options = 0;
      int tempByte = inputStream.read(); // first byte

      version = (byte) ((tempByte & 0xC0) >> 6);
      if (version != 1) {
        throw new COAPException("CoAP version %s not supported", version);
      }

      type = (byte) ((tempByte & 0x30) >> 4);
      if (type < 0 || type > 3) {
        throw new COAPException("Wrong message type (%s)", type);
      }

      options = (byte) ((tempByte & 0x0F));

      tempByte = inputStream.read(); // second byte
      
      if ( this.getType() == MSG_TYPE_ACKNOWLEDGMENT ) {
        code = Code.valueOf(tempByte);
      } else {
        method = Method.valueOf(tempByte);
      }

      transationId = inputStream.read() << 8;
      transationId = transationId | inputStream.read();

      System.out.println("# of options :" + options);

      // read options
      byte hdrType = 0;
      for (int i = 0; i < options; i++) {
          tempByte = inputStream.read();
          byte delta = (byte) ((tempByte >> 0x4) & 0xF);
          hdrType += delta;
          System.out.println("Option "+hdrType + " (delta "+delta+")");
          int hdrLen = tempByte & 0x0F;
          //System.out.println(" hdrLen1: " + hdrLen);
          if (hdrLen == 0xF) {
            hdrLen = 0xF + inputStream.read();
          }

          byte[] hdrData = new byte[hdrLen];
          inputStream.read(hdrData);

          this.headerOptions.put(hdrType, hdrData);
        }

      // read payload
      int plLen = inputStream.available();
      this.payload = new byte[plLen];
      inputStream.read(payload);

    } catch (IOException iOException) {
      throw new COAPException();
    }
  }

  public static byte[] copyBytes(InputStream inputStream) throws IOException {
    ByteArrayOutputStream baos = new ByteArrayOutputStream();
    int dd = inputStream.read();
    while (dd >= 0) {
      baos.write(dd);
      dd = inputStream.read();
    }
    return baos.toByteArray();
  }

  public byte[] getHeaderOption(byte optionType) {
    return this.headerOptions.get(optionType);
  }

  public String getHeaderUri() {
    byte[] optData = getHeaderOption(COAPPacket.OPTION_URI_PATH);
    if (optData == null) {
      return null;
    }
    return new String(optData);
  }

  public void setHeaderUri(String uri) {
    // putHeaderOption(OPTION_URI_PATH, uri.getBytes(DEFAULT_CHARSET));
    putHeaderOption(OPTION_URI_PATH, uri.getBytes());
  }

  public ContentType getHeaderContentType() {
    Integer i = readVariableUInt(getHeaderOption(OPTION_CONTENT_TYPE));
    if (i == null) {
      return null;
    }
    return ContentType.valueOf(i);
  }

  public void setHeaderContentType(ContentType contentType)
      throws COAPException {
    if (contentType == null) {
      throw new COAPException("Wrong content type value");
    }
    headerOptions.put(OPTION_CONTENT_TYPE, writeVariableUInt(contentType
        .getValue()));
  }

  public Long getHeaderMaxAge() {
    return readVariableULong(getHeaderOption(COAPPacket.OPTION_MAX_AGE));
  }

  public void setHeaderMaxAge(long maxAge) throws COAPException {
    if (maxAge < 0) {
      throw new COAPException("Wrong max age value");
    }
    headerOptions.put(OPTION_MAX_AGE, writeVariableUInt(maxAge));
  }
  
  public Long getHeaderSubscriptionLifetime() {
    return readVariableULong(getHeaderOption(COAPPacket.OPTION_SUBSCRIPTION_LIFETIME));
  }

  public void setHeaderSubscriptionLifetime(long subsLife) throws COAPException {
    if (subsLife < 0) {
      throw new COAPException("Wrong subscription lifetime value");
    }
    headerOptions.put(OPTION_SUBSCRIPTION_LIFETIME, writeVariableUInt(subsLife));
  }

  // public void setHeaderDate(long date) {
  // headerOptions.put(OPTION_DATE, writeVariableUInt(date, 4));
  // }
  //
  // public Long getHeaderDate() {
  // return readVariableULong(getHeaderOption(COAPPacket.OPTION_DATE));
  // }
  //
  // public Integer getHeaderUriCode() {
  // return readVariableUInt(getHeaderOption(COAPPacket.OPTION_URI_CODE));
  // }
  //
  // public void setHeaderUriCode(int uriCode) {
  // headerOptions.put(OPTION_URI_CODE, writeVariableUInt(uriCode) );
  // }

  public Integer getHeaderETag() {
    return readVariableUInt(getHeaderOption(COAPPacket.OPTION_ETAG));
  }

  public void setHeaderETag(int etag) {
    headerOptions.put(COAPPacket.OPTION_ETAG, writeVariableUInt(etag));
  }
  
  public class Block {
    public int option;
    public int number;
    public boolean more;
    public int size;
    public int version;
    
    public Block(int option, int ver) {
      this.option = option;
      number = (option >> 4);
      byte temp = (byte) (option & 0xF);
      this.more = ((temp >> 3) != 0);
      this.size = 16 << (temp & 0x7);
      
      System.out.println("SZX "+(temp & 0x7)+" = "+this.size);
      
      this.version = ver;
      System.out.println("BlockO: " + option + " number: " + number + " more:" + more + " size:" + size);
    }
    
    public Block(int number, boolean more, int size, int ver) {
      this.option = number << 4;
      if (more) {
    	  this.option |= 0x8;
      }
      size = size >> 4;
      this.size = (int) Math.floor(Math.log(size) / Math.log(2));
      this.number = number;
      this.option |= this.size;
      
      this.version = ver;
      System.out.println("BlockI: " + this.option + " ("+this.number+"/"+more+"/"+this.size+")");
    }
  }
  
  public Block getHeaderBlock() {
    Integer value = readVariableUInt(getHeaderOption(COAPPacket.OPTION_BLOCK));
    if (value != null) {
    	System.out.println("VER 3: "+Integer.toHexString(value));
      return new Block(value, 3);
    } else {
	  value = readVariableUInt(getHeaderOption(COAPPacket.OPTION_BLOCK2));
	  if (value != null) {
		System.out.println("VER 6: "+Integer.toHexString(value));
        return new Block(value, 6);
      } else {
    	return new Block(0, 6);
      }
    }
  }

  public void setHeaderBlock(Block block) {
	  System.out.println("SETTING "+(16 << (block.option & 0x7))+" = "+block.size);
	  if (block.version==6) {
		  headerOptions.put(COAPPacket.OPTION_BLOCK2, writeVariableUInt(block.option));
	  } else {
		  headerOptions.put(COAPPacket.OPTION_BLOCK, writeVariableUInt(block.option));
	  }
  }

   public COAPPacket createResponse() {
     return createResponse(Code._200_OK);
   }
  
   public COAPPacket createResponse(Code responseCode) {
     if (this.getType() != COAPPacket.MSG_TYPE_CONFIRMABLE) {
       return null;
     }
     
     COAPPacket response = new COAPPacket();
     response.setTransactionId(this.transationId);
     response.setType(COAPPacket.MSG_TYPE_ACKNOWLEDGMENT);
     response.setCode(responseCode);
    
     return response;
   }

  public byte getVersion() {
    return version;
  }

  public byte getType() {
    return type;
  }

  public void setType(byte type) {
    this.type = type;
  }

  public Method getMethod() {
    return method;
  }

  public void setMethod(Method method) {
    this.method = method;
  }

  public void setTransactionId(int transationsID) {
    this.transationId = transationsID;
  }

  public void putHeaderOption(byte type, byte[] optionData) {
    this.headerOptions.put(type, optionData);
  }

  public byte[] getPayload() {
    return payload;
  }

  public DataInputStream getPayloadDataInput() {
    return new DataInputStream(new ByteArrayInputStream(payload));
  }

  public void setPayload(String payload) {
    // setPayload(payload.getBytes(DEFAULT_CHARSET));
    setPayload(payload.getBytes());
  }

  public void setPayload(byte[] payload) {
    this.payload = payload;
  }

  public void writeTo(OutputStream outputStream) throws COAPException {
    try {

      if (this.headerOptions.size() > 0xF) {
        throw new COAPException("To many option header");
      }

      byte options = (byte) this.headerOptions.size();
      int tempByte = 0;

      tempByte = (0x3 & version) << COAP_HEADER_VERSION_POSITION; // Version
      tempByte |= (0x3 & type) << COAP_HEADER_TYPE_POSITION; // Message Type
      tempByte |= options & 0xF; // Number of Options

      outputStream.write(tempByte);
      
      if (this.getType() == MSG_TYPE_ACKNOWLEDGMENT) {
        tempByte = code.getCoapCode(); 
      } else {
        tempByte = method.getCode();
      }
      
      outputStream.write(tempByte);

      outputStream.write(0xFF & (transationId >> 8));
      outputStream.write(0xFF & transationId);

      int delta = 0;
      for (Byte optType : headerOptions.keySet()) {
        byte[] optData = headerOptions.get(optType);
        int optLen = optData.length;
        
        while (optType - delta > 15) {
    	  tempByte = (14 - delta%14) << 4;
    	  outputStream.write(tempByte);
    	  delta += 14 - delta%14;
        }

        tempByte = (optType - delta) << 4;
        delta = optType;

        if (optLen < 0xF) {
          tempByte = tempByte | optLen;
          outputStream.write(tempByte);
        } else if (optLen <= 270) {
          tempByte = tempByte | 0xF;
          outputStream.write(tempByte);
          outputStream.write(optLen - 0xF);
        } else {
          throw new COAPException(
              "Option length larger that 270 is not supported");
        }
        outputStream.write(optData);
      }

      // payload
      if (payload != null) {
        outputStream.write(payload);
      }
    } catch (IOException iOException) {
      throw new COAPException(iOException.getMessage());
    }
  }

  /*
   * Creates a CoAP packet. Returns array of bytes.
   */
  public byte[] toByteArray() throws COAPException {
    ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
    writeTo(outputStream);
    return outputStream.toByteArray();
  }

  /**
   * @return the transationId
   */
  public int getTransationId() {
    return transationId;
  }

  /**
   * @return the code
   */
  public Code getCode() {
    return code;
  }

  /**
   * @param code
   *          the code to set
   */
  public void setCode(Code code) {
    this.code = code;
  }

  @Override
  public String toString() {
    StringBuilder sb = new StringBuilder();

    if (this.getType() == COAPPacket.MSG_TYPE_ACKNOWLEDGMENT) {
      sb.append("CoAP Response " + this.getCode());
    } else {
      sb.append("CoAP Request " + method);
    }

    if (this.getType() == COAPPacket.MSG_TYPE_CONFIRMABLE) {
      sb.append(" Confirmable");
    } else if (this.getType() == COAPPacket.MSG_TYPE_NON_CONFIRMABLE) {
      sb.append(" NONConfirmable");
    } else if (this.getType() == COAPPacket.MSG_TYPE_ACKNOWLEDGMENT) {
      sb.append(" ACK");
    } else if (this.getType() == COAPPacket.MSG_TYPE_RESET) {
      sb.append(" RESET");
    } 
    
    sb.append(" TID:" + this.transationId);

    if (this.getHeaderUri() != null) {
      sb.append(" URI:" + this.getHeaderUri());
    }
    
    if (this.getHeaderETag() != null) {
      sb.append(" ETag:" + this.getHeaderETag());
    }
    if (this.getHeaderMaxAge() != null) {
      sb.append(" MaxAge:" + this.getHeaderMaxAge() / 1000 + "s");
    }
    if (this.getHeaderContentType() != null) {
      sb.append(" ContType:" + this.getHeaderContentType());
    }
    
    if (this.getHeaderSubscriptionLifetime() != null) {
      sb.append(" Subs Lifetime:" + this.getHeaderSubscriptionLifetime());
    }
    
    Block block = this.getHeaderBlock();
    if (block != null){
      sb.append(" Block #:" + block.number + " M:" + block.more + " size:" + block.size);
    }

    if (payload != null) {
      byte[] plBytes = payload;
      sb.append(" plen:'" + plBytes.length);
      if (plBytes.length > 0) {
        //sb.append(" PL:" + toHex(plBytes, plBytes.length));
        sb.append(" pl:'" + new String(plBytes) + "'");
      }
    }

    return sb.toString();
  }

  // --- Utility functions ---
  private static Integer readVariableUInt(byte[] data) {
    Long l = readVariableULong(data);
    if (l != null) {
      return l.intValue();
    } else {
      return null;
    }
  }

  private static Long readVariableULong(byte[] data) {
    if (data == null) {
      return null;
    }
    long val = 0;
    for (byte b : data) {
      val <<= 8;
      val += (b & 0xFF);
    }
    return val;
  }

  private static byte[] writeVariableUInt(long value) {
    return writeVariableUInt(value, 1);
  }

  private static byte[] writeVariableUInt(long value, int minBytes) {
    int len = (int) Math.ceil((Math.log10(value) / Math.log10(2)) / 8); // calculates
                                                                        // needed
                                                                        // minimum
                                                                        // lenght
    len = Math.max(len, minBytes);
    byte[] data = new byte[len];
    for (int i = 0; i < len; i++) {
      data[i] = (byte) (0xFF & (value >> 8 * (len - (i + 1))));
    }
    return data;
  }

  public static String toHex(byte[] raw, int length) {
    final String HEXES = "0123456789ABCDEF";
    if (raw == null) {
      return null;
    }
    final StringBuilder hex = new StringBuilder(2 * length);
    for (int i = 0; i < length; i++) {
      byte b = raw[i];
      hex.append(HEXES.charAt((b & 0xF0) >> 4))
          .append(HEXES.charAt((b & 0x0F)));
      hex.append(" ");
    }
    return hex.toString();
  }
}