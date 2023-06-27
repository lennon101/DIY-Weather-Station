/*
  ESP32_WeatherStation
  Auther: Dane Lennon
  Date: 2022-11-22

  Description:
    Connects to wifi and and an MQTT server as a client. Subscribes to a topic and waits
    for the callback function of this topic to be fired. Inside the callback function,
    Weather Station data is collected and published to a second MQTT topic.

*/

#include <WiFi.h>
#include <PubSubClient.h>

// BME280 includes
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// include library to read and write from flash memory
#include <EEPROM.h>

#define NUM_BOOTS_SIZE 4
#define BOOT_REASON_SIZE 20
#define EEPROM_SIZE NUM_BOOTS_SIZE + BOOT_REASON_SIZE
#define EEPROM_ADDR 0  //EEPROM address to start reading from
struct BootData {
  uint32_t num_boots;             // 4 bytes
  char reason[BOOT_REASON_SIZE];  // 20 bytes
};                                // total: 14 bytes

BootData boot_data;
Adafruit_BME280 bme;  // I2C

// Davis rain gauge variables
const int rain_gauge_pin = 16;  // pin used for the tipping bucket rain gauge
int tips = 0;                   // the number of tips

// variables needed to de-bounce the rain gauge
int tip_state;                       // the current reading from the input pin
int last_tip_state = LOW;            // the previous reading from the input pin
unsigned long last_tip = 0;          // the last time the rain guage was tipped
unsigned long debounce_delay = 500;  // the debounce time; increase if the output flickers

// Replace the next variables with your SSID/Password combination
const char* ssid = "The Retreat";
const char* password = "DandE25042016";
const char* mqtt_server = "192.168.1.22";
//  const char* ssid = "equip";
//  const char* password = "Yoloforever";
// const char* mqtt_server = "mqtt.thelennons.synology.me";
IPAddress local_IP(192, 168, 1, 40);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// Callback function header
void callback(char* topic, byte* payload, unsigned int length);

WiFiClient espClient;
PubSubClient client(mqtt_server, 1883, callback, espClient);

/*
   Clear the EEPROM and reset all memory bytes back to 0
*/
void reset_eeprom() {
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0);
  }
}

bool out_of_range(float value, float min, float max) {
  if (value < min) {
    return true;
  }
  if (value > max) {
    return true;
  }
  return false;
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  float temp, pressure, rh;
  int attempts_left = 5;
  get_bme_values(temp, pressure, rh);

  while (out_of_range(temp, 0, 50) || out_of_range(pressure, 800, 1200) || out_of_range(rh, 0, 100)) {
    get_bme_values(temp, pressure, rh);
    if (attempts_left-- <= 0) {
      Serial.println("BME280 not reporting valid values, rebooting esp32 now.");
      reboot("BME280 values");
    }
  }
  float mm_rain = 0.2 * tips;
  tips = 0;  // reset the accumlator of tips to 0

  String json_package = "{\"temperature\":" + String(temp) + ",\"pressure\":" + String(pressure) + ",\"humidity\":" + String(rh) + ",\"mm_rain\":" + String(mm_rain) + "}";
  const char* to_pub = json_package.c_str();
  Serial.println(to_pub);
  client.publish("esp32/out", to_pub);
}

// this code fires each time a tip occurs in the tipping bucket rain gauge
// The IRAM_ATTR identifier is recommended by Espressif in order to place
// this piece of code in the internal RAM memory instead of the flash.
void IRAM_ATTR Ext_INT1_ISR() {
  if ((millis() - last_tip) > debounce_delay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:
    tips += 1;            // add one tip
    last_tip = millis();  // reset the debouncing timer
    Serial.print("Tips: ");
    Serial.println(tips);
  }
}

void setup_bme280() {
  Serial.println(F("Setup BME280"));

  bool status;

  // default settings
  int attempts_left = 10;
  while (attempts_left >= 0) {
    Serial.print("Attempts left: ");
    Serial.print(attempts_left--);
    Serial.print(":");
    status = bme.begin(0x77);
    if (!status) {
      Serial.println("Could not find a valid BME280 sensor, check wiring!");
      delay(1000);
    } else {
      Serial.println("Found a BME280.");
      break;
    }
  }
  if (attempts_left <= 0) {
    Serial.println("Could not find a BME280, rebooting esp32 now.");
    reboot("BME280 failed");
  }

  pinMode(rain_gauge_pin, INPUT);
  attachInterrupt(rain_gauge_pin, Ext_INT1_ISR, RISING);

  Serial.println();
}

void get_bme_values(float& temp, float& pressure, float& rh) {
  temp = bme.readTemperature();
  Serial.println("Temp: " + String(temp));
  pressure = bme.readPressure() / 100.0F;
  Serial.println("pressure: " + String(pressure));
  rh = bme.readHumidity();
  Serial.println("rh: " + String(rh));
  Serial.print("Temperature = ");
  Serial.print(temp);
  Serial.println(" *C");

  Serial.print("Pressure = ");
  Serial.print(pressure);
  Serial.println(" hPa");

  Serial.print("Humidity = ");
  Serial.print(rh);
  Serial.println(" %");

  Serial.println();
}


void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  delay(1500);  // allow time for wifi to startup
  int attempts_left = 200;
  int num_dots_per_line = 60;
  Serial.println("Trying to reconnect to WiFi network...");
  while (WiFi.status() != WL_CONNECTED && --attempts_left >= 0) {
    delay(100);
    Serial.print(".");
    if (--num_dots_per_line <= 0) {
      Serial.println();
      num_dots_per_line = 60;
    }
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wifi not able to connect, rebooting device.");
    reboot("wifi failed");
  }

  if (WiFi.getSleep() == true) {
    WiFi.setSleep(false);
    Serial.println("WiFi Sleep is now deactivated.");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void connect_to_mqtt() {
  client.setKeepAlive(60);
  int client_state = client.state();
  // Loop until we're reconnected
  int attempts_left = 5;
  Serial.print("Attempting MQTT connection. Current client state: ");
  Serial.println(String(client_state));
  while (!client.connected() && --attempts_left >= 0) {
    // Attempt to connect
    if (client.connect("esp32Client", "dane-lennon", "aj3eu9Vjb3")) {
      Serial.println("\tMQTT connected");
      // Subscribe
      client.subscribe("esp32/in");
      // no need to break because client.connected() will now return true
    } else {
      Serial.print("\tfailed, rc=");
      Serial.print(client.state());

      Serial.print(" trying again in 5 seconds. ");
      Serial.print("Attempts left: ");
      Serial.println(attempts_left);
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
  if (attempts_left <= 0) {
    int client_state = client.state();
    Serial.println("MQTT not able to connect, rebooting esp32 now.");
    reboot("MQTT: " + String(client_state));
  }

  // if we successfully reconnected to mqtt, post a debug message
  String out_msg = "MQTT reconnected. Boot num:" + String(boot_data.num_boots) + ", Reboot reason:" + String(boot_data.reason);
  const char* to_pub = out_msg.c_str();
  client.publish("esp32/out/debug", to_pub);
}

void update_boot_data(String reboot_reason) {
  boot_data.num_boots++;                                          // increment the boot count
  reboot_reason.toCharArray(boot_data.reason, BOOT_REASON_SIZE);  // Copy the reason into the struct

  EEPROM.put(EEPROM_ADDR, boot_data);  // save the struct into eeprom
  EEPROM.commit();                     // commit the data to eeprom
  Serial.println("Written: " + String(boot_data.num_boots) + ", " + String(boot_data.reason));
}

void setup_eeprom() {
  EEPROM.begin(EEPROM_SIZE);  // initialize EEPROM with predefined size
  // reset_eeprom(); // uncomment to clear the eeprom memory and reset all bytes back to 0
  EEPROM.get(EEPROM_ADDR, boot_data);  // read the last state from flash memory
  Serial.println("Boot data read after reboot: " + String(boot_data.num_boots) + ", " + String(boot_data.reason));
}

void reboot(String reboot_reason) {
  update_boot_data(reboot_reason);
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  setup_eeprom();  // Note: do this step first because it reads the boot_data out of eeprom and into a global variable
  setup_wifi();
  setup_bme280();
  
  float temp, pressure, rh;
  get_bme_values(temp, pressure, rh);
  Serial.println("Commencing main loop.");
}


void loop() {
  if (WiFi.status() == WL_CONNECTED) {  //Connected to WiFi
    if (!client.connected()) {
      connect_to_mqtt();
    }
    client.loop();  // should be called on a regular basis,  in order to allow the client to process incoming messages and maintain the connection to the MQTT server.
  } else {          //WiFi not connected, try to reconnect
    setup_wifi();
  }
  // Serial.println("Delaying for 1min before checking wifi status again.")
  // delay(60000);  // delay for 1min before checking wifi status again
}
