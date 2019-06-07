#include <Wire.h>  // for I2C communications
#include <Adafruit_GFX.h> // for oled display
#include <Adafruit_SSD1306.h> // for oled display
#include <PubSubClient.h> //pubsub library for mqtt integration
#include <ESP8266WiFi.h> //library for wifi integration
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h> //json library integration for working with json

#include <Fonts/FreeSerif9pt7b.h>

//wifi and pubsub setup
#define WIFI_SSID "CYB"
#define WIFI_PASS "m1ck3yM0us3"
WiFiClient espClient;
PubSubClient mqtt(espClient);
char mac[6]; //unique id

// adafruit io setup
#define IO_USERNAME   "crysis"
#define IO_KEY        "f64f7594037b4ffeb4e1716527fa5114"
#include "AdafruitIO_WiFi.h"
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

unsigned long currentTimer, timer, currentApiTimer, apiTimer;

// api keys
const char* key = "b4038b91c68deb4bdec045124c6df669";
const char* weatherkey = "73b43e4a3b95d2a36b9d9551fa564d82";

//mqtt server and login credentials
#define mqtt_server "mediatedspaces.net"  //this is its address, unique to the server
#define mqtt_user "hcdeiot"               //this is its server login, unique to the server
#define mqtt_password "esp8266"           //this is it server password, unique to the server

// initializing variables
String areaTemp;
String sensorTemp;
String pressureSensor;
String humiditySens;
String data;
String pastData;
float avgTemp;
String weeklyco2;
String historicco2;

// set up the 'temperature', 'humidity', 'pressure', and two co2 feeds
AdafruitIO_Feed *temperature = io.feed("sensorTemp");
AdafruitIO_Feed *weekly = io.feed("weeklyco2");
AdafruitIO_Feed *historic = io.feed("historicco2");
AdafruitIO_Feed *pressure = io.feed("pressure");
AdafruitIO_Feed *humidity = io.feed("humidity");


// create OLED Display instance on an ESP8266
// set OLED_RESET to pin -1 (or another), because we are using default I2C pins D4/D5.
#define OLED_RESET -1

Adafruit_SSD1306 display(OLED_RESET); //creating an ssd1306 instance named display

void setup() {
  // start the serial connection
  Serial.begin(115200);
  Serial.print("This board is running: ");
  Serial.println(F(__FILE__));
  Serial.print("Compiled: ");
  Serial.println(F(__DATE__ " " __TIME__));

  // set up the OLED display
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  display.clearDisplay();
  display.setTextSize(0);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Booting up...");
  display.display();
  // init done

  display.println("Starting WiFi & MQTT");
  display.display();
  setup_wifi(); //start wifi
  mqtt.setServer(mqtt_server, 1883); //start mqtt server
  mqtt.setCallback(callback); //register the callback function
  timer = apiTimer = millis(); //set timers

  // connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();
  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  // we are connected
  Serial.println();
  Serial.println(io.statusText());
  
  // wait for serial monitor to open
  while (! Serial);

  // make initial API call
  areaTemp = getMet();

  // creating the database of climate data. (50 years ago, 1969-1970)
  data = "{\"1969\":{\"january\": 42.08, \"february\": 51.70, \"march\": 55.15, \"april\": 56.73, \"may\": 66.71, \"june\": 74.98, \"july\": 75.81, \"august\": 74.02, \"september\": 68.04, \"october\": 60.32, \"november\": 53.33, \"december\": 46.85}}";
  DynamicJsonBuffer djb;
  JsonObject& db = djb.parseObject(data);
  pastData = db["1969"]["june"].as<String>();
  getCO2();
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");  //get the unique MAC address to use as MQTT client ID, a 'truly' unique ID.
  Serial.println(WiFi.macAddress());  //.macAddress returns a byte array 6 bytes representing the MAC address
  WiFi.macAddress().toCharArray(mac, 5);            //5C:CF:7F:F0:B0:C1 for example
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("connected");
      mqtt.subscribe("colinyb/+"); //subscribing to 'colinyb' and all subtopics below that topic
    } else {
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
  io.run();

  currentTimer = currentApiTimer = millis(); //updating the timers

  if (currentApiTimer - apiTimer > 1800000) { //a periodic call, every 30 minutes
    areaTemp = getMet();
    apiTimer = currentApiTimer;
  }

  Serial.println("Looping...");

  if (currentTimer - timer > 10000) { //a periodic update, every 10 seconds
    float avgTemp = (areaTemp.toFloat() + sensorTemp.toFloat())/2;
    //-------------UPDATE THE DISPLAY--------------//
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("AvgTemp  : ");
    display.print(avgTemp);
    display.println("'F");
    display.print("50YA High: ");
    display.print(pastData);
    display.println("'F");
    display.print("CO2(Week): ");
    display.print(weeklyco2);
    display.println("ppm");
    display.print("CO2(10yr): ");
    display.print(historicco2);
    display.println("ppm");
//    display.print("Humidity : ");
//    display.print(sensorHumidity);
//    display.println("%");
//    display.print("Pressure : ");
//    display.print(sensorPressure);
//    display.println("kPa");
    display.display();
    
    timer = currentTimer;
    
    // save fahrenheit (or celsius) to Adafruit IO
    temperature->save(sensorTemp); //updating adafruit io
    weekly->save(weeklyco2); //updating adafruit io
    historic->save(historicco2); //updating adafruit io
    humidity->save(humiditySens);
    pressure->save(pressureSensor);

    Serial.println("ENTERED IF");
    Serial.println("ENTERED IF");
    Serial.println("ENTERED IF");
    Serial.println("ENTERED IF");
    Serial.println("ENTERED IF");
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(topic); //'topic' refers to the incoming topic name, the 1st argument of the callback function
  Serial.println("] ");

  DynamicJsonBuffer  jsonBuffer; //creating DJB instance named jsonBuffer
  JsonObject& root = jsonBuffer.parseObject(payload); //parse it!

  if (!root.success()) { // Serial fail message
    Serial.println("parseObject() failed, are you sure this message is JSON formatted.");
    return;
  }

  // assigning to variable and serial printout of temp for debugging
  sensorTemp = root["tempF"].as<String>();
  Serial.println(sensorTemp);

  // assigning to variable and serial printout of humidity for debugging
  humiditySens = root["humidity"].as<String>();
  Serial.println(humiditySens);

  // assigning to variable and serial printout of pressure for debugging
  pressureSensor = root["pressure"].as<String>();
  Serial.println(pressureSensor);
}

String getMet() {
  HTTPClient theClient; //creates an HTTPClient object named theClient
  String apistring = "http://api.openweathermap.org/data/2.5/weather?q=" + getGeo() + "&units=imperial&appid=" + weatherkey; //concatonating the api request url
  theClient.begin(apistring); //make the request
  int httpCode = theClient.GET(); //get the HTTP code (-1 is fail)

  if (httpCode > 0) { //test if the request failed
    if (httpCode == 200) { //if successful...
      DynamicJsonBuffer jsonBuffer; //create a DynamicJsonBuffer object named jsonBuffer
      String payload = theClient.getString(); //get the string of json data from the request and assign it to payload
      Serial.println("Parsing...");
      Serial.println(payload);
      JsonObject& root = jsonBuffer.parse(payload); //set the json data to the variable root
      
      if (!root.success()) { //check if the parsing worked correctly
        Serial.println("parseObject() failed");
        Serial.println(payload); //print what the json data is in a string form
        return("error");
      } // return the temperature
      return(root["main"]["temp"].as<String>());
    } else { //print error if the request wasnt successful
      Serial.println("Had an error connecting to the network.");
    }
  }
}

String getCO2() {
  HTTPClient theClient; //creates an HTTPClient object named theClient
  String apistring = "http://hqcasanova.com/co2/?callback=process"; //concatonating the api request url
  theClient.begin(apistring); //make the request
  int httpCode = theClient.GET(); //get the HTTP code (-1 is fail)

  if (httpCode > 0) { //test if the request failed
    if (httpCode == 200) { //if successful...
      DynamicJsonBuffer jsonBuffer; //create a DynamicJsonBuffer object named jsonBuffer
      String payload = theClient.getString(); //get the string of json data from the request and assign it to payload

      // turning it into correct json format
      payload.remove(0, 8);
      payload.remove(payload.indexOf(")"));
      
      Serial.println("Parsing...");
      Serial.println(payload);
      JsonObject& root = jsonBuffer.parse(payload); //set the json data to the variable root
      
      if (!root.success()) { //check if the parsing worked correctly
        Serial.println("parseObject() failed");
        Serial.println(payload); //print what the json data is in a string form
        return("error");
      } // return the temperature
      weeklyco2 = root["0"].as<String>();
      historicco2 = root["10"].as<String>();
    } else { //print error if the request wasnt successful
      Serial.println("Had an error connecting to the network.");
    }
  }
}

String getIP() {
  HTTPClient theClient;
  String ipAddress;

  theClient.begin("http://api.ipify.org/?format=json"); //Make the request
  int httpCode = theClient.GET(); //get the http code for the request

  if (httpCode > 0) {
    if (httpCode == 200) { //making sure the request was successful

      DynamicJsonBuffer jsonBuffer; // create a dynamicjsonbuffer object named jsonbuffer

      String payload = theClient.getString(); //get the data from the api call and assign it to the string object called payload
      JsonObject& root = jsonBuffer.parse(payload); //create a jsonObject called root and use the jsonbuffer to parse the payload string to json accessible data
      ipAddress = root["ip"].as<String>();

    } else { //error message for unsuccessful request
      Serial.println("Something went wrong with connecting to the endpoint.");
      return "error";
    }
  }
  return ipAddress; //returning the ipAddress 
}

String getGeo() {
  HTTPClient theClient;
  Serial.println("Making HTTP request");
  theClient.begin("http://api.ipstack.com/" + getIP() + "?access_key=" + key); //return IP as .json object
  int httpCode = theClient.GET();

  if (httpCode > 0) {
    if (httpCode == 200) {
      Serial.println("Received HTTP payload.");
      DynamicJsonBuffer jsonBuffer;
      String payload = theClient.getString();
      Serial.println("Parsing...");
      JsonObject& root = jsonBuffer.parse(payload);

      // Test if parsing succeeds.
      if (!root.success()) {
        Serial.println("parseObject() failed");
        Serial.println(payload);
        return("error");
      }

      return(root["city"].as<String>()); //return the city name

    } else {
      Serial.println("Something went wrong with connecting to the endpoint.");
      return("error");
    }
  }
}
