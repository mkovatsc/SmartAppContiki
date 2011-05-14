package org.sics.coap;

public class COAPException extends Exception {
  public COAPException() {
    super("Coap message exception");
  }
  
  public COAPException(String message) {
      super(message);
  }
  
  public COAPException(String format, Object... args) {
      super(String.format(format, args));
  }
}
