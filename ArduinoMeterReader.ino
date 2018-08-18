#include <SPI.h>
#include <Ethernet.h>

///////// CHANGEABLE VALUES /////////

const char importMultiplier[] = "1.25";
const char generationMultiplier[] = "1.0";

const char pompeii[] = "192.168.0.16";
const int pompeiiPort = 28080;

const unsigned long millisecondsBetweenCalls = 60000L;

const char pompeiiService[] = "/meter";

///////// CHANGEABLE VALUES ABOVE /////////

EthernetClient pompeiiClient;
const byte mac[] = {0x90, 0xA2, 0xDA, 0x0F, 0xA1, 0xE6};

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

void setup() {
  Serial.begin(9600);
  connectToEthernet();
}

void connectToEthernet()
{
  unsigned long millisecondsPerMinute = 60000L;
  // attempt to connect to Wifi network:
  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP waiting 1 minute");
    delay(millisecondsPerMinute);

    if (Ethernet.begin(mac) == 0)
    {
      Serial.println("Failed to configure Ethernet using DHCP waiting 1 more minute");
      delay(millisecondsPerMinute);

      if (Ethernet.begin(mac) == 0) {
        Serial.println("Failed to configure Ethernet using DHCP stopping - will need reset");
        while (true);
      }
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
    sendResultsToPompeii();
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

void sendResultsToPompeii() {
  Serial.println("sendResultsToPompeii");

  String postData = getPostData();
  Serial.println(postData);

  if (pompeiiClient.connect(pompeii, pompeiiPort)) {
    Serial.println("connected to pompeii");
    // Make a HTTP request:
    pompeiiClient.println("POST " + String(pompeiiService) + " HTTP/1.1");
    pompeiiClient.println("Host: " + String(pompeii) + ":" + pompeiiPort);
    pompeiiClient.println("Content-Type: application/json");
    pompeiiClient.println("Content-Length: " + String(postData.length()));
    pompeiiClient.println("Pragma: no-cache");
    pompeiiClient.println("Cache-Control: no-cache");
    pompeiiClient.println("Connection: close");
    pompeiiClient.println();

    pompeiiClient.println(postData);
    pompeiiClient.println();

    delay(10);
    pompeiiClient.stop();
    pompeiiClient.flush();
    Serial.println("Called pompeii");

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
