#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>

// WiFi settings
#define WIFI_SSID "zxl"               // WiFi SSID (network name)
#define WIFI_PASSWORD "987654321"     // WiFi password

// MQTT settings
#define MQTT_SERVER "broker-cn.emqx.io"  // MQTT broker address
#define MQTT_PORT 1883                   // MQTT broker port
#define CLIENT_ID "50d5130975b4c93a4e8dddde521abcd"  // Unique client ID for MQTT
#define PUB_TOPIC "zxl/home/room/temp"   // MQTT topic for publishing sensor data
#define SUB_TOPIC "zxl/home/room/switch" // MQTT topic for subscribing to control messages

// OLED display settings
#define SCREEN_WIDTH 128   // OLED display width, in pixels
#define SCREEN_HEIGHT 64   // OLED display height, in pixels
#define OLED_RESET    -1   // Reset pin (not used in this setup)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);  // Initialize OLED display

// BME280 sensor settings
Adafruit_BME280 bme;  // Create an instance of BME280 sensor

// Smoke sensor and buzzer
#define SMOKE_PIN 34       // GPIO pin for the smoke sensor
#define BUZZER_PIN 15      // GPIO pin for the buzzer
int smoke_threshold = 300; // Threshold value for smoke detection, adjust based on your environment

// Fan control
#define FAN_PIN 2  // GPIO pin for controlling the fan

// WiFi and MQTT clients
WiFiClient espClient;  // WiFi client object
PubSubClient client(espClient);  // MQTT client object using the WiFi client

// Alibaba Cloud IoT settings
const char* productKey = "k1b21bN12Fq6q";  // Alibaba Cloud IoT product key
const char* deviceName = "IOT_LED1";       // Alibaba Cloud IoT device name
const char* deviceSecret = "8e42143c9670299f292613ec2b4269bf042";  // Alibaba Cloud IoT device secret

// Flag to indicate connection to the IoT platform
bool iot_connected = false;

void setup() {
  Serial.begin(115200);  // Start the serial communication at 115200 baud rate

  // Initialize WiFi connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);  // Start WiFi connection
  while (WiFi.status() != WL_CONNECTED) {  // Wait until connected
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");  // Print a message when connected

  // Initialize MQTT connection
  client.setServer(MQTT_SERVER, MQTT_PORT);  // Set MQTT broker server and port
  client.setCallback(mqttCallback);  // Set the callback function for MQTT messages
  
  // Initialize BME280 sensor
  if (!bme.begin(0x76)) {  // Check if BME280 sensor is detected at I2C address 0x76
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);  // Stop if the sensor is not found
  }

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Initialize OLED at I2C address 0x3C
    Serial.println(F("SSD1306 allocation failed"));
    while (1);  // Stop if the display is not found
  }
  display.clearDisplay();  // Clear the OLED display

  // Set pin modes for fan and buzzer
  pinMode(FAN_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Connect to Alibaba Cloud IoT platform
  connectToAliyun();  // Call the function to connect to Alibaba Cloud IoT
}

void loop() {
  // Reconnect to MQTT broker if connection is lost
  if (!client.connected()) {
    reconnect();
  }
  client.loop();  // Process incoming MQTT messages

  // Read sensor data from BME280
  float temperature = bme.readTemperature();  // Read temperature from BME280
  float humidity = bme.readHumidity();        // Read humidity from BME280
  float pressure = bme.readPressure() / 100.0F;  // Read pressure from BME280 and convert to hPa
  int smoke_level = analogRead(SMOKE_PIN);  // Read smoke level from the smoke sensor

  // Display temperature, humidity, and pressure data on the OLED
  display.clearDisplay();  // Clear the OLED display buffer
  display.setTextSize(1);  // Set text size
  display.setTextColor(SSD1306_WHITE);  // Set text color
  display.setCursor(0, 0);  // Set cursor position
  display.print("Temp: ");  // Print "Temp: " label
  display.print(temperature);  // Print the temperature value
  display.print(" C");  // Print the unit
  display.setCursor(0, 10);  // Move cursor to the next line
  display.print("Humi: ");  // Print "Humi: " label
  display.print(humidity);  // Print the humidity value
  display.print(" %");  // Print the unit
  display.setCursor(0, 20);  // Move cursor to the next line
  display.print("Pres: ");  // Print "Pres: " label
  display.print(pressure);  // Print the pressure value
  display.print(" hPa");  // Print the unit
  display.setCursor(0, 30);  // Move cursor to the next line
  display.print("Smoke: ");  // Print "Smoke: " label
  display.print(smoke_level);  // Print the smoke level value
  display.display();  // Display all the printed values on the OLED

  // Check smoke level and activate buzzer if necessary
  if (smoke_level > smoke_threshold) {
    digitalWrite(BUZZER_PIN, HIGH);  // Turn on the buzzer if smoke level exceeds the threshold
    Serial.println("Smoke detected! Activating buzzer!");
  } else {
    digitalWrite(BUZZER_PIN, LOW);  // Turn off the buzzer otherwise
  }

  // Control fan based on temperature
  if (temperature >= 28) {
    digitalWrite(FAN_PIN, LOW); // Turn on the fan if the temperature is 28°C or higher
  } else {
    digitalWrite(FAN_PIN, HIGH); // Turn off the fan if the temperature is lower than 28°C
  }

  // Publish data to MQTT broker and Alibaba Cloud IoT platform
  publishDataToMQTT(temperature, humidity, pressure);  // Publish data to the MQTT topic
  publishDataToAliyun(temperature, humidity, pressure);  // Publish data to Alibaba Cloud IoT

  delay(10000);  // Wait for 10 seconds before reading and publishing data again
}

// Function to reconnect to MQTT broker
void reconnect() {
  while (!client.connected()) {  // Loop until reconnected
    Serial.print("Attempting MQTT connection...");
    if (client.connect(CLIENT_ID)) {  // Try to connect with the client ID
      Serial.println("connected");
      client.subscribe(SUB_TOPIC);  // Subscribe to the control topic
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());  // Print the reason for the failed connection
      Serial.println(" try again in 5 seconds");
      delay(5000);  // Wait 5 seconds before retrying
    }
  }
}

// MQTT callback function to handle incoming messages
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];  // Convert payload to string
  }
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);  // Print the topic
  Serial.print("Message: ");
  Serial.println(message);  // Print the message

  // Control fan based on the received message
  if (String(topic) == SUB_TOPIC) {
    if (message == "ON") {
      digitalWrite(FAN_PIN, LOW);  // Turn on the fan if the message is "ON"
    } else if (message == "OFF") {
      digitalWrite(FAN_PIN, HIGH);  // Turn off the fan if the message is "OFF"
    }
  }
}

// Function to connect to Alibaba Cloud IoT platform
void connectToAliyun() {
  // Here, you would implement the connection to Alibaba Cloud IoT platform
  // using the provided productKey, deviceName, and deviceSecret.
  // This typically involves using a library or SDK provided by Alibaba Cloud.

  Serial.println("Connecting to Alibaba Cloud IoT...");
  // Simulate connection delay
  delay(2000);
  iot_connected = true;  // Set the flag to indicate successful connection
  Serial.println("Connected to Alibaba Cloud IoT.");
}

// Function to publish sensor data to the MQTT broker
void publishDataToMQTT(float temperature, float humidity, float pressure) {
  char tempStr[8], humStr[8], presStr[8];
  dtostrf(temperature, 1, 2, tempStr);  // Convert temperature to string
  dtostrf(humidity, 1, 2, humStr);  // Convert humidity to string
  dtostrf(pressure, 1, 2, presStr);  // Convert pressure to string
  
  String payload = String("{\"temperature\":") + tempStr + ",\"humidity\":" + humStr + ",\"pressure\":" + presStr + "}";
  client.publish(PUB_TOPIC, payload.c_str());  // Publish the JSON payload to the MQTT topic
}

// Function to publish sensor data to Alibaba Cloud IoT platform
void publishDataToAliyun(float temperature, float humidity, float pressure) {
  // Here you would implement the code to send data to Alibaba Cloud IoT platform
  // using the established connection.

  if (iot_connected) {
    Serial.print("Publishing to Alibaba Cloud IoT: Temp = ");
    Serial.print(temperature);
    Serial.print(", Humi = ");
    Serial.print(humidity);
    Serial.print(", Pres = ");
    Serial.println(pressure);
    
    // Simulate a delay for publishing
    delay(500);
  } else {
    Serial.println("Failed to publish to Alibaba Cloud IoT: Not connected.");
  }
}