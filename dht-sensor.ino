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
bool serialStringCompare(const char* const str, unsigned strSize,
  char nextChar, unsigned int* currStrIndex)
{
  if(*currStrIndex < strSize &&
    str[*currStrIndex] == nextChar)
  {
    (*currStrIndex)++;
    if(*currStrIndex >= strSize)
    {
      *currStrIndex = 0;
      return true;
    }
    // do not reset currStrIndex because we are still reading
    //  a valid string match up to this point!
    return false;
  }
  *currStrIndex = 0;
  return false;
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
  bluetooth.println("SI,0100"); //default
  //bluetooth.println("SI,0012");
  delay(100);// wait for "AOK" response
  // Adjust the Page Scan Window to the lowest possible value
  //  (uses less power, delays connection time)
  //bluetooth.println("SJ,0012");
  bluetooth.println("SJ,0100"); //default
  delay(100);// wait for "AOK" response
  /* Apparently this doesn't actually work because MASTER has to agree...
  // Enable Sniff Mode to put the radio to sleep every X milliseconds
  bluetooth.println("SW,12C0");
  delay(100);// wait for "AOK" response
  */
  // Lower the transmission power to lowest level
  //  (uses less power, decreases transmission range)
  bluetooth.println("SY,0010"); //default
  //bluetooth.println("SY,FFF4");
  delay(100);// wait for "AOK" response
  // lower the baud of the modem as well as the serial interface
  //  this command also takes the modem out of command mode
  bluetooth.println("U,9600,N");
  bluetooth.begin(9600);
}
void loop()
{
  const unsigned long currTime = millis();
  ///TODO: handle the edge case where currTime <= ioTime when
  /// the unsigned long loops after ~50 days or so
  if (currTime - ioTime > 10000)
  {
    /*
    // enter data mode/reconnect to the bluetooth
    //  -Leave quiet mode using "Q,0", wait a little bit for "AOK" response
    bluetooth.println("Q,0");
    delay(100);// wait for "AOK" response
    // lower the baud of the modem as well as the serial interface
    //  this command also takes the modem out of command mode
    bluetooth.println("U,9600,N");
    bluetooth.begin(9600);
    // wait for the modem to reconnect
    delay(3000);
    */
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
    delay(3000);
  }
  if (bluetooth.available())
  {
    const char nextBtChar = (char)bluetooth.read();
    /*
    if(serialStringCompare(ACK_STRING, ACK_STRING_LENGTH,
      nextBtChar, &currStringIndexAck))
    {
      // At this point, we have read the entire ack string
      //  from the bluetooth connection, so we know that a
      //  complete data string was sent over the connection //
      //  -enter command mode on the bluetooth
      // set the bluetooth serial line to the modem's default baud
      bluetooth.begin(115200);
      // enter command mode on the modem
      bluetooth.print("$$$");
      delay(100);// wait for "CMD" response
      //  -kill the bluetooth connection (maybe not needed w/ quiet mode?)
      //    command="K,", wait a little bit for the "KILL<cr><lf>" response
      bluetooth.println("K,");
      delay(100);// wait for "KILL<cr><lf>" response
      //  -put the bluetooth modem into sleep mode?
      //    command="Q,1", wait for a "AOK" response
      //  in Q,1 mode, the modem cannot connect OR be discovered
      bluetooth.println("Q,1");
      delay(100);// wait for "AOK" response
    }
    */
    ioTime = currTime;
    Serial.print(nextBtChar);
  }
  if (Serial.available())
  {
    ioTime = currTime;
    bluetooth.print((char)Serial.read());
  }
}
