#include <LiquidCrystal.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include "time.h"

// WiFi credentials
#define WIFI_SSID "3mr"
#define WIFI_PASSWORD "3muur_777"

// Firebase configuration
#define API_KEY "AIzaSyDmkuIPTmMni_4MeIkHMvlg_rKC9vXuFHE"
#define USER_EMAIL "AhmedG1066@gmail.com"
#define USER_PASSWORD "123456"
#define DATABASE_URL "https://data-tesst-2-default-rtdb.firebaseio.com/"

// Pin definitions for LCD
const int rs = 19, en = 23, d4 = 32, d5 = 33, d6 = 25, d7 = 26;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Fingerprint sensor setup
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// Structure to store name and ID
struct FingerprintData
{
  uint8_t id;
  String name;
};

FingerprintData fingerprints[128]; // Array to store up to 128 fingerprints
uint16_t nextID = 0;               // Counter for automatic ID assignment

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup()
{
  Serial.begin(115200);
  lcd.begin(16, 2);
  lcd.print("Initializing...");

  // Initialize fingerprint sensor
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);
  if (finger.verifyPassword())
  {
    lcd.setCursor(0, 1);
    lcd.print("Sensor found!");
  }
  else
  {
    lcd.setCursor(0, 1);
    lcd.print("No sensor :(");
    while (1)
      delay(1);
  }
  delay(500);
  lcd.clear();

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lcd.print("Connecting WiFi");
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  lcd.clear();
  lcd.print("WiFi connected");
  delay(1000);
  lcd.clear();

  // Synchronize time (critical for Firebase)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  lcd.print("Syncing time...");
  Serial.println("Waiting for time sync...");
  while (time(nullptr) < 8 * 3600 * 2)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Time synchronized");
  delay(1000);
  lcd.clear();

  // Initialize Firebase
  setupFirebase();

  lcd.print("System ready!");
  delay(1000);
  lcd.clear();
}

void loop()
{
  // Check token status regularly
  if (Firebase.isTokenExpired())
  {
    Serial.println("Refreshing token...");
    Firebase.refreshToken(&config);
  }

  lcd.setCursor(0, 0);
  lcd.print("1:ADD 2:DEL 3:VIEW");
  lcd.setCursor(0, 1);
  lcd.print("0:SYNC W/FIREBASE");

  while (!Serial.available())
  {
  } // Wait for user input
  int choice = Serial.read() - '0'; // Read input and convert to int

  if (choice == 1)
  {
    addFingerprint();
  }
  else if (choice == 2)
  {
    deleteFingerprint();
  }
  else if (choice == 3)
  {
    viewFingerprints();
  }
  else if (choice == 0)
  {
    syncWithFirebase();
  }
  else
  {
    lcd.clear();
    lcd.print("Invalid choice");
    delay(2000);
    lcd.clear();
  }
}

void setupFirebase()
{
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;

  // Time configuration - updated for compatibility
  config.timeout.serverResponse = 10 * 1000;
  config.timeout.tokenGenerationError = 10 * 1000; // Changed from tokenGeneration
  config.timeout.socketConnection = 10 * 1000;

  // Token handling
  config.token_status_callback = tokenStatusCallback;
  config.signer.test_mode = false;

  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(4096, 1024);
  fbdo.setResponseSize(2048);

  // Initialize Firebase
  Firebase.begin(&config, &auth);

  // Wait for authentication
  Serial.println("Waiting for Firebase auth...");
  lcd.clear();
  lcd.print("Auth with Firebase");
  unsigned long authStart = millis();
  while (!Firebase.ready() && millis() - authStart < 30000)
  {
    delay(100);
    Serial.print(".");
  }

  if (!Firebase.ready())
  {
    Serial.println("Firebase auth failed!");
    printErrorDetails();
    lcd.clear();
    lcd.print("Firebase auth fail");
    delay(2000);
    ESP.restart();
  }
  else
  {
    Serial.println("Firebase authenticated");
  }
}

void addFingerprint()
{
  lcd.clear();
  lcd.print("Place finger...");
  delay(1000);

  // Get first image
  int p = -1;
  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
    switch (p)
    {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
    delay(100);
  }

  // First image to template
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK)
  {
    lcd.clear();
    lcd.print("Image error: " + String(p));
    Serial.println("Image2Tz error: " + String(p));
    delay(2000);
    return;
  }

  lcd.clear();
  lcd.print("Remove finger");
  delay(2000);

  p = 0;
  while (p != FINGERPRINT_NOFINGER)
  {
    p = finger.getImage();
    delay(100);
  }

  lcd.clear();
  lcd.print("Place same finger");
  delay(2000);

  // Get second image
  p = -1;
  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
    switch (p)
    {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
    delay(100);
  }

  // Second image to template
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK)
  {
    lcd.clear();
    lcd.print("Image error: " + String(p));
    Serial.println("Image2Tz error: " + String(p));
    delay(2000);
    return;
  }

  // Create model
  p = finger.createModel();
  if (p != FINGERPRINT_OK)
  {
    lcd.clear();
    lcd.print("Model error: " + String(p));
    Serial.println("CreateModel error: " + String(p));

    switch (p)
    {
    case FINGERPRINT_ENROLLMISMATCH:
      Serial.println("Prints did not match");
      lcd.print("Prints mismatch");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }

    delay(3000);
    return;
  }

  // Automatically assign the next available ID
  uint16_t id = nextID;
  nextID++;

  // Store the fingerprint model
  p = finger.storeModel(id);
  if (p != FINGERPRINT_OK)
  {
    lcd.clear();
    lcd.print("Store failed: " + String(p));
    Serial.println("StoreModel error: " + String(p));
    delay(2000);
    return;
  }

  // Ask for the name
  lcd.clear();
  lcd.print("Enter name:");
  Serial.println("Enter name:");

  String name = "";
  while (true)
  {
    while (Serial.available() > 0)
    {
      char c = Serial.read();
      if (c == '\n' || c == '\r')
      {
        if (name.length() > 0)
        {
          goto nameReceived;
        }
      }
      else
      {
        name += c;
      }
    }
  }

nameReceived:
  name.trim();

  // Store locally
  fingerprints[id].id = id;
  fingerprints[id].name = name;

  // Display info
  lcd.clear();
  lcd.print("ID: " + String(id));
  lcd.setCursor(0, 1);
  lcd.print("Name: " + name);

  Serial.println("Fingerprint saved!");
  Serial.println("ID: " + String(id));
  Serial.println("Name: " + name);

  // Upload to Firebase
  if (Firebase.ready())
  {
    String path = "/fingerprints/" + String(id);
    FirebaseJson json;
    json.set("id", id);
    json.set("name", name);

    if (Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json))
    {
      Serial.println("Uploaded to Firebase");
      lcd.clear();
      lcd.print("Uploaded to");
      lcd.setCursor(0, 1);
      lcd.print("Firebase!");
    }
    else
    {
      Serial.println("Firebase upload failed: " + fbdo.errorReason());
      lcd.clear();
      lcd.print("Upload failed");
      lcd.setCursor(0, 1);
      lcd.print(fbdo.errorReason().substring(0, 16));
    }
  }

  delay(3000);
  lcd.clear();
}

void deleteFingerprint()
{
  lcd.clear();
  lcd.print("Enter ID to delete:");
  Serial.println("Enter ID to delete:");

  // Wait for the ID input
  while (Serial.available() == 0)
  {
  }
  uint16_t id = Serial.parseInt();

  if (id < 0 || id > 127)
  {
    lcd.clear();
    lcd.print("Invalid ID");
    delay(2000);
    return;
  }

  // Delete from fingerprint sensor
  int p = finger.deleteModel(id);
  if (p != FINGERPRINT_OK)
  {
    lcd.clear();
    lcd.print("Delete failed: " + String(p));
    Serial.println("DeleteModel error: " + String(p));
    delay(2000);
    return;
  }

  // Clear local data
  fingerprints[id].id = 0;
  fingerprints[id].name = "";

  // Delete from Firebase
  if (Firebase.ready())
  {
    String path = "/fingerprints/" + String(id);
    if (Firebase.RTDB.deleteNode(&fbdo, path.c_str()))
    {
      Serial.println("Deleted from Firebase");
      lcd.clear();
      lcd.print("Deleted from");
      lcd.setCursor(0, 1);
      lcd.print("Firebase!");
    }
    else
    {
      Serial.println("Firebase delete failed: " + fbdo.errorReason());
      lcd.clear();
      lcd.print("Delete failed");
      lcd.setCursor(0, 1);
      lcd.print(fbdo.errorReason().substring(0, 16));
    }
  }

  delay(2000);
  lcd.clear();
}

void viewFingerprints()
{
  lcd.clear();
  lcd.print("Viewing records...");
  Serial.println("Stored fingerprints:");

  if (Firebase.ready())
  {
    // Get all fingerprints from Firebase
    if (Firebase.RTDB.getJSON(&fbdo, "/fingerprints"))
    {
      FirebaseJson json = fbdo.jsonObject();
      FirebaseJsonData result;

      for (int i = 0; i < 128; i++)
      {
        if (json.get(result, "/" + String(i)))
        {
          if (result.success)
          {
            FirebaseJson data;
            data.setJsonData(result.stringValue);

            uint16_t id;
            String name;

            data.get(result, "id");
            id = result.to<int>();

            data.get(result, "name");
            name = result.to<String>();

            Serial.print("ID: ");
            Serial.print(id);
            Serial.print(", Name: ");
            Serial.println(name);
          }
        }
      }
    }
    else
    {
      Serial.println("Failed to get data from Firebase: " + fbdo.errorReason());
      lcd.clear();
      lcd.print("Fetch failed");
      lcd.setCursor(0, 1);
      lcd.print(fbdo.errorReason().substring(0, 16));
    }
  }

  delay(3000);
  lcd.clear();
}

void syncWithFirebase()
{
  lcd.clear();
  lcd.print("Syncing with");
  lcd.setCursor(0, 1);
  lcd.print("Firebase...");

  // Download from Firebase to update local records
  if (Firebase.ready())
  {
    if (Firebase.RTDB.getJSON(&fbdo, "/fingerprints"))
    {
      FirebaseJson json = fbdo.jsonObject();
      FirebaseJsonData result;

      for (int i = 0; i < 128; i++)
      {
        if (json.get(result, "/" + String(i)))
        {
          if (result.success)
          {
            FirebaseJson data;
            data.setJsonData(result.stringValue);

            uint16_t id;
            String name;

            data.get(result, "id");
            id = result.to<int>();

            data.get(result, "name");
            name = result.to<String>();

            // Update local records
            fingerprints[id].id = id;
            fingerprints[id].name = name;

            // Update nextID if needed
            if (id >= nextID)
            {
              nextID = id + 1;
            }
          }
        }
      }
      lcd.clear();
      lcd.print("Sync complete!");
      Serial.println("Sync with Firebase complete");
    }
    else
    {
      lcd.clear();
      lcd.print("Sync failed!");
      Serial.println("Sync failed: " + fbdo.errorReason());
    }
  }

  delay(2000);
  lcd.clear();
}

void printErrorDetails()
{
  Serial.println("Firebase Error:");
  Serial.println(fbdo.errorReason());

  if (fbdo.httpCode())
  {
    Serial.print("HTTP Code: ");
    Serial.println(fbdo.httpCode());
  }

  if (fbdo.payload().length())
  {
    Serial.print("Payload: ");
    Serial.println(fbdo.payload());
  }
}