/***************************************************************************
  This is a library for the BME280 humidity, temperature & pressure sensor

  Designed specifically to work with the Adafruit BME280 Breakout
  ----> http://www.adafruit.com/products/2650

  These sensors use I2C or SPI to communicate, 2 or 4 pins are required
  to interface. The device's I2C address is either 0x76 or 0x77.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit andopen-source hardware by purchasing products
  from Adafruit!

  Written by Limor Fried & Kevin Townsend for Adafruit Industries.
  BSD license, all text above must be included in any redistribution
  See the LICENSE file for details.
 ***************************************************************************/
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

SoftwareSerial softSerial(10, 11); // RX, TX

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme; // I2C
unsigned long delayTime;

void setup() {
  Serial.begin(115200);
  while (!Serial);   // time to get serial running
  Serial.println(F("BME280 test"));

  unsigned status;

  // default settings
  status = bme.begin();
  while (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
    Serial.print("SensorID was: 0x"); Serial.println(bme.sensorID(), 16);
    delay(1000);
    Serial.println("\nAttempting to revalidate the address.");
    status = bme.begin();
  }

  Serial.println("-- Default Test --");
  delayTime = 1000;
  Serial.println();
  softSerial.begin(115200);
}

void print_values(float bmp280_temp, float bme280_pressure, float bme280_rh)
{
  Serial.print("Temperature = ");
  Serial.print(bmp280_temp);
  Serial.println(" °C");

  Serial.print("Pressure = ");

  Serial.print(bme280_pressure);
  Serial.println(" hPa");

  Serial.print("Approx. Altitude = ");
  Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  Serial.println(" m");

  Serial.print("Humidity = ");
  Serial.print(bme280_rh);
  Serial.println(" %");

  Serial.println();
}

void get_values(float& temp, float& pressure, float& rh)
{
  temp = bme.readTemperature();
  pressure = bme.readPressure() / 100.0F;
  rh = bme.readHumidity();
}

void loop() {
  float bmp280_temp = 0;
  float bme280_pressure = 0;
  float bme280_rh = 0;

  get_values(bmp280_temp, bme280_pressure, bme280_rh);
  print_values(bmp280_temp, bme280_pressure, bme280_rh);

  char inChar;
  Serial.print("Blocking... ");
  while (! softSerial.available());

  // get the new byte:
  inChar = (char)softSerial.read();
  Serial.print("Got byte: ");
  Serial.print(inChar);
  Serial.print(", 0x");
  Serial.println(inChar, HEX);

  if (inChar == 'M')
  {
    String outString = String(bmp280_temp) + "," + String(bme280_pressure) + "," +
                       String(bme280_rh) + "\r\n";

    softSerial.print(outString);
  }
  delay(delayTime);
}
