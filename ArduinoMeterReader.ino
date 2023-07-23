#include <SPI.h>
#include <Ethernet.h>

///////// CHANGEABLE VALUES /////////

const char importMultiplier[] = "1.0";
const char generationMultiplier[] = "1.0";

const char serverAddress[] = "home-monitoring.scaleys.co.uk";
const int serverPort = 80;
const int httpRequestDelay = 20;

const unsigned long millisecondsBetweenCalls = 60000L;

const char serviceEndpoint[] = "/meter";

///////// CHANGEABLE VALUES ABOVE /////////

EthernetClient ethernetClient;
byte mac[] = {0x90, 0xA2, 0xDA, 0x0F, 0xA1, 0xE6};

unsigned long importImpulseCount = 0;
unsigned long generationImpulseCount = 0;

unsigned long importImpulseCountSinceLastUpload = 0;
unsigned long generationImpulseCountSinceLastUpload = 0;
unsigned long millisecondsBetweenCallsSinceLastUpload = 0;

unsigned long lastTimeUploaded = millis();
unsigned long previousTime = 0UL;

boolean interruptsAttached = false;

// is actually pin 3...
const int importSensorPin = 1;
// is actually pin 2...
const int generationSensorPin = 0;

const int voltageSensorPin = A0;
IPAddress ip(192,168,0,12);

void setup() {
  Serial.begin(9600);
  connectToEthernet();
}

void connectToEthernet()
{
  unsigned long millisecondsPerMinute = 60000L;
  // attempt to connect to Wifi network:
  // start the Ethernet connection:
  bool connectedToNetwork = false;
  while(!connectedToNetwork) {
    Serial.println("Attempting to connect to network...");

    if (Ethernet.begin(mac) == 0) {
        Serial.println("Failed to connect, trying again...");
    } else {
        Serial.println("Connected successfully");
        connectedToNetwork = true;
    }
  }

  // give the Ethernet shield a second to initialize:
  delay(1000);
  Serial.println("connecting...");

  Serial.print("Connected to the network IP: ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  attachInterrupts();

  if (isTimeToUploadData())
  {
    Serial.println("Uploading data");
    sendResultsToServer();
  }
}

void attachInterrupts()
{
  if (!interruptsAttached)
  {
    interruptsAttached = true;
    Serial.println("Attaching import interrupt on pin 3");
    attachInterrupt(importSensorPin, flashImport, FALLING);
    Serial.println("Attaching generation interrupt on pin 2");
    attachInterrupt(generationSensorPin, flashGeneration, FALLING);
  }
}

boolean isTimeToUploadData() {
  unsigned long currentTime = millis();

  if (currentTime < previousTime)  {
    lastTimeUploaded = currentTime;
  }

  previousTime = currentTime;

  if ( (currentTime - lastTimeUploaded) >= millisecondsBetweenCalls) {
    Serial.println("Time to upload");
    lastTimeUploaded = currentTime;
    return true;
  }
  return false;
}

/* Handles the interrupt flash logic */
void flashImport() {
  importImpulseCount++;
}

void flashGeneration() {
  generationImpulseCount++;
}

void sendResultsToServer() {
  Serial.println("sendResultsToServer");

  String postData = getPostData();
  Serial.println(postData);

  if (ethernetClient.connect(serverAddress, serverPort)) {
    Serial.println("connected to server");
    // Make a HTTP request:
    ethernetClient.println("POST " + String(serviceEndpoint) + " HTTP/1.1");
    ethernetClient.println("Host: " + String(serverAddress) + ":" + serverPort);
    ethernetClient.println("Content-Type: application/json");
    ethernetClient.println("Content-Length: " + String(postData.length()));
    ethernetClient.println("Pragma: no-cache");
    ethernetClient.println("Cache-Control: no-cache");
    ethernetClient.println("Connection: close");
    ethernetClient.println();

    ethernetClient.println(postData);
    ethernetClient.println();

    delay(httpRequestDelay);
    ethernetClient.stop();
    ethernetClient.flush();
    Serial.println("Called server");

    uploadSuccessResetCounters();
  }
}

String calculateImportWattsAndResetFlashes()
{
  importImpulseCountSinceLastUpload = importImpulseCountSinceLastUpload + importImpulseCount;
  importImpulseCount = 0UL;

  if (importImpulseCountSinceLastUpload == 0UL) {
    return "0";
  }

  return String(importImpulseCountSinceLastUpload);
}

String calculateGenerationWattsAndResetFlashes()
{
  generationImpulseCountSinceLastUpload = generationImpulseCountSinceLastUpload + generationImpulseCount;
  generationImpulseCount = 0UL;

  if (generationImpulseCountSinceLastUpload == 0UL) {
    return "0";
  }

  return String(generationImpulseCountSinceLastUpload);
}

String getPostData()
{
  String importWatts = calculateImportWattsAndResetFlashes();
  String generationWatts = calculateGenerationWattsAndResetFlashes();
  millisecondsBetweenCallsSinceLastUpload = millisecondsBetweenCallsSinceLastUpload + millisecondsBetweenCalls;
  String milliseconds = String(millisecondsBetweenCallsSinceLastUpload);

  return "{\"importImpulses\":" + importWatts + ",\"generationImpulses\":" + generationWatts + ",\"msBetweenCalls\":" + milliseconds
         + ",\"importMultiplier\":" + importMultiplier + ",\"generationMultiplier\":" + generationMultiplier + ",\"mainsVoltage\":" + readMainsVoltage() + "}";
}

String readMainsVoltage()
{
  double readAnalog = ((double)analogRead(voltageSensorPin) * (5.0 / 1023.0));
  //Serial.println(readAnalog);
  double mainsVoltage = ((readAnalog * 49.68d) + 24.04d) - 1.7d;

  char tempChar[10];
  dtostrf(mainsVoltage, 3, 2, tempChar);

  return String(tempChar);
}

void uploadSuccessResetCounters() {
  importImpulseCountSinceLastUpload = 0UL;
  generationImpulseCountSinceLastUpload = 0UL;
  millisecondsBetweenCallsSinceLastUpload = 0UL;
}
