#include <SPI.h>
#include <Ethernet.h>

///////// CHANGEABLE VALUES /////////

char importMultiplier[] = "1.25";
char generationMultiplier[] = "1.0";

char pompeii[] = "192.168.0.16";
int pompeiiPort = 80;

long millisecondsBetweenCalls = 60000L;

///////// CHANGEABLE VALUES ABOVE /////////

EthernetClient pompeiiClient;
byte mac[] = {0x90, 0xA2, 0xDA, 0x0F, 0xA1, 0xE6};
char pompeiiService[] = "/pvoutput-post-consumption.php";

unsigned long importImpulseCount = 0;
unsigned long generationImpulseCount = 0;

unsigned long lastTimeUploaded = millis();
unsigned long previousTime = 0UL;

boolean interruptsAttached = false;

// is actually pin 3...
const int importSensorPin = 1;
// is actually pin 2...
const int generationSensorPin = 0;

const int voltageSensorPin = A0;

unsigned long millisecondsPerMinute = 60000L;

void setup() {
  Serial.begin(9600);
  connectToEthernet();
}

void connectToEthernet()
{
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
        while(true);
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

  if(currentTime < previousTime)  {
    lastTimeUploaded = currentTime;
  }
  
  previousTime = currentTime;

  if( (currentTime - lastTimeUploaded) >= millisecondsBetweenCalls) {
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
    pompeiiClient.print("POST ");
    pompeiiClient.print(pompeiiService);
    pompeiiClient.println(" HTTP/1.1");
    pompeiiClient.print("Host: ");
    pompeiiClient.print(pompeii);
    pompeiiClient.print(":");
    pompeiiClient.println(pompeiiPort);
    pompeiiClient.println("Accept: text/html");
    pompeiiClient.println("Content-Type: application/x-www-form-urlencoded; charset=UTF-8");
    pompeiiClient.print("Content-Length: ");
    pompeiiClient.println(postData.length());
    pompeiiClient.println("Pragma: no-cache");
    pompeiiClient.println("Cache-Control: no-cache");
    pompeiiClient.println("Connection: close");
    pompeiiClient.println("X-Data-Source: METER");
    pompeiiClient.println();

    pompeiiClient.println(postData);
    pompeiiClient.println();

    pompeiiClient.stop();
    pompeiiClient.flush();
    Serial.println("Called pompeii");
  }
}

String calculateImportWattsAndResetFlashes()
{
  if (importImpulseCount == 0UL) {
    return "0";
  }

  String wattImpulses = String(importImpulseCount);
  Serial.print("Calculated Import Watts: ");
  Serial.println(wattImpulses);
  
  importImpulseCount = 0UL;
  
  return wattImpulses;
}

String calculateGenerationWattsAndResetFlashes()
{
  if (generationImpulseCount == 0UL) {
    return "0";
  }

  String wattImpulses = String(generationImpulseCount);
  Serial.print("Calculated Generation Watts: ");
  Serial.println(wattImpulses);
  
  generationImpulseCount = 0UL;
  
  return wattImpulses;
}

String getPostData()
{
  String importWatts = calculateImportWattsAndResetFlashes();
  String generationWatts = calculateGenerationWattsAndResetFlashes();
  
  return "importImpulses=" + importWatts + "&generationImpulses=" + generationWatts + "&msBetweenCalls=" + millisecondsBetweenCalls 
      + "&importMultiplier=" + importMultiplier + "&generationMultiplier=" + generationMultiplier + "&mainsVoltage=" + readMainsVoltage();
}

String readMainsVoltage()
{
  double readAnalog = ((double)analogRead(voltageSensorPin) * (5.0 / 1023.0));
  Serial.println(readAnalog);
  double mainsVoltage = ((readAnalog * 49.68d) + 24.04d) - 1.7d;
  
  char tempChar[10];
  dtostrf(mainsVoltage,3,2,tempChar);
  
  return String(tempChar);
}
