#include <DHT.h>
#include <SoftwareSerial.h>
#define PIN_DHT          2
#define PIN_BLUETOOTH_TX 4
#define PIN_BLUETOOTH_RX 7
#define DHTTYPE DHT22
DHT dht(PIN_DHT, DHTTYPE);
SoftwareSerial bluetooth(PIN_BLUETOOTH_TX, PIN_BLUETOOTH_RX);
unsigned long ioTime = 0;
void setup()
{
  Serial.begin(9600);
  Serial.println("DHT22 sensor");
  dht.begin();
  bluetooth.begin(115200);
  bluetooth.print("$");
  bluetooth.print("$");
  bluetooth.print("$");
  delay(100);
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
    const float humidity    = dht.readHumidity();
    const float temperature = dht.readTemperature();
    const float voltage     = 0;
    ///TODO: calculate voltage
    if (isnan(humidity)    || 
        isnan(temperature) || 
        isnan(voltage))
    {
      Serial.println("Corrupted data!");
      return;
    }
    Serial.println("Humidity= " + String(humidity   , 1) + "%");
    Serial.println("Temp    = " + String(temperature, 1) + "C");
    Serial.println("Voltage = " + String(voltage    , 4) + "V");
    bluetooth.print(           "H="   +
      String(humidity   , 1) + "%,T=" +
      String(temperature, 1) + "C,V=" +
      String(voltage    , 4) + "U");
    delay(3000);
  }
  if (bluetooth.available())
  {
    ioTime = currTime;
    Serial.print((char)bluetooth.read());
  }
  if (Serial.available())
  {
    ioTime = currTime;
    bluetooth.print((char)Serial.read());
  }
}