package test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.sql.Date;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Random;

import org.junit.Test;

import coap.GETRequest;
import coap.POSTRequest;
import coap.Request;
import coap.Response;


public class HoneywellTest {
	
	private Random random = new Random(Calendar.getInstance().getTimeInMillis());
	
	private final String node = "coap://[bbbb::11:11ff:fe11:1111]:5683";

	public Response generalRequest(Request request, String uri, String payload){
		try {
			request.setURI(new URI(uri));
		} catch (URISyntaxException e) {
			e.printStackTrace();
		}
		request.setPayload(payload);
		request.enableResponseQueue(true);

		try {
			request.execute();
		} catch (IOException e) {
			System.err.println("Failed to execute request: " + e.getMessage());
		}

		// receive response
		Response response = null;
		try {
			response = request.receiveResponse();

			// check for indirect response
			if (response != null && response.isEmptyACK()) {
				response.log();
				System.out.println("Request acknowledged, waiting for separate response...");

				response = request.receiveResponse();
			}
		} catch (InterruptedException e) {
			System.err.println("Failed to receive response: " + e.getMessage());
		}
		
		// wait for request to settle down
		try {
			Thread.sleep(1000);
		} catch (InterruptedException e) {}

		return response;
	}

	public Response getRequest(String uri){
		Request request = new GETRequest();
		return generalRequest(request, uri, null);
	}


	public Response postRequest(String uri, String payload){
		Request request = new POSTRequest();
		return generalRequest(request, uri, payload);
	}

	public String getRandomTime() {
		String hour = String.format("%02d", random.nextInt(24));
		String minute = String.format("%02d", random.nextInt(60));
		return hour+":"+minute;
	}
	
	public String getRandomMode(){
		int randint = random.nextInt(4);
		switch(randint){
		case 0:
			return "comfort";
		case 1:
			return "supercomfort";
		case 2:
			return "energy";
		case 3:
			return "frost";
		}
		return null;
	}
	
	@Test
	public void wrongTimeTest(){
		String uri = node+"/time";
		System.out.println("Setting time with wrong parameters");
		
		// not existing hour
		Response response = postRequest(uri,"25:12:12");
		assertNotNull(response);
		assertEquals(128, response.getCode());
		
		// not existing second
		response = postRequest(uri,"12:12:61");
		assertNotNull(response);
		assertEquals(128, response.getCode());
		
		// not existing minute
		response = postRequest(uri,"12:61:12");
		assertNotNull(response);
		assertEquals(128, response.getCode());
		
		// too long parameter
		response = postRequest(uri,"12:12:124");
		assertNotNull(response);
		assertEquals(128, response.getCode());
		
		// no second parameter but delimiter given
		response = postRequest(uri,"12:12:");
		assertNotNull(response);
		assertEquals(128, response.getCode());
		
		// wrong delimiters
		response = postRequest(uri,"12:12.12");
		assertNotNull(response);
		assertEquals(128, response.getCode());
		
		response = postRequest(uri,"12.12");
		assertNotNull(response);
		assertEquals(128, response.getCode());
		
		response = postRequest(uri,"12.12:12");
		assertNotNull(response);
		assertEquals(128, response.getCode());
	}
	
	@Test
	public void setTimeTest(){
		String randomTime = getRandomTime();
		String uri = node+"/time";
		System.out.println("Setting time to "+randomTime);
		Response response = postRequest(uri,randomTime);
		System.out.println("\t"+response.getPayloadString());
		assertNotNull(response);
		assertEquals(69, response.getCode());
		
		response = getRequest(uri);
		assertNotNull(response);
		assertEquals(69, response.getCode());
		String responseString = response.getPayloadString();
		assertTrue(responseString.startsWith(randomTime.substring(0, randomTime.length()-2)));
	}
	
	@Test
	public void wrongDateTest(){
		String uri = node+"/date";
		System.out.println("Setting date with wrong parameters");
		
		// leap year
		Response response = postRequest(uri,"29.02.12");
		assertNotNull(response);
		assertEquals(69, response.getCode());
		
		// no leap year
		response = postRequest(uri,"29.02.11");
		assertNotNull(response);
		assertEquals(128, response.getCode());
		
		// not existing day
		response = postRequest(uri,"31.04.11");
		assertNotNull(response);
		assertEquals(128, response.getCode());
		
		// not existing month
		response = postRequest(uri,"12.13.11");
		assertNotNull(response);
		assertEquals(128, response.getCode());

		//wrong delimiters
		response = postRequest(uri,"12:12.61");
		assertNotNull(response);
		assertEquals(128, response.getCode());
		
		response = postRequest(uri,"12;12:61");
		assertNotNull(response);
		assertEquals(128, response.getCode());
	}
	
	@Test
	public void setDateTest(){
		SimpleDateFormat sdf = new SimpleDateFormat("dd.MM.yy");
		String randomDate = sdf.format(new Date(random.nextLong()));
		System.out.println("Setting date to "+randomDate);
		setAndCheckValue("/date", randomDate, randomDate);
	}

	@Test
	public void emptyPostSetTimerTest(){
		Response response = postRequest(node+"/auto/weektimer?1","");
		assertNotNull(response);
		assertEquals(128, response.getCode());
	}
	
	@Test
	public void getTemperature(){
		Response response = getRequest(node+"/temperature");
		assertNotNull(response);
		assertEquals(69, response.getCode());
	}
	
	@Test
	public void getBattery(){
		Response response = getRequest(node+"/battery");
		assertNotNull(response);
		assertEquals(69, response.getCode());
		int bat=Integer.parseInt(response.getPayloadString());
		assertTrue(bat>1000 && bat<4000);
	}
	
	@Test
	public void setMode(){
		System.out.println("Testing mode settings");
		setAndCheckValue("/mode", "auto", "auto");
		setAndCheckValue("/mode", "manual", "manual");
		setAndCheckValue("/mode", "valve", "valve");
	}
	
	@Test
	public void setValve(){
		int r = random.nextInt(51) + 30;
		String rand = ""+r;
		
		String uri = node+"/valve";
		Response response = postRequest(uri, rand);
		assertNotNull(response);
		assertEquals(69, response.getCode());
		
		response = getRequest(uri);
		
		response = getRequest(uri);
		assertNotNull(response);
		assertEquals(69, response.getCode());
		String responseString = response.getPayloadString();
		assertEquals(rand, responseString);
	}
	
	
	public void setAndCheckValue(String resource, String payload, String expected){
		String uri = node+resource;
		Response response = postRequest(uri,payload);
		System.out.println("\t"+response.getPayloadString());
		assertNotNull(response);
		assertEquals(69, response.getCode());
		
		response = getRequest(uri);
		assertNotNull(response);
		assertEquals(69, response.getCode());
		String responseString = response.getPayloadString();
		assertEquals(expected, responseString);
	}
	
	public void setTemperature(String resource){
		int new_temp=random.nextInt(51)*5+50;
		String payload = ""+new_temp;
		
		String uri = node+resource;
		System.out.println("Setting temp to "+new_temp);
		Response response = postRequest(uri,payload);
		System.out.println("\t"+response.getPayloadString());
		assertNotNull(response);
		assertEquals(69, response.getCode());
		
		response = getRequest(uri);
		
		response = getRequest(uri);
		assertNotNull(response);
		assertEquals(69, response.getCode());
		String responseString = response.getPayloadString();
		String expected=(new_temp%10==0)?"00":"50";
		assertEquals(new_temp/10+"."+expected, responseString);
	}
	
	@Test
	public void setComfortTemperature(){
		setTemperature("/auto/comfort");
	}
	
	@Test
	public void setSuperComfortTemperature(){
		setTemperature("/auto/supercomfort");
	}
	
	@Test
	public void setEnergyTemperature(){
		setTemperature("/auto/energy");
	}
	
	@Test
	public void setFrostTemperature(){
		setTemperature("/auto/frost");
	}

	@Test
	public void setTimerModeTest(){
		System.out.println("Testing timermodes");
		setAndCheckValue("/auto/timermode", "justOne", "justOne");
		setAndCheckValue("/auto/timermode", "weekdays", "weekdays");
	}
	
	@Test
	public void setTimerTest(){
		System.out.println("Setting some random timeslots to random values");
		int numberOfTests = 10;
		for(int i=1; i<=numberOfTests; i++){
			int day = random.nextInt(8);
			int slot = random.nextInt(8)+1;
			String dayString = (day==0)?"weektimer":("day"+day+"timer");
			String now = getRandomTime();
			String mode = getRandomMode();
			assertNotNull(mode);			
			System.out.println("\tTest "+i+"/"+numberOfTests+": Setting "+dayString+"?"+slot+" to "+"time="+now+"&mode="+mode);
			
			String payload = "time="+now+"&mode="+mode;
			String uri = node+"/auto/"+dayString+"?"+slot;
			Response response = postRequest(uri,payload);
			assertNotNull(response);
			assertEquals(69, response.getCode());

//			response = getRequest(uri);
//			assertNotNull(response);
//			assertEquals(69, response.getCode());
//			String responseString = response.getPayloadString();
//			String[] array = responseString.split(" at ");

			response = getRequest(uri);
			String responseString = response.getPayloadString();
			String[] array = responseString.split(" at ");
			assertEquals(mode, array[0]);
			assertEquals(now, array[1]);
		}
	}
}
