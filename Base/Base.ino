
/**
 * Example for efficient call-response using ack-payloads
 *
 * This example continues to make use of all the normal functionality of the radios including
 * the auto-ack and auto-retry features, but allows ack-payloads to be written optionlly as well.
 * This allows very fast call-response communication, with the responding radio never having to
 * switch out of Primary Receiver mode to send back a payload, but having the option to switch to
 * primary transmitter if wanting to initiate communication instead of respond to a commmunication.
 * The circuit:
 * LCD RS pin to digital pin 12
 * LCD Enable pin to digital pin 11
 * LCD D4 pin to digital pin 5
 * LCD D5 pin to digital pin 4
 * LCD D6 pin to digital pin 3
 * LCD D7 pin to digital pin 2
 * LCD R/W pin to ground
 * 10K resistor:
 * ends to +5V and ground
 * wiper to LCD VO pin (pin 3)
 */

#include <SPI.h>
#include <LiquidCrystal.h>
#include <RF24Network.h>
#include <RF24.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(3, 4, 8, 7, 6, 5);

#define SSID ""
#define PASS ""
#define DST_IP "192.41.101.140" //securecomo.com

boolean debugSerial = false;


// Number of Temperature sensors
#define NumNodes 5

// Set your own pins with these defines !
#define DS1302_SCLK_PIN   A0    // Arduino pin for the Serial Clock
#define DS1302_IO_PIN     A1    // Arduino pin for the Data I/O
#define DS1302_CE_PIN     A2    // Arduino pin for the Chip Enable


// Macros to convert the bcd values of the registers to normal
// integer variables.
// The code uses seperate variables for the high byte and the low byte
// of the bcd, so these macros handle both bytes seperately.
#define bcd2bin(h,l)    (((h)*10) + (l))
#define bin2bcd_h(x)   ((x)/10)
#define bin2bcd_l(x)    ((x)%10)

 
// Register names.
// Since the highest bit is always '1', 
// the registers start at 0x80
// If the register is read, the lowest bit should be '1'.
#define DS1302_SECONDS           0x80
#define DS1302_MINUTES           0x82
#define DS1302_HOURS             0x84
#define DS1302_DATE              0x86
#define DS1302_MONTH             0x88
#define DS1302_DAY               0x8A
#define DS1302_YEAR              0x8C
#define DS1302_ENABLE            0x8E
#define DS1302_TRICKLE           0x90
#define DS1302_CLOCK_BURST       0xBE
#define DS1302_CLOCK_BURST_WRITE 0xBE
#define DS1302_CLOCK_BURST_READ  0xBF
#define DS1302_RAMSTART          0xC0
#define DS1302_RAMEND            0xFC
#define DS1302_RAM_BURST         0xFE
#define DS1302_RAM_BURST_WRITE   0xFE
#define DS1302_RAM_BURST_READ    0xFF

// Defines for the bits, to be able to change 
// between bit number and binary definition.
// By using the bit number, using the DS1302 
// is like programming an AVR microcontroller.
// But instead of using "(1<<X)", or "_BV(X)", 
// the Arduino "bit(X)" is used.
#define DS1302_D0 0
#define DS1302_D1 1
#define DS1302_D2 2
#define DS1302_D3 3
#define DS1302_D4 4
#define DS1302_D5 5
#define DS1302_D6 6
#define DS1302_D7 7


// Bit for reading (bit in address)
#define DS1302_READBIT DS1302_D0 // READBIT=1: read instruction

// Bit for clock (0) or ram (1) area, 
// called R/C-bit (bit in address)
#define DS1302_RC DS1302_D6

// Seconds Register
#define DS1302_CH DS1302_D7   // 1 = Clock Halt, 0 = start

// Hour Register
#define DS1302_AM_PM DS1302_D5 // 0 = AM, 1 = PM
#define DS1302_12_24 DS1302_D7 // 0 = 24 hour, 1 = 12 hour

// Enable Register
#define DS1302_WP DS1302_D7   // 1 = Write Protect, 0 = enabled

// Trickle Register
#define DS1302_ROUT0 DS1302_D0
#define DS1302_ROUT1 DS1302_D1
#define DS1302_DS0   DS1302_D2
#define DS1302_DS1   DS1302_D2
#define DS1302_TCS0  DS1302_D4
#define DS1302_TCS1  DS1302_D5
#define DS1302_TCS2  DS1302_D6
#define DS1302_TCS3  DS1302_D7


// Structure for the first 8 registers.
// These 8 bytes can be read at once with 
// the 'clock burst' command.
// Note that this structure contains an anonymous union.
// It might cause a problem on other compilers.
typedef struct ds1302_struct
{
  uint8_t Seconds:4;      // low decimal digit 0-9
  uint8_t Seconds10:3;    // high decimal digit 0-5
  uint8_t CH:1;           // CH = Clock Halt
  uint8_t Minutes:4;
  uint8_t Minutes10:3;
  uint8_t reserved1:1;
  union
  {
    struct
    {
      uint8_t Hour:4;
      uint8_t Hour10:2;
      uint8_t reserved2:1;
      uint8_t hour_12_24:1; // 0 for 24 hour format
    } h24;
    struct
    {
      uint8_t Hour:4;
      uint8_t Hour10:1;
      uint8_t AM_PM:1;      // 0 for AM, 1 for PM
      uint8_t reserved2:1;
      uint8_t hour_12_24:1; // 1 for 12 hour format
    } h12;
  };
  uint8_t Date:4;           // Day of month, 1 = first day
  uint8_t Date10:2;
  uint8_t reserved3:2;
  uint8_t Month:4;          // Month, 1 = January
  uint8_t Month10:1;
  uint8_t reserved4:3;
  uint8_t Day:3;            // Day of week, 1 = first day (any day)
  uint8_t reserved5:5;
  uint8_t Year:4;           // Year, 0 = year 2000
  uint8_t Year10:4;
  uint8_t reserved6:7;
  uint8_t WP:1;             // WP = Write Protect
};

//declare the Structure for ds1302
ds1302_struct rtc;

// Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 9 & 10
RF24 radio(9, 10);
// Network uses that radio
RF24Network network(radio);

// Channel of our node
const uint16_t channel = 90;
// Address of our node
const uint16_t this_node = 0;

// Structure of our payload
struct payload_t
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

// Since nodes start at 1, I added 1 to arrays to make it easier
unsigned int NodeCounter[NumNodes + 1];  // 2 bytes 0-65,535
int NodeTemp1F[NumNodes + 1];            // 2 bytes -32,768 to 32,767
int NodeHumidityTempF[NumNodes + 1];     // 2 bytes -32,768 to 32,767
int NodeVcc[NumNodes + 1];               // 2 bytes -32,768 to 32,767
int NodeBatVcc[NumNodes + 1];            // 2 bytes -32,768 to 32,767
int NodeSolarVcc[NumNodes + 1];          // 2 bytes -32,768 to 32,767
int NodeHumidity[NumNodes + 1];          // 2 bytes -32,768 to 32,767
int NodeMoisture[NumNodes + 1];          // 2 bytes -32,768 to 32,767
int NodePH[NumNodes + 1];                // 2 bytes -32,768 to 32,767
int NodeFuture1[NumNodes + 1];           // 2 bytes -32,768 to 32,767
int NodeFuture2[NumNodes + 1];           // 2 bytes -32,768 to 32,767

//Initiate next upload of data information to the server; It's either 4 or 9 from rtc.Minutes
byte nextUpload;


//Prototypes for utility functions
void displayData();					// display data
void getRadioData();				// get Radio data
void clearTemperatures();			// clear temps to make sure modules are active
void updateTime();
void checkTime();


void setup() {
  lcd.begin(16, 2);
  delay(5);
  pinMode(A5, OUTPUT);   //esp8266 CH_PD
  pinMode(A4, OUTPUT);   //esp8266 rst
  delay(5);
  SPI.begin();
  delay(5);
  // Radio setup
  radio.begin();
  // network.begin(/*channel*/, /*node address*/);
  network.begin(channel, this_node);
  delay(5);
  
  lcd.clear();lcd.home();lcd.print(F("Korybantes v2.12"));
  Serial.begin(9600);
  
  delay(2000);
  digitalWrite(A5, HIGH);
  digitalWrite(A4, HIGH);
  delay(2000);

  while(!checkWiFi())
  
  lcd.clear();lcd.home();lcd.print(F("Wifi Ready!"));
  Serial.println(F("AT+CWMODE=1"));    //Run Esp8266 in Standard Router mode
  delay(500);
  // Read all clock data at once (burst mode).
    DS1302_clock_burst_read( (uint8_t *) &rtc);
  delay(5);
   if(debugSerial){
    Serial.print(F("Time: "));
    Serial.println(((rtc.Minutes*60)+bcd2bin( rtc.Seconds10, rtc.Seconds)));
    Serial.print(F("nextUpload="));
    Serial.print(nextUpload);
    Serial.print(F(" && rtc.Minutes="));
    Serial.println(rtc.Minutes);
  }
  if(((rtc.Minutes*60)+bcd2bin(rtc.Seconds10, rtc.Seconds)>285))
      nextUpload=9;
    else
      nextUpload=4;
   if(debugSerial){
    Serial.print(F("Time: "));
    Serial.println(((rtc.Minutes*60)+bcd2bin( rtc.Seconds10, rtc.Seconds)));
    Serial.print(F("nextUpload="));
    Serial.print(nextUpload);
    Serial.print(F(" && rtc.Minutes="));
    Serial.println(rtc.Minutes);
  }
  updateTime();   //Grab NTP info from our server and update the ds1302 RTC
}

void loop() {
  lcd.clear();
  lcd.home();
  for(int i=0;i<35;i++){
    // Read all clock data at once (burst mode).
    DS1302_clock_burst_read( (uint8_t *) &rtc);
    lcd.setCursor(0, 0);
    lcd.print(bcd2bin( rtc.Month10, rtc.Month));
    lcd.print(F("/"));
    lcd.print(bcd2bin( rtc.Date10, rtc.Date));
    lcd.print(F("/20"));
    lcd.print(rtc.Year10);
    lcd.print(rtc.Year);
    lcd.setCursor(0, 1);
    if(bcd2bin( rtc.h24.Hour10, rtc.h24.Hour)<10)
    lcd.print(F("0"));
    lcd.print(bcd2bin( rtc.h24.Hour10, rtc.h24.Hour));
    lcd.print(F(":"));
    if(bcd2bin( rtc.Minutes10, rtc.Minutes)<10)
    lcd.print(F("0"));
    lcd.print(bcd2bin( rtc.Minutes10, rtc.Minutes));
    lcd.print(F(":"));
    if(bcd2bin( rtc.Seconds10, rtc.Seconds)<10)
    lcd.print(F("0"));
    lcd.print(bcd2bin( rtc.Seconds10, rtc.Seconds));
    checkTime();
    delay(300); 
  }
  // Pump the network regularly
  network.update();
  // Is there anything ready for us?
  while (network.available()) {
    // If so, grab it and print it out
    getRadioData();
  } 
  displayData();  
}

boolean checkWiFi(){
lcd.clear();lcd.home();lcd.print(F("Checking Wifi"));
if (Serial.find("ready"))
  return true;
 Serial.print(F("AT\r\n"));
   delay(2500);
    if (Serial.find("OK")) {
        return true;
      }else{
        lcd.clear();lcd.home();lcd.print(F("No response!"));
        //resetEsp8266();
        delay(500);
        return false;
      }     
}

boolean connectWiFi(){
lcd.clear();lcd.home();lcd.print(F("Connecting Wifi"));
Serial.print(F("AT+CWJAP=\""));
Serial.print(SSID);
Serial.print(F("\",\""));
Serial.print(PASS);
Serial.print(F("\"\r\n"));
delay(2500);
if(Serial.find("OK")){
  lcd.clear();lcd.home();lcd.print(F("Connected 2 Wifi"));
  delay(1000);
  return true;
 }
delay(2500);
 if(Serial.find("OK")){
  lcd.clear();lcd.home();lcd.print(F("Connected 2 Wifi"));
  delay(1000);
  return true;
 }else{
   lcd.clear();lcd.home();lcd.print(F("error with Wifi"));
   delay(1000);
   resetEsp8266();
   return false;
 }
}


void updateTime(){
 if(debugSerial)
   Serial.println(F("UpdateTime"));
   delay(1000);
 while(!connectWiFi())
 delay(5);
 //set the single connection mode
 Serial.println(F("AT+CIPMUX=0"));
  lcd.clear();lcd.home();lcd.print(F("Updating Time"));

  Serial.print(F("AT+CIPSTART=\"TCP\",\""));
  Serial.print(DST_IP);
  Serial.println(F("\",80"));
  delay(50);
  if(Serial.find("Error")){
    lcd.clear();lcd.home();lcd.print(F("RECEIVED: Error"));
    //resetEsp8266();
    return;
  }
  delay(100);
  Serial.println(F("AT+CIPSEND=47"));
  if(Serial.find(">")){
    Serial.print(F("GET /scripts/esp8266/dicky/update.php?op=time\r\n"));
  }else{
    Serial.print(F("AT+CIPCLOSE"));
    lcd.clear();lcd.home();lcd.print(F("Connect Timeout"));
    //resetEsp8266();
    delay(500);
  }
  if(Serial.find("OK")){
    lcd.clear();lcd.home();lcd.print(F("RECEIVED: OK"));
    delay(500);
    if(Serial.find("+IPD,13:")){       // Look for char in serial queue and process if found
        set1302Date();
    }
  }else{
    lcd.clear();lcd.home();lcd.print(F("RECEIVED: Error"));
    //resetEsp8266();
    updateTime();
  }
  delay(50);
}


void sendData(){
 if(debugSerial)
   Serial.println(F("Send Data"));
   delay(1000);
  String cmd;
  //connect to the wifi
 while(!connectWiFi())
 
 delay(50);
 //set the single connection mode
 Serial.println(F("AT+CIPMUX=0"));
  lcd.clear();lcd.home();lcd.print(F("Uploading Data"));
    
  Serial.print(F("AT+CIPSTART=\"TCP\",\""));
  Serial.print(DST_IP);
  Serial.println(F("\",80"));
  delay(50);
  if(Serial.find("Error")){
    lcd.clear();lcd.home();lcd.print(F("RECEIVED: Error"));
    //resetEsp8266();
    return;
  }
  delay(500);
  String GET = "GET /scripts/esp8266/dicky/update.php?";
  cmd = GET;
  if(NumNodes==1){
       cmd+="name=";cmd+="Node1";
       cmd+="&temp1=";cmd+=float(NodeTemp1F[1])/100; // Don't add comma at the end
       cmd+="&humidityTempF=";cmd+=float(NodeHumidityTempF[1])/100;
       cmd+="&humidity=";cmd+=float(NodeHumidity[1])/100;
       cmd+="&volt1=";cmd+=float(NodeVcc[1])/100;
       cmd+="&volt2=";cmd+=float(NodeBatVcc[1])/100;
       cmd+="&volt3=";cmd+=float(NodeSolarVcc[1])/100;
       cmd+="&moisture=";cmd+=float(NodeMoisture[1])/100;
       cmd+="&ph=";cmd+=float(NodePH[1])/100;
       cmd+="&future1=";cmd+=float(NodeFuture1[1])/100;
       cmd+="&future2=";cmd+=float(NodeFuture2[1])/100;
     }else{
       String str_temp="name=";
       int i;
       for (i = 1; i <= NumNodes; i++){
           str_temp+="Node";str_temp+=i;
           if(NumNodes>i)
           str_temp+=",";                                   //output should look like this Temp1 = "Node1,Node2,Node3..etc";
       }
           cmd+=str_temp;str_temp="&temp1=";                                  //save to cmd
           
       for (i = 1; i <= NumNodes; i++){   
           str_temp+=float(NodeTemp1F[i])/100;
            if(NumNodes>i)
           str_temp+=",";          //output should look like this Temp1 = "99.9,99.9,99.9...etc";
       }
           cmd+=str_temp;str_temp="&humidityTempF=";                                  //save to cmd
           
       for (i = 1; i <= NumNodes; i++){    
           str_temp+=float(NodeHumidityTempF[i])/100;
            if(NumNodes>i)
           str_temp+=",";          //output should look like this Temp1 = "99.9,99.9,99.9...etc";
       }
           cmd+=str_temp;str_temp="&volt1=";                                  //save to cmd
           
       for (i = 1; i <= NumNodes; i++){    
           str_temp+=float(NodeVcc[i])/100;
            if(NumNodes>i)
           str_temp+=",";             //output should look like this Temp1 = "3.33,3.33,3.33...etc";
       }  
           cmd+=str_temp;str_temp="&volt2=";                                  //save to cmd
           
       for (i = 1; i <= NumNodes; i++){    
           str_temp+=float(NodeBatVcc[i])/100;
            if(NumNodes>i)
           str_temp+=",";          //output should look like this Temp1 = "3.33,3.33,3.33...etc";
       }
           cmd+=str_temp;str_temp="&volt3=";                                  //save to cmd
           
       for (i = 1; i <= NumNodes; i++){    
           str_temp+=float(NodeSolarVcc[i])/100;
            if(NumNodes>i)
           str_temp+=",";        //output should look like this Temp1 = "3.33,3.33,3.33...etc";
       }
           cmd+=str_temp;str_temp="&humidity=";                                  //save to cmd
           
       for (i = 1; i <= NumNodes; i++){    
           str_temp+=float(NodeHumidity[i])/100;
            if(NumNodes>i)
           str_temp+=",";        //output should look like this Temp1 = "3.33,3.33,3.33...etc";
       }
           cmd+=str_temp;str_temp="&moisture";                                  //save to cmd
           
       for (i = 1; i <= NumNodes; i++){    
           str_temp+=float(NodeMoisture[i])/100;
            if(NumNodes>i)
           str_temp+=",";        //output should look like this Temp1 = "3.33,3.33,3.33...etc";
       }
           cmd+=str_temp;str_temp="&ph";                                  //save to cmd
           
       for (i = 1; i <= NumNodes; i++){    
           str_temp+=float(NodePH[i])/100;
            if(NumNodes>i)
           str_temp+=",";        //output should look like this Temp1 = "3.33,3.33,3.33...etc";
       }
           cmd+=str_temp;str_temp="&future1";                                  //save to cmd
           
       for (i = 1; i <= NumNodes; i++){    
           str_temp+=float(NodeFuture1[i])/100;
            if(NumNodes>i)
           str_temp+=",";        //output should look like this Temp1 = "3.33,3.33,3.33...etc";
       }
           cmd+=str_temp;str_temp="&future2";                                  //save to cmd
           
       for (i = 1; i <= NumNodes; i++){    
           str_temp+=float(NodeFuture2[i])/100;
            if(NumNodes>i)
           str_temp+=",";        //output should look like this Temp1 = "3.33,3.33,3.33...etc";
       }
           cmd+=str_temp;str_temp="";                                  //save to cmd
           
    }  
  cmd += "\r\n";
  Serial.print(F("AT+CIPSEND="));
  Serial.println(cmd.length());
  if(Serial.find(">")){
    Serial.print(cmd);
  }else{
    Serial.print(F("AT+CIPCLOSE"));
    lcd.clear();lcd.home();lcd.print(F("Connect Timeout"));
    //resetEsp8266();
    delay(500);
  }
  if(Serial.find("OK")){  
    lcd.clear();lcd.home();lcd.print(F("RECEIVED: OK"));
    delay(1000);
    clearTemperatures();
  }else{
    lcd.clear();lcd.home();lcd.print(F("RECEIVED: Error"));
    delay(1000);
    //resetEsp8266();
    sendData();
  }
  delay(50);
}

void resetEsp8266(){
   digitalWrite(A4, LOW);       //reset esp8266
   delay(1000);
   digitalWrite(A4, HIGH);
   delay(1000);
   Serial.println(F("AT+CWMODE=1"));    //Run Esp8266 in Standard Router mode
}
void checkTime(){
   // Read all clock data at once (burst mode).
  DS1302_clock_burst_read( (uint8_t *) &rtc);
  
  delay(5);
  
  byte hours = bcd2bin(rtc.h24.Hour10, rtc.h24.Hour);
  byte minutes = bcd2bin(rtc.Minutes10, rtc.Minutes);
  byte secounds = bcd2bin(rtc.Seconds10, rtc.Seconds);
 
  if(debugSerial){
    Serial.print(F("Time: "));
    Serial.println(((rtc.Minutes*60)+bcd2bin( rtc.Seconds10, rtc.Seconds)));
    Serial.print(F("nextUpload="));
    Serial.print(nextUpload);
    Serial.print(F(" && rtc.Minutes="));
    Serial.println(rtc.Minutes);
  }
  //if(((rtc.Minutes*60)+secounds)>=585||(((rtc.Minutes*60)+secounds)>=285&&((rtc.Minutes*60)+secounds)<290))
  if((((rtc.Minutes*60)+secounds)>=585||(((rtc.Minutes*60)+secounds)>=285&&((rtc.Minutes*60)+secounds)<300))&&nextUpload==rtc.Minutes){
    if(nextUpload==4)
       nextUpload=9;
    else
       nextUpload=4;
    sendData();
  }
  if((hours==0&&minutes==2&&secounds==0)||(hours==12&&minutes==2&&secounds==0)||(hours==6&&minutes==2&&secounds==0)||(hours==18&&minutes==2&&secounds==0))
     updateTime(); 

   if(debugSerial){
    Serial.print(F("Time: "));
    Serial.println(((rtc.Minutes*60)+bcd2bin( rtc.Seconds10, rtc.Seconds)));
    Serial.print(F("nextUpload="));
    Serial.print(nextUpload);
    Serial.print(F(" && rtc.Minutes="));
    Serial.println(rtc.Minutes);
  }     
}
  

//////////////////////////////////////////////////////////////////////////////////
// displayData()
//////////////////////////////////////////////////////////////////////////////////
void displayData() {
  // Lcd1602 will display 16 characters x 2 rows
 
  for (byte i = 1; i <= NumNodes; i++) {
    lcd.clear();lcd.home();lcd.print(F("Node "));
    lcd.print(i);
    lcd.print(F(": "));
    lcd.print(float(NodeTemp1F[i])/100);
    lcd.print(F("F "));

    lcd.setCursor(0, 1);
    lcd.print(F("Vcc: "));
    lcd.print(float(NodeVcc[i]/100));
    lcd.print(F("V"));
    for(int i=0;i<5;i++){
       checkTime();
       delay(400);
     }
    //delay(2000);
    lcd.clear();lcd.home();lcd.print(F("Node "));
    lcd.print(i);
    lcd.print(F(": "));
    lcd.print(float(NodeHumidityTempF[i])/100);
    lcd.print(F("F "));

    lcd.setCursor(0, 1);
    lcd.print(F("BattVcc: "));
    lcd.print(float(NodeBatVcc[i])/100);
    lcd.print(F("V"));
    for(int i=0;i<5;i++){
       checkTime();
       delay(400);
     }
    //delay(2000);
    lcd.clear();lcd.home();lcd.print(F("Node "));
    lcd.print(i);
    lcd.print(F(": "));
    lcd.print(float(NodeHumidity[i])/100);
    lcd.print(F("% "));

    lcd.setCursor(0, 1);
    lcd.print(F("SolarVcc: "));
    lcd.print(float(NodeSolarVcc[i])/100);
    lcd.print(F("V"));
    for(int i=0;i<8;i++){
       checkTime();
       delay(400);
     }
    //delay(2000);
  }
}
//////////////////////////////////////////////////////////////////////////////////
// Get Serial time and date from server and set it to ds1302
//////////////////////////////////////////////////////////////////////////////////
void set1302Date() {
  byte seconds, minutes, mil_hours, hours, dayofweek, dayofmonth, month, year;
  //T(sec)(min)(hour)(dayOfWeek)(dayOfMonth)(month)(year)
 
  //T(00-59)(00-59)(00-23)(1-7)(01-31)(01-12)(00-99)
 
  //Example: 02-Feb-09 @ 19:57:11 for the 3rd day of the week -> T1157193020209
 
  seconds = (byte) ((Serial.read() - 48) * 10 + (Serial.read() - 48)); // Use of (byte) type casting and ascii math to achieve result.  
 
  minutes = (byte) ((Serial.read() - 48) *10 +  (Serial.read() - 48));
 
  hours   = (byte) ((Serial.read() - 48) *10 +  (Serial.read() - 48));
 
  dayofweek     = (byte) (Serial.read() - 48);
 
  dayofmonth    = (byte) ((Serial.read() - 48) *10 +  (Serial.read() - 48));
 
  month   = (byte) ((Serial.read() - 48) *10 +  (Serial.read() - 48));
 
  year    = (byte) ((Serial.read() - 48) *10 +  (Serial.read() - 48));
  
  
 // Start by clearing the Write Protect bit
  // Otherwise the clock data cannot be written
  // The whole register is written, 
  // but the WP-bit is the only bit in that register.
  DS1302_write (DS1302_ENABLE, 0);

  // Disable Trickle Charger.
  DS1302_write (DS1302_TRICKLE, 0x00);

// Remove the next define, 
// after the right date and time are set.
//#define SET_DATE_TIME_JUST_ONCE
//#ifdef SET_DATE_TIME_JUST_ONCE  

  // Fill these variables with the date and time.
  //int seconds, minutes, hours, dayofweek, dayofmonth, month, year;

  // Example for april 15, 2013, 10:08, monday is 2nd day of Week.
  // Set your own time and date in these variables.
 /* seconds    = 0;
  minutes    = 8;
  hours      = 10;
  dayofweek  = 2;  // Day of week, any day can be first, counts 1...7
  dayofmonth = 15; // Day of month, 1...31
  month      = 4;  // month 1...12
  year       = 2013;
*/
  // Set a time and date
  // This also clears the CH (Clock Halt) bit, 
  // to start the clock.

  // Fill the structure with zeros to make 
  // any unused bits zero
  memset ((char *) &rtc, 0, sizeof(rtc));

  rtc.Seconds    = bin2bcd_l( seconds);
  rtc.Seconds10  = bin2bcd_h( seconds);
  rtc.CH         = 0;      // 1 for Clock Halt, 0 to run;
  rtc.Minutes    = bin2bcd_l( minutes);
  rtc.Minutes10  = bin2bcd_h( minutes);
  // To use the 12 hour format,
  // use it like these four lines:
  //    rtc.h12.Hour   = bin2bcd_l( hours);
  //    rtc.h12.Hour10 = bin2bcd_h( hours);
  //    rtc.h12.AM_PM  = 0;     // AM = 0
  //    rtc.h12.hour_12_24 = 1; // 1 for 24 hour format
  rtc.h24.Hour   = bin2bcd_l( hours);
  rtc.h24.Hour10 = bin2bcd_h( hours);
  rtc.h24.hour_12_24 = 0; // 0 for 24 hour format
  rtc.Date       = bin2bcd_l( dayofmonth);
  rtc.Date10     = bin2bcd_h( dayofmonth);
  rtc.Month      = bin2bcd_l( month);
  rtc.Month10    = bin2bcd_h( month);
  rtc.Day        = dayofweek;
  rtc.Year       = bin2bcd_l( year - 2000);
  rtc.Year10     = bin2bcd_h( year - 2000);
  rtc.WP = 0;  

  // Write all clock data at once (burst mode).
  DS1302_clock_burst_write( (uint8_t *) &rtc);
//#endif
}


//////////////////////////////////////////////////////////////////////////////////
// getRadioData()					// get Network data
//////////////////////////////////////////////////////////////////////////////////
void getRadioData() {
  RF24NetworkHeader header;
  payload_t payload;
  bool done = false;

  while (!done) {

      done = 	network.read(header, &payload, sizeof(payload));
  
      NodeCounter[header.from_node] = payload.counter;
      NodeTemp1F[header.from_node] = payload.temp1F;
      NodeHumidityTempF[header.from_node] = payload.humidityTempF;
      NodeVcc[header.from_node] = payload.Vcc;
      NodeBatVcc[header.from_node] = payload.BatVcc;
      NodeSolarVcc[header.from_node] = payload.SolarVcc;
      NodeHumidity[header.from_node] = payload.humidity;
      NodeMoisture[header.from_node] = payload.moist;
      NodePH[header.from_node] = payload.ph;
      NodeFuture1[header.from_node] = payload.future1;
      NodeFuture2[header.from_node] = payload.future2;
    }
}

//////////////////////////////////////////////////////////////////////////////////
// clearTemperatures()					// clear temps to make sure modules are active
//////////////////////////////////////////////////////////////////////////////////

void clearTemperatures() {
  for (int i = 1; i <= NumNodes; i++) {
    NodeTemp1F[i] = 0;
    NodeHumidityTempF[i] = 0;
    NodeVcc[i] = 0;
    NodeBatVcc[i] = 0;
    NodeSolarVcc[i] = 0;
    NodeHumidity[i]= 0;
    NodeMoisture[i] = 0;
    NodePH[i] = 0;
    NodeFuture1[i] = 0;
    NodeFuture2[i]= 0;
  }
}

// --------------------------------------------------------
// DS1302_clock_burst_read
//
// This function reads 8 bytes clock data in burst mode
// from the DS1302.
//
// This function may be called as the first function, 
// also the pinMode is set.
//
void DS1302_clock_burst_read( uint8_t *p)
{
  int i;

  _DS1302_start();

  // Instead of the address, 
  // the CLOCK_BURST_READ command is issued
  // the I/O-line is released for the data
  _DS1302_togglewrite( DS1302_CLOCK_BURST_READ, true);  

  for( i=0; i<8; i++)
  {
    *p++ = _DS1302_toggleread();
  }
  _DS1302_stop();
}


// --------------------------------------------------------
// DS1302_clock_burst_write
//
// This function writes 8 bytes clock data in burst mode
// to the DS1302.
//
// This function may be called as the first function, 
// also the pinMode is set.
//
void DS1302_clock_burst_write( uint8_t *p)
{
  int i;

  _DS1302_start();

  // Instead of the address, 
  // the CLOCK_BURST_WRITE command is issued.
  // the I/O-line is not released
  _DS1302_togglewrite( DS1302_CLOCK_BURST_WRITE, false);  

  for( i=0; i<8; i++)
  {
    // the I/O-line is not released
    _DS1302_togglewrite( *p++, false);  
  }
  _DS1302_stop();
}


// --------------------------------------------------------
// DS1302_read
//
// This function reads a byte from the DS1302 
// (clock or ram).
//
// The address could be like "0x80" or "0x81", 
// the lowest bit is set anyway.
//
// This function may be called as the first function, 
// also the pinMode is set.
//
/*uint8_t DS1302_read(int address)
{
  uint8_t data;

  // set lowest bit (read bit) in address
  bitSet( address, DS1302_READBIT);  

  _DS1302_start();
  // the I/O-line is released for the data
  _DS1302_togglewrite( address, true);  
  data = _DS1302_toggleread();
  _DS1302_stop();

  return (data);
}
*/

// --------------------------------------------------------
// DS1302_write
//
// This function writes a byte to the DS1302 (clock or ram).
//
// The address could be like "0x80" or "0x81", 
// the lowest bit is cleared anyway.
//
// This function may be called as the first function, 
// also the pinMode is set.
//
void DS1302_write( int address, uint8_t data)
{
  // clear lowest bit (read bit) in address
  bitClear( address, DS1302_READBIT);   

  _DS1302_start();
  // don't release the I/O-line
  _DS1302_togglewrite( address, false); 
  // don't release the I/O-line
  _DS1302_togglewrite( data, false); 
  _DS1302_stop();  
}


// --------------------------------------------------------
// _DS1302_start
//
// A helper function to setup the start condition.
//
// An 'init' function is not used.
// But now the pinMode is set every time.
// That's not a big deal, and it's valid.
// At startup, the pins of the Arduino are high impedance.
// Since the DS1302 has pull-down resistors, 
// the signals are low (inactive) until the DS1302 is used.
void _DS1302_start( void)
{
  digitalWrite( DS1302_CE_PIN, LOW); // default, not enabled
  pinMode( DS1302_CE_PIN, OUTPUT);  

  digitalWrite( DS1302_SCLK_PIN, LOW); // default, clock low
  pinMode( DS1302_SCLK_PIN, OUTPUT);

  pinMode( DS1302_IO_PIN, OUTPUT);

  digitalWrite( DS1302_CE_PIN, HIGH); // start the session
  delayMicroseconds( 4);           // tCC = 4us
}


// --------------------------------------------------------
// _DS1302_stop
//
// A helper function to finish the communication.
//
void _DS1302_stop(void)
{
  // Set CE low
  digitalWrite( DS1302_CE_PIN, LOW);

  delayMicroseconds( 4);           // tCWH = 4us
}


// --------------------------------------------------------
// _DS1302_toggleread
//
// A helper function for reading a byte with bit toggle
//
// This function assumes that the SCLK is still high.
//
uint8_t _DS1302_toggleread( void)
{
  uint8_t i, data;

  data = 0;
  for( i = 0; i <= 7; i++)
  {
    // Issue a clock pulse for the next databit.
    // If the 'togglewrite' function was used before 
    // this function, the SCLK is already high.
    digitalWrite( DS1302_SCLK_PIN, HIGH);
    delayMicroseconds( 1);

    // Clock down, data is ready after some time.
    digitalWrite( DS1302_SCLK_PIN, LOW);
    delayMicroseconds( 1);        // tCL=1000ns, tCDD=800ns

    // read bit, and set it in place in 'data' variable
    bitWrite( data, i, digitalRead( DS1302_IO_PIN)); 
  }
  return( data);
}


// --------------------------------------------------------
// _DS1302_togglewrite
//
// A helper function for writing a byte with bit toggle
//
// The 'release' parameter is for a read after this write.
// It will release the I/O-line and will keep the SCLK high.
//
void _DS1302_togglewrite( uint8_t data, uint8_t release)
{
  int i;

  for( i = 0; i <= 7; i++)
  { 
    // set a bit of the data on the I/O-line
    digitalWrite( DS1302_IO_PIN, bitRead(data, i));  
    delayMicroseconds( 1);     // tDC = 200ns

    // clock up, data is read by DS1302
    digitalWrite( DS1302_SCLK_PIN, HIGH);     
    delayMicroseconds( 1);     // tCH = 1000ns, tCDH = 800ns

    if( release && i == 7)
    {
      // If this write is followed by a read, 
      // the I/O-line should be released after 
      // the last bit, before the clock line is made low.
      // This is according the datasheet.
      // I have seen other programs that don't release 
      // the I/O-line at this moment,
      // and that could cause a shortcut spike 
      // on the I/O-line.
      pinMode( DS1302_IO_PIN, INPUT);

      // For Arduino 1.0.3, removing the pull-up is no longer needed.
      // Setting the pin as 'INPUT' will already remove the pull-up.
      // digitalWrite (DS1302_IO, LOW); // remove any pull-up  
    }
    else
    {
      digitalWrite( DS1302_SCLK_PIN, LOW);
      delayMicroseconds( 1);       // tCL=1000ns, tCDD=800ns
    }
  }
}
