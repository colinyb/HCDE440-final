#include <Wire.h>  // for I2C communications
#include <PubSubClient.h> //pubsub library for mqtt integration
#include <ESP8266WiFi.h> //library for wifi integration
#include <ArduinoJson.h> //json library integration for working with json

#include <Adafruit_MPL115A2.h> // Barometric pressure sensor library
#include <Adafruit_Sensor.h>  // the generic Adafruit sensor library used with both sensors
#include <DHT.h>   // temperature and humidity sensor library
#include <DHT_U.h> // unified DHT library

//wifi and pubsub setup
#define wifi_ssid "University of Washington"
#define wifi_password ""
WiFiClient espClient;
PubSubClient mqtt(espClient);

//mqtt server and login credentials
#define mqtt_server "mediatedspaces.net"  //this is its address, unique to the server
#define mqtt_user ""               //this is its server login, unique to the server
#define mqtt_password ""           //this is it server password, unique to the server
char mac[6]; //unique id
char message[201]; //setting message length

// pin connected to DH22 data line
#define DATA_PIN 12

// create DHT22 instance
DHT_Unified dht(DATA_PIN, DHT22);

// create MPL115A2 instance
Adafruit_MPL115A2 mpl115a2;

//creating a timer and a counter
unsigned long currentTimer, timer; //, currentApiTimer, apiTimer

// pin variables for LEDs
const int tempindicator = 13;
const int humiindicator = 14;
const int presindicator = 15;
const int statusindicator = 16;

//// variables for switching LEDs on or off
//int warningStatus = LOW;
//int requestStatus = LOW;

void setup() {
  // start the serial connection
  Serial.begin(115200);
  Serial.print("This board is running: ");
  Serial.println(F(__FILE__));
  Serial.print("Compiled: ");
  Serial.println(F(__DATE__ " " __TIME__));

  // setting up the leds
  pinMode(tempindicator, OUTPUT);
  pinMode(humiindicator, OUTPUT);
  pinMode(presindicator, OUTPUT);
  pinMode(statusindicator, OUTPUT);

  setup_wifi(); //start wifi
  mqtt.setServer(mqtt_server, 1883); //start mqtt server
  mqtt.setCallback(callback); //register the callback function

  // wait for serial monitor to open
  while (! Serial);

  Serial.println("Sensor Platform Started");

  // initialize dht22
  dht.begin();

  // initialize MPL115A2
  mpl115a2.begin();
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");  //get the unique MAC address to use as MQTT client ID, a 'truly' unique ID.
  Serial.println(WiFi.macAddress());  //.macAddress returns a byte array 6 bytes representing the MAC address
  WiFi.macAddress().toCharArray(mac, 4);            //5C:CF:7F:F0:B0:C1 for example
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("connected");
      mqtt.subscribe("colinyb/sensorplatform"); //subscribing to 'colinyb/sensorplatform'
    } else { // print the state of the mqtt connection
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  if (!mqtt.connected()) { //handles reconnecting and initial connect
    reconnect();
  }
  mqtt.loop(); //this keeps the mqtt connection 'active'

  // signifying that the device is operating
  digitalWrite(statusindicator, HIGH);
  delay(250);
  digitalWrite(statusindicator, LOW);

  currentTimer = millis();
  
  if (currentTimer - timer > 10000) { //a periodic report, every 10 seconds
    //-------------GET THE TEMPERATURE--------------//
    // the Adafruit_Sensor library provides a way of getting 'events' from sensors
    //getEvent returns data from the sensor
    sensors_event_t event; //creating sensor_event_t instance named event
    dht.temperature().getEvent(&event); //reading temperature
  
    float celsius = event.temperature; //assigning temperature to celsius
    float fahrenheit = (celsius * 1.8) + 32; //calculating fahrenheit

    digitalWrite(tempindicator, HIGH);
    delay(250);
    digitalWrite(tempindicator, LOW);
  
    Serial.print("Celsius: ");
    Serial.print(celsius);
    Serial.println("C");
  
    Serial.print("Fahrenheit: ");
    Serial.print(fahrenheit);
    Serial.println("F");
  
  
    //-------------GET THE HUMIDITY--------------//
    dht.humidity().getEvent(&event); //reading humidity
    float humidity = event.relative_humidity; //assigning humidity to humidity
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println("%");

    digitalWrite(humiindicator, HIGH);
    delay(250);
    digitalWrite(humiindicator, LOW);
  
    //-------------GET THE PRESSURE--------------//
    // The Adafruit_Sensor library doesn't support the MPL1152, so we'll just grab data from it
    // with methods provided by its library
  
    float pressureKPA = 0; //creating a variable for pressure
  
    pressureKPA = mpl115a2.getPressure(); //reading pressure
    Serial.print("Pressure (kPa): "); 
    Serial.print(pressureKPA, 4); 
    Serial.println(" kPa");

    digitalWrite(presindicator, HIGH);
    delay(250);
    digitalWrite(presindicator, LOW);

    //creating variables to store the data
    char str_tempf[6];
    char str_etemp[6];
    char str_humd[6];
    char str_press[6];

    //converting to char arrays
//    externalTemp.toCharArray(str_etemp, 6);
    dtostrf(fahrenheit, 4, 2, str_tempf);
    dtostrf(humidity, 4, 2, str_humd);
    dtostrf(pressureKPA, 4, 2, str_press);
    // building message
    sprintf(message, "{\"tempF\": %s, \"humidity\": %s, \"pressure\": %s}", str_tempf, str_humd, str_press);
    mqtt.publish("colinyb/sensorplatform", message); //publishing to mqtt
    Serial.println("publishing");
    timer = currentTimer; //resetting timer
  }
  delay(5000);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(topic); //'topic' refers to the incoming topic name, the 1st argument of the callback function
  Serial.println("]");

  // flashing onboard LED to show that data is incoming
//  digitalWrite(LED_BUILTIN, LOW);
//  delay(500);
//  digitalWrite(LED_BUILTIN, HIGH);

  DynamicJsonBuffer  jsonBuffer; //creating DJB instance named jsonBuffer
  JsonObject& root = jsonBuffer.parseObject(payload); //parse it!

  if (!root.success()) { // Serial fail message
    Serial.println("parseObject() failed, are you sure this message is JSON formatted.");
    return;
  }
}
