// JeeLib for low-power sleep function
#include <JeeLib.h>
#include <DHT.h>
#include <SoftwareSerial.h>
#define SENSOR_ID        0
#define PIN_DHT          2
#define PIN_BLUETOOTH_TX 4
#define PIN_BLUETOOTH_RX 7
#define DHTTYPE DHT22
unsigned int currStringIndexAck = 0;
DHT dht(PIN_DHT, DHTTYPE);
SoftwareSerial bluetooth(PIN_BLUETOOTH_TX, PIN_BLUETOOTH_RX);
unsigned long ioTime = 0;
String strAck;
// Setup the JeeLib watchdog function
ISR(WDT_vect)
{
  Sleepy::watchdogEvent(); 
}
// this function is taken from this website:
//  https://jeelabs.org/2012/05/04/measuring-vcc-via-the-bandgap/
int vccRead(byte us = 250)
{
  analogRead(6);
  bitSet(ADMUX, 3);
  delayMicroseconds(us);
  bitSet(ADCSRA, ADSC);
  while(bit_is_set(ADCSRA, ADSC));
  word x = ADC;
  return x ? (1100L*1023)/x : -1;
}
// returns 0 if we're not even on the first char of str yet
//  returns 1 if we've matched at least 1 char of str
//  returns 2 if we have just matched the last char of str
int serialStringCompare(const char* const str, unsigned strSize,
  char nextChar, unsigned int* currStrIndex)
{
  if(*currStrIndex < strSize &&
    str[*currStrIndex] == nextChar)
  {
    (*currStrIndex)++;
    if(*currStrIndex >= strSize)
    {
      *currStrIndex = 0;
      return 2;
    }
    // do not reset currStrIndex because we are still reading
    //  a valid string match up to this point!
    return 1;
  }
  *currStrIndex = 0;
  return 0;
}
void setup()
{
  // MUST WAIT 500ms for the bluetooth modem to boot
  //  see documentation page 45
  // https://cdn.sparkfun.com/datasheets/Wireless/Bluetooth/bluetooth_cr_UG-v1.0r.pdf
  // Assuming, ofc, that cpu & modem power on simultaneously
  delay(500);
  Serial.begin(9600);
  Serial.println("DHT22 sensor");
  dht.begin();
  // set the bluetooth serial line to the modem's default baud
  bluetooth.begin(115200);
  // enter command mode on the modem
  bluetooth.print("$$$");
  delay(100);// wait for "CMD" response
  // Adjust the Inquiry Scan Window to the lowest possible value
  //  (uses less power, delays discovery time)
//  bluetooth.println("SI,0100"); //default
  bluetooth.print("SI,0012\n");
  delay(100);// wait for "AOK" response
  // Adjust the Page Scan Window to the lowest possible value
  //  (uses less power, delays connection time)
  bluetooth.print("SJ,0012\n");
//  bluetooth.print("SJ,0100\n"); //default
  delay(100);// wait for "AOK" response
  // Enable Sniff Mode to put the radio to sleep every X milliseconds
  //  NOTE: this only works when you account for this stupid fucking bluetooth
  //  adapter not enabling it unless you have like 20 seconds of radio silence
  //  for SOME FUCKING REASON (see comments at the top of code regarding "bullshit")
  // sleep time =  625 μs x 0x<value>
//  bluetooth.println("SW,12C0");// 625*0x12C0 = 3,000,000μs = 3seconds (SUPPOSEDLY)
  bluetooth.println("SW,1F40");// 625*0x1F40 = 5,000,000μs = 5seconds (SUPPOSEDLY)
//  bluetooth.println("SW,3E80");// 625*0x3E80 = 10,000,000μs = 10seconds (SUPPOSEDLY)
  delay(100);// wait for "AOK" response
  // Lower the transmission power to lowest level
  //  (uses less power, decreases transmission range)
//  bluetooth.print("SY,0010\n"); //default (16 dBM)
  bluetooth.print("SY,FFF4\n");// lowest possible value (-12 dBM)
  delay(100);// wait for "AOK" response
  // lower the baud of the modem as well as the serial interface
  //  this command also takes the modem out of command mode
  bluetooth.print("U,9600,N\n");
  delay(100);// wait for "AOK" response
  bluetooth.begin(9600);
}
void loop()
{
  const unsigned long currTime = millis();
  ///TODO: handle the edge case where currTime <= ioTime when
  /// the unsigned long loops after ~50 days or so
  /// or maybe not?...
  // Need to wait a sufficient amount of time for an acknowledgement that
  //  the previous data packet was sent successfully
  if (currTime - ioTime > 30000)
  {
    const float humidity    = dht.readHumidity();
    if (isnan(humidity))
    {
      ioTime = currTime;
      Serial.println("Corrupted humidity!");
      return;
    }
    const float temperature = dht.readTemperature();
    if (isnan(temperature))
    {
      ioTime = currTime;
      Serial.println("Corrupted temperature!");
      return;
    }
    const float voltage     = vccRead()/1000.f;
    if (isnan(voltage))
    {
      ioTime = currTime;
      Serial.println("Corrupted voltage!");
      return;
    }
    // limit the size of the ack to 20 bits, or 5 base16 ascii digits
    //  or 4 base36 ascii digits!
    //  which will cause our ack time to loop every 17.47625 minutes,
    //  but will limit the size of our ack!
    const unsigned long ackMilliSeconds = currTime & 0xFFFFF;
    strAck = String(ackMilliSeconds, 36);
    const String data = "DHT{"+
      String(SENSOR_ID)+
      ","+String(humidity   , 1)+
      ","+String(temperature, 1)+
      ","+String(voltage    , 3)+
      ","+strAck+"}";
    Serial.print("("+String(ackMilliSeconds)+")");
    Serial.println(data);
    bluetooth.print(data);
    ioTime = currTime;
  }
  if (bluetooth.available())
  {
    const char nextBtChar = (char)bluetooth.read();
    const int compareToAckStr = serialStringCompare(
      strAck.c_str(), strAck.length(), nextBtChar, &currStringIndexAck);
    if(compareToAckStr != 0)
    {
      if(compareToAckStr == 2)
      {
        // At this point, we have read the entire ack string
        //  from the bluetooth connection, so we know that a
        //  complete data string was sent over the connection //
        // Okay so, after much debugging, I have figured out how to force this
        //  piece of shit bluetooth adapter to enter into a powersave mode
        //  (SNIFF MODE), by delaying things right here and not transferring any
        //  data for like 20 seconds once a connection has been established,
        //  verified with an acknowledgement packet from the server
        Serial.println("Got Ack!");
        // wait for the debug print message to be displayed in the serial output
        delay(500);
        // JeeLib low-power sleep!
        Sleepy::loseSomeTime(30000);
        // wait some time for the device to fully wake up again
        delay(500);
      }
    }
    else
    {
      ioTime = currTime;
      Serial.print(nextBtChar);
    }
  }
  if (Serial.available())
  {
    ioTime = currTime;
    bluetooth.print((char)Serial.read());
  }
}
