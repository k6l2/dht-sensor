#include <DHT.h>
#include <SoftwareSerial.h>
#define SENSOR_ID        0
#define PIN_DHT          2
#define PIN_BLUETOOTH_TX 4
#define PIN_BLUETOOTH_RX 7
#define ACK_STRING_LENGTH 3
#define DHTTYPE DHT22
const char ACK_STRING[ACK_STRING_LENGTH + 1] = "ACK";
unsigned int currStringIndexAck = 0;
DHT dht(PIN_DHT, DHTTYPE);
SoftwareSerial bluetooth(PIN_BLUETOOTH_TX, PIN_BLUETOOTH_RX);
unsigned long ioTime = 0;
// So for WHATEVER bullshit reason, the bluetooth module I got doesn't actually enter
//  the power-save mode called Sniff Mode unless I keep the stupid fucking thing in
//  Command-Mode for one full cycle of waiting for the next data packet to send,
//  so this bool is to let us know if we have passed an ACK->Data cycle entirely
//  in Command-Mode since power-on.
int fixedBullshitState;
// this is to help us determine if we've connected and started the first data cycle
//  so we can fix the bullshit state (see above)
bool receivedFirstAck;
bool isBullshitStateFixed()
{
  return fixedBullshitState >= 1;
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
  fixedBullshitState = 0;
  receivedFirstAck   = false;
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
  //bluetooth.println("SI,0100"); //default
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
  bluetooth.println("SW,12C0");// 625*0x12C0 = 3,000,000μ = 3seconds (SUPPOSEDLY)
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
//  // enter command mode on the modem
//  bluetooth.print("$$$");
//  delay(100);// wait for "CMD" response
//  delay(10000);// WHY?...
//  // leave command mode on the modem
//  bluetooth.print("---\n");
//  delay(100);// wait for "END" response
}
void loop()
{
  const unsigned long currTime = millis();
  ///TODO: handle the edge case where currTime <= ioTime when
  /// the unsigned long loops after ~50 days or so
  if (currTime - ioTime > 10000)
  {
//    // enter data mode/reconnect to the bluetooth
//    //  -Leave quiet mode using "Q,0", wait a little bit for "AOK" response
//    bluetooth.println("Q,0");
//    delay(100);// wait for "AOK" response

//    // lower the baud of the modem as well as the serial interface
//    //  this command also takes the modem out of command mode
//    bluetooth.println("U,9600,N");
//    delay(100);// wait for "AOK" response
//    bluetooth.begin(9600);
    if(!isBullshitStateFixed() && receivedFirstAck)
    {
//      // Exit command mode
//      bluetooth.print("\n---\n");
//      delay(100);// wait for "END" response
      fixedBullshitState++;
    }
//    // wait for the modem to reconnect
//    delay(3000);
    const float humidity    = dht.readHumidity();
    const float temperature = dht.readTemperature();
    const float voltage     = vccRead()/1000.f;
    if (isnan(humidity)    || 
        isnan(temperature) || 
        isnan(voltage))
    {
      Serial.println("Corrupted data!");
      return;
    }
    Serial.println("Humidity= " + String(humidity   , 1) + "%");
    Serial.println("Temp    = " + String(temperature, 1) + "C");
    Serial.println("Voltage = " + String(voltage    , 3) + "V");
    bluetooth.print(           "H="   +
      String(humidity   , 1) + "%,T=" +
      String(temperature, 1) + "C,V=" +
      String(voltage    , 3) + "U");
    // wait some time for the data to be transmitted, hopefully
    //  by the time this delay finishes our bluetooth serial
    //  connection has an acknowledgement in the buffer.. ~
    //delay(3000);
    ioTime = currTime;
  }
  if (bluetooth.available())
  {
    const char nextBtChar = (char)bluetooth.read();
    const int compareToAckStr = serialStringCompare(
      ACK_STRING, ACK_STRING_LENGTH, nextBtChar, &currStringIndexAck);
    if(compareToAckStr != 0)
    {
      Serial.print("[");
      Serial.print(nextBtChar);
      Serial.print("]");
      if(compareToAckStr == 2)
      {
        // At this point, we have read the entire ack string
        //  from the bluetooth connection, so we know that a
        //  complete data string was sent over the connection //
        //  -enter command mode on the bluetooth
        // set the bluetooth serial line to the modem's default baud
        Serial.println("Got Ack!");
        delay(15000);
//        bluetooth.begin(115200);
//        if(!receivedFirstAck)
//        {
//          delay(10000);// WHY?...
//        }
        receivedFirstAck = true;
        if(!isBullshitStateFixed())
        {
//          // enter command mode on the modem
//          bluetooth.print("$$$");
//          delay(100);// wait for "CMD" response
          // Okay so, after much debugging, I have figured out how to force this
          //  piece of shit bluetooth adapter to enter into a powersave mode
          //  (SNIFF MODE), by delaying things right here and not transferring any
          //  data for like 20 seconds once a connection has been established,
          //  verified with an acknowledgement packet from the server
//          delay(20000);// WHY?...
        }
//        //  -put the bluetooth modem into sleep mode?
//        //    command="Q,1", wait for a "AOK" response
//        //  in Q,1 mode, the modem cannot connect OR be discovered
//        bluetooth.println("Q,1");
//        delay(100);// wait for "AOK" response
//        //  -kill the bluetooth connection (maybe not needed w/ quiet mode?)
//        //    command="K,", wait a little bit for the "KILL<cr><lf>" response
//        bluetooth.println("K,");
//        delay(100);// wait for "KILL<cr><lf>" response
      }
    }
    else
    {
      ioTime = currTime;
      Serial.print(nextBtChar);
    }
  }
//  if (Serial.available())
//  {
//    ioTime = currTime;
//    bluetooth.print((char)Serial.read());
//  }
}
