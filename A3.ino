#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MPL115A2.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//A3

#include <ESP8266WiFi.h>    //Requisite Libraries . . .
#include "Wire.h"           //
#include <PubSubClient.h>   //
#include <ArduinoJson.h>    //

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define DHTPIN 2 //digital pin connected to the DHT sensor
#define DHTTYPE    DHT22     // DHT 22 (AM2302)
DHT_Unified dht(DHTPIN, DHTTYPE);

//start MPL sensor
Adafruit_MPL115A2 mpl115a2;

#define wifi_ssid "University of Washington"   //You have seen this before
#define wifi_password "" //

#define mqtt_server "mediatedspaces.net"  //this is its address, unique to the server
#define mqtt_user "hcdeiot"               //this is its server login, unique to the server
#define mqtt_password "esp8266"           //this is it server password, unique to the server

WiFiClient espClient;             //blah blah blah, espClient
PubSubClient mqtt(espClient);     //blah blah blah, tie PubSub (mqtt) client to WiFi client

char mac[6]; //A MAC address is a 'truly' unique ID for each device, lets use that as our 'truly' unique user ID!!!
char message[201]; //201, as last character in the array is the NULL character, denoting the end of the array
unsigned long currentMillis, timerOne, timerTwo, timerThree; //we are using these to hold the values of our timers

const int buttonPin = 12;     // the number of the pushbutton pin
int buttonState = 0;         // variable for reading the pushbutton status

/////SETUP_WIFI/////
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
}                                     //5C:CF:7F:F0:B0:C1 for example

/////SETUP/////
void setup() {
  Serial.begin(115200);
  setup_wifi();
  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback); //register the callback function
  timerOne = timerTwo = timerThree = millis();

  dht.begin();
  mpl115a2.begin();
  //initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT_PULLUP);
  
  //SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  delay(2000); // Pause for 2 seconds
  display.clearDisplay();
}

/////CONNECT/RECONNECT/////Monitor the connection to MQTT server, if down, reconnect
void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("connected");
      mqtt.subscribe("fromJon/+"); //we are subscribing to 'theTopic' and all subtopics below that topic
    } else {                        //please change 'theTopic' to reflect your topic you are subscribing to
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/////LOOP/////
void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }
  mqtt.loop(); //this keeps the mqtt connection 'active'
  
  //Here we just send a regular c-string which is not formatted JSON, or json-ified.
  if (millis() - timerOne > 10000) {
    //Here we would read a sensor, perhaps, storing the value in a temporary variable

    //STARTING HERE, ADD DHT/MPL SENSING AND SEND TO MQTT
    float presNum = 0, tempNum = 0;    
    presNum = mpl115a2.getPressure();
    tempNum = mpl115a2.getTemperature();
    sensors_event_t event;
    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
      Serial.println(F("Error reading humidity!"));
    }
    else {
      //CREATE THE MESSAGE TO SEND TO MQTT
      //turn numbers into strings
      char temp[10];
      char pres[10];
      char hum[10];
      dtostrf(presNum, 5, 2, pres);
      dtostrf(tempNum, 5, 2, temp);
      dtostrf(event.relative_humidity, 5, 2, hum);

      //send message
      sprintf(message, "{\"temp\":\"%s%\", \"pres\":\"%s%\", \"hum\":\"%s%\"}", temp, pres, hum); // %d is used for an int
      mqtt.publish("fromJon/words", message);

      //SOMETHING SENT TO SCREEN
      display.clearDisplay();
      display.print("T: ");
      display.println(temp);
      display.print("P: ");
      display.println(pres);
      display.print("H: ");
      display.println(hum);
    }
    buttonState = digitalRead(buttonPin);
    if (buttonState == LOW) {
      //String butt = "On";
      sprintf(message, "{\"button\":\"%s%\"}", "ON");
      mqtt.publish("fromJon/words", message);
    } else {
      sprintf(message, "{\"button\":\"%s%\"}", "OFF");
      mqtt.publish("fromJon/words", message);
    }
    
    timerOne = millis();
  }

  //Here we will deal with a JSON string
  if (millis() - timerTwo > 15000) {
    //Here we would read a sensor, perhaps, storing the value in a temporary variable
    //For this example, I will make something up . . .
    float temp = 78.34;
    float humidity = 56.97;

    char str_temp[6]; //a temp array of size 6 to hold "XX.XX" + the terminating character
    char str_humd[6]; //a temp array of size 6 to hold "XX.XX" + the terminating character

    //take temp, format it into 5 char array with a decimal precision of 2, and store it in str_temp
    dtostrf(temp, 5, 2, str_temp);
    //ditto
    dtostrf(humidity, 5, 2, str_humd);

    sprintf(message, "{\"temp\":\"%s\", \"humd\":\"%s\"}", str_temp, str_humd);
    //mqtt.publish("brock/allthedata/isthisatopic", message);
    timerTwo = millis();
  }

  if (millis() - timerThree > 23000) {
    boolean button = true;
    sprintf(message, "{\"buttonState\" : \"%d\"}", button); // %d is used for a bool as well
    //mqtt.publish("brock/allthedata/isthisatopic", message);
    timerThree = millis();
  }
}//end Loop

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(topic); //'topic' refers to the incoming topic name, the 1st argument of the callback function
  Serial.println("] ");

  DynamicJsonBuffer  jsonBuffer; //blah blah blah a DJB
  JsonObject& root = jsonBuffer.parseObject(payload); //parse it!

  if (!root.success()) { //well?
    Serial.println("parseObject() failed, are you sure this message is JSON formatted.");
    return;
  }

  if (strcmp(topic, "fromJon/words") == 0) {
    Serial.println("A message from Batman . . .");
  }

  root.printTo(Serial); //print out the parsed message
  Serial.println(); //give us some space on the serial monitor read out
}
