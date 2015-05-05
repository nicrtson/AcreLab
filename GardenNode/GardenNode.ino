//  Copyright (C) 2012 James Coliz, Jr. <maniacbug@ymail.com>
// Narcoleptic - A sleep library for Arduino
// Copyright (C) 2010 Peter Knight (Cathedrow)

/**
 * L.O.G. measuring temperature with a DS18B20
 * and measuring Vcc 
 * For each node, change DS18B20 DeviceAddress
 * and this_node address
 * uses Narcoleptic library to put Arduino to sleep
 */
#include <dht.h>
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Narcoleptic.h>




int sleepDelay = 30000;	// in milliseconds

// DS18B20 is plugged into Arduino D4 
// Analog pins
#define VccPin A1
#define BatVccPin A2
#define SolarVccPin A0

// Digital pins
#define ActivePin 5
#define ONE_WIRE_BUS 6
#define DHT22_PIN 7

#define TEMPERATURE_PRECISION 12
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
// This is DS18B20 #1 address
//DeviceAddress Therm1 = { 0x28, 0x33, 0x4B, 0xD0, 0x04, 0x00, 0x00, 0xAC};
// arrays to hold device addresses
DeviceAddress outsideThermometer;

int temp1F;            // 2 bytes -32,768 to 32,767
int humidityTempF;     // 2 bytes -32,768 to 32,767
int Vcc; 	       // 2 bytes -32,768 to 32,767
int BatVcc;            // 2 bytes -32,768 to 32,767
int SolarVcc;          // 2 bytes -32,768 to 32,767
int humidity;          // 2 bytes -32,768 to 32,767
int moist;             // 2 bytes -32,768 to 32,767
int ph;                // 2 bytes -32,768 to 32,767
int future1;           // 2 bytes -32,768 to 32,767
int future2;           // 2 bytes -32,768 to 32,767

// nRF24L01(+) radio attached  (CE, CSN)
RF24 radio(9,10);
// Network uses that radio
RF24Network network(radio);
// Channel of our node
const uint16_t channel = 90;
// Address of our node
const uint16_t this_node = 1;
// Address of the other node
const uint16_t other_node = 0;

// How many packets have we sent already
unsigned int packets_sent;

// Structure of our payload, limited to 32 bytes
struct payload_t			// 32 bytes max
{
  unsigned int counter;  // 2 bytes 0-65,535
  int temp1F;            // 2 bytes -32,768 to 32,767
  int humidityTempF;     // 2 bytes -32,768 to 32,767
  int Vcc;		 // 2 bytes -32,768 to 32,767
  int BatVcc;            // 2 bytes -32,768 to 32,767
  int SolarVcc;          // 2 bytes -32,768 to 32,767
  int humidity;          // 2 bytes -32,768 to 32,767
  int moist;             // 2 bytes -32,768 to 32,767
  int ph;                // 2 bytes -32,768 to 32,767
  int future1;           // 2 bytes -32,768 to 32,767
  int future2;           // 2 bytes -32,768 to 32,767
};

dht DHT;


// Prototypes
void getTemperature(DeviceAddress);	                // getTemperature
void getVoltage();					// getVoltage
void sendPayload();					// check if time to send payload

void setup(void)
{
  Serial.begin(115200);
  
	analogReference(INTERNAL); 		// Set analog reference to 1.1V
	analogRead(VccPin); 			//discard first analogRead
        analogRead(BatVccPin); 			//discard first analogRead
        analogRead(SolarVccPin); 			//discard first analogRead
	pinMode(ActivePin, OUTPUT);		// Set for output
  
	// Start up the library
	sensors.begin();

	// set the DS18B20 address 
	//sensors.getAddress(insideThermometer, 0); 
        sensors.getAddress(outsideThermometer, 0);
        
        // set the resolution to 12 bit
        //sensors.setResolution(insideThermometer, TEMPERATURE_PRECISION);
        sensors.setResolution(outsideThermometer, TEMPERATURE_PRECISION); 
        
	SPI.begin();
	radio.begin();
	network.begin(channel, this_node);

	// Power down the radio. Note that the radio will get powered back up on the next write() call.
	radio.powerDown();
}

void loop(void){
	// Pump the network regularly
	network.update();

        getHumidityandTemperature();
	getVoltage();
	getTemperature(outsideThermometer);
	sendPayload();
        delay(100);
	Narcoleptic.delay(sleepDelay); // During this time power consumption is minimized
}

//////////////////////////////////////////////////////////////////
// getTemperature
//////////////////////////////////////////////////////////////////
void getTemperature(DeviceAddress deviceAddress)
{
	sensors.requestTemperatures();
	temp1F = int(sensors.getTempF(deviceAddress)*100);
        Serial.print("Temp1:");
        Serial.println(temp1F);
}

//////////////////////////////////////////////////////////////////////////////////
// Read analog input for VccPin averaged over NUM_SAMPLES
// Uses a running average
// Vcc is scaled with a voltage divider * 75K/(75K + 240K) so reverse
// Should be 4.2, try 3.9
//////////////////////////////////////////////////////////////////////////////////
void getVoltage(){
		const byte NUM_SAMPLES = 10;
	float SumTotal=0;
	for (byte j=0;j<NUM_SAMPLES;j++){    
		SumTotal+= analogRead(VccPin);
		delay(2);
	}    
	
	//Vcc =  ((SumTotal/NUM_SAMPLES)*1.1/1023.0)*3.9;
          Vcc =  int((((SumTotal/NUM_SAMPLES)/1023.0)*6.25)*100);
        
        SumTotal=0;
	for (byte j=0;j<NUM_SAMPLES;j++){    
		SumTotal+= analogRead(BatVccPin);
		delay(2);
	}    		
	//Vcc =  ((SumTotal/NUM_SAMPLES)*1.1/1023.0)*3.9;
          BatVcc =  int((((SumTotal/NUM_SAMPLES)/1023)*6.15)*100);
          
        SumTotal=0;
	for (byte j=0;j<NUM_SAMPLES;j++){    
		SumTotal+= analogRead(SolarVccPin);
		delay(2);
	}    		
	//Vcc =  ((SumTotal/NUM_SAMPLES)*1.1/1023.0)*3.9;
          SolarVcc =  int((((SumTotal/NUM_SAMPLES)/1023.0)*6.18)*100);
  
}
//////////////////////////////////////////////////////////////////
// getHumidityandTemperature
//////////////////////////////////////////////////////////////////
void getHumidityandTemperature()
{
	DHT.read22(DHT22_PIN);
	humidity = int(DHT.humidity*100);
        humidityTempF = int(((DHT.temperature*1.8)+32)*100);

}

//////////////////////////////////////////////////////////////////////////////////
// sendPayload();					// send payload
//////////////////////////////////////////////////////////////////////////////////
void sendPayload(){
	digitalWrite(ActivePin, HIGH);		// Turn on LED
        Serial.print("packet:");Serial.println(packets_sent);
        Serial.print("node:");Serial.println(this_node);
        Serial.print("temp1F:");Serial.println(temp1F);
        Serial.print("humidityTempF:");Serial.println(humidityTempF);
        Serial.print("Vcc:");Serial.println(Vcc);
        Serial.print("BatVcc:");Serial.println(BatVcc);
        Serial.print("SolarVcc:");Serial.println(SolarVcc);
        Serial.print("humidity:");Serial.println(humidity);
        Serial.print("moisture:");Serial.println(moist);
        Serial.print("ph:");Serial.println(ph);
        Serial.print("future1:");Serial.println(future1);
        Serial.print("future2:");Serial.println(future2);
        
	payload_t payload = { packets_sent++, temp1F, humidityTempF, Vcc, BatVcc, SolarVcc, humidity,moist,ph,future1,future2};
	RF24NetworkHeader header(/*to node*/ other_node);
	bool ok = network.write(header,&payload,sizeof(payload));
        if(ok)
        Serial.println("Data Sent Off");
        else
        Serial.println("Error sending data");
	// Power down the radio. Note that the radio will get powered back up on the next write() call.
	radio.powerDown();
	digitalWrite(ActivePin, LOW);		// Turn off LED
}
