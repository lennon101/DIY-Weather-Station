/*
  Basic MQTT example

  This sketch demonstrates the basic capabilities of the library.
  It connects to an MQTT server then:
  - publishes "hello world" to the topic "esp32/out"
  - subscribes to the topic "esp32/in", printing out any messages 
    it receives. NB - it assumes the received payloads are strings not binary

  It will reconnect to the server if the connection is lost using a blocking
  reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
  achieve the same result without blocking the main loop.

  More info: https://pubsubclient.knolleary.net/api#connect

*/

#include <WiFi.h>
#include <PubSubClient.h>

// WIFI contants 
// Replace the next variables with your SSID/Password combination
const char* ssid = "equip";
const char* password = "Yoloforever";

// Note: don't use https as this script is not set up to use SSL encryption 
const char* mqtt_url = "http://";
const char* node_id = "esp32Client";  // the id/name the mqtt sees when the client connects 

// if your mqtt server configuration is set to:  
// allow_anonymous = false
// then you'll need to include the username and password below
// Otherwise, leave them blank. 
const char* mqtt_username = "username";
const char* mqtt_pw = "password";

// Callback function for the mqtt server 
void callback(char* topic, byte* payload, unsigned int length);

WiFiClient espClient;
PubSubClient client(mqtt_url, 1883, callback, espClient);

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  // publish and ACK back to the mqtt server 
  client.publish("esp32/out", "This is an ACK");
}


void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void connect_to_mqtt() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    //if (client.connect(node_id, mqtt_username, mqtt_pw)) {  // use this for username/pw connection
    if (client.connect(node_id)) {  // use this if allow_anonymous = True in your mqtt settings 
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/in");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  setup_wifi();
}

void loop()
{
  if (!client.connected()) {
    connect_to_mqtt();
  }
  client.loop();
  client.publish("esp32/out", "hello world");
  delay(2000);
}
