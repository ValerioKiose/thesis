/*********************************************************************

       Programmable Electrical Circuit Testing Device

 *********************************************************************
   Thesis Identifier: #24130

   Supervisor:        Giakoumis Aggelos, ang1960@ihu.gr

   Assigned Student:  Kiose Valerio, iee2019070

   Institution:       International Hellenic University
 ******************************************************************
  Description:
        biri biri

   Microcontroller:   Espressif ESP32

   Compiler:          Arduino IDE

   Notes:
      - Put error checking in the database function
 ********************************************************************/

#include <WiFi.h>               // WiFi connection
#include <AsyncTCP.h>           // Asynchronous TCP library required by ESPAsyncWebServer.h
#include <HTTPClient.h>         // Communication with the SQL server
#include <ESPAsyncWebServer.h>  // Asynchronous Webserver Creation 
#include <WebSocketsServer.h>   // Communication between client and server through Websockets
#include <ArduinoJson.h>        // JSON encapsulation
#include <FS.h>                 // File system library required by SPIFFS.h
#include <SPIFFS.h>             // Access to onboard flash memory for file storage (SPIFFS)
#include <Wire.h>

// ADS1114 I2C address is 0x48(72)
#define Addr 0x48

uint8_t data[2];                //Stores two 8 bit registers to make one 16 bit value
uint16_t raw_adc = 0;           //Stores in 16 bit format the ADC reading
float measurements[16];         //Stores 16 consiquent measurements of the ADC
int measurementCount = 0;       //Counts how many measurements have been taken
float adc_res = 4.096/(65535/2);//ADC resolution 
float adc_cal = 8000/8403.77;   //Error correction
unsigned long interval = 115;   //Time between each ADC reading
unsigned long pMillis = 0;      //Previous Millis
float min_values[16];           //Minimum accepted values for each probe
float max_values[16];           //Maximum accepted values for each probe
float measured_values[16];      //AVG measurement of each probe
int results[16];                //FAILED or PASSED results of each probe
int counter = 0;

// SSID and password of Wifi connection:
const char* ssid = "GOZMOTE";
const char* password = "";
//const char* ssid = "";
//const char* password = "";

// Link to local host SQL database
String URL = "http://192.168.1.4/thesis/test_data.php";

// Initialization of webserver and websocket
AsyncWebServer server(80);  // port 80
WebSocketsServer webSocket = WebSocketsServer(81);  //port 81

void setup() {
  Serial.begin(115200);
  Wire.begin();
  setupADC();
  /*****************************SPIFFS SETUP*****************************/
  // Check if SPIFFS is initialized
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS could not initialize");
  }
  // Connect to WiFi
  connectWiFi();
  // Route to load main.html file
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/main.html", "text/html");
  });
  // Show 404 error if main.html is not found
  server.onNotFound([](AsyncWebServerRequest * request) {
    request->send(404, "text/plain", "File not found");
  });
  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });
  // Show 404 error if style.css is not found
  server.onNotFound([](AsyncWebServerRequest * request) {
    request->send(404, "text/plain", "File not found");
  });
  // Route to load script.js file
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/script.js", "text/javascript");
  });
  // Show 404 error if script.js is not found
  server.onNotFound([](AsyncWebServerRequest * request) {
    request->send(404, "text/plain", "File not found");
  });
  // dont know what it does
  server.serveStatic("/", SPIFFS, "/");
  /*********************************************************************/

  // Start websocket
  webSocket.begin();
  // Set webSocketEvent to be called every time a web socket event is received
  webSocket.onEvent(webSocketEvent);
  // Start server -> best practise is to start the server after the websocket
  server.begin();
}

void loop() {
  // Update function for the webSockets
  webSocket.loop();
}

/**************************************************************************
  webSocketEvent()
    -When a web socket event is received, webSocketEvent() checks what the
  client is doing and acts accordingly, the client can either be connected,
  disconnected and or sending data to the server.
    -Parameters:
  num: id of the client who send the event,
  type: type of message,
  payload: actual data sent,
  length: length of payload
***************************************************************************/
void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length) {
  // Switch on the type of information sent
  switch (type) {
    //  Client is disconnected
    case WStype_DISCONNECTED:
      Serial.println("Client " + String(num) + " disconnected");
      break;
    // Client is connected
    case WStype_CONNECTED:
      Serial.println("Client " + String(num) + " connected");
      break;
    // Client has sent data
    case WStype_TEXT:
      // Create JSON container
      StaticJsonDocument<200> doc_rx;
      // Try to deserialize the JSON string received
      DeserializationError error = deserializeJson(doc_rx, payload);
      if(error){
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }else{
        // JSON string was deserialized correctly, information can be retrieved:
        //const char* l_type = doc["type"]; // Probe function
        const int l_value = doc_rx["value_min"]; // Min
        const int r_value = doc_rx["value_max"]; // Max
        min_values[counter] = l_value;
        max_values[counter] = r_value;
        makeMeasurement[counter] = makeMeasurement();
        counter++;
        //we need an if to check when the min max have all been saved and then begin ADC readings
      }
      break;
  }
}

/**************************************************************************
  sendJson()
    -Sends results and ADC reading information to web clients.
***************************************************************************/
void sendJson() {
  // JSON container
  StaticJsonDocument<200> doc_tx;
  // JSON Object
  JsonObject object = doc_tx.to<JsonObject>();
  for(int i = 0; i < counter; i++){
    // Writing data into the JSON object
    object["value"] = measured_values[i];
    object["result"] = results[i];
    // JSON string for sending data to the client
    String jsonString = "";
    // Convert JSON object to string
    serializeJson(doc_tx, jsonString);
    // Send JSON string to client
    webSocket.broadcastTXT(jsonString);
  }
}

/**************************************************************************
  connectWiFi()
    Uses the variables [ssid] and [password] to establish a wifi connection
***************************************************************************/
void connectWiFi() {
  // Start WiFi interface
  WiFi.begin(ssid, password);
  // Print SSID to the serial interface for debugging
  Serial.println("Establishing connection to WiFi with SSID: " + String(ssid));
  // Wait until WiFi is connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  // Show IP address that the ESP32 has received from router
  Serial.print("Connected to network with IP address: ");
  Serial.println(WiFi.localIP());
}

/**************************************************************************
  sendDatabase()
    Saves measurements to the database.
    -Parameters:
  num: id of the client who send the event,
***************************************************************************/
void sendDatabase(String min, String max) {
  String postData = "&min=" + min + "&max=" + max;

  HTTPClient http;
  WiFiClient client;
  http.begin(client, URL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int httpCode = http.POST(postData);
  String payload = "";

  if (httpCode > 0) {
    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    } else {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();  //Close connection
}

/**************************************************************************
  makeMeasurement()
    Makes a measurement with the ADC
***************************************************************************/
float makeMeasurement(){
  Wire.beginTransmission(Addr);
  // Select data register
  Wire.write(0x00);
  // Stop I2C Transmission
  Wire.endTransmission();
  // Request 2 bytes of data
  Wire.requestFrom(Addr, 2);
  // Read 2 bytes of data
  // raw_adc msb, raw_adc lsb
  if (Wire.available() == 2){
    data[0] = Wire.read();
    data[1] = Wire.read();
  }
  // Convert the data
  raw_adc = (data[0] << 8) + data[1];

  float raw_voltage = (raw_adc * adc_res) * adc_cal;
  float true_voltage = ((raw_voltage - 2.41)/0.16666) + 2.41;

  measurements[measurementCount] = true_voltage;

  if(measurementCount >= 16){
    float sum = 0;
    for (int i = 0; i < 16; i++) {
      sum += measurements[i];
    }
    float average = sum / 16;
    measurementCount = 0;
    return average;
  }else{
    measurementCount++;
  }
}

/**************************************************************************
  setupADC()  CHANGE TO 16 SAMPLES!!!!!!!!
    Set up ADC settings
***************************************************************************/
void setupADC(){
  Wire.beginTransmission(Addr);
  // Select configuration register
  Wire.write(0x01);
  /*
  A 16 bit register controlling the ADC
  0 100 001 0 000 0 0 0 11
  - MUX PGA M DR  - - - --
  */
  // AIN0 & GND, +/- 4.096V
  Wire.write(0x42);// 01000010
  // Continuous conversion mode, 8 SPS
  Wire.write(0x03);// 00000011
  // Stop I2C Transmission
  Wire.endTransmission();
  delay(300);
}

/**************************************************************************
  min_max_check()
    Checks if measured voltage is within spec and sends info to client
***************************************************************************/
void min_max_check(){
  for (i = 0; i < counter; i++){
    if (measured_values[i] >= min_values[i] && measured_values[i] =< max_values[i]){
      results[i] = 1;
    }else{
      results[i] = 0;
    }
  }
  sendJson();
}
