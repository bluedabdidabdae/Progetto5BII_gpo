
#include <Wire.h>
#include <Preferences.h>
#include "melodies.h"

#include <WiFi.h>
#include <HTTPClient.h>

#include <LiquidCrystal_I2C.h>

#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
//#include <MFRC522DriverI2C.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>

#define RW_MODE false
#define RO_MODE true

#define QUICK_SETUP true

// one try is done every 100ms
#define WIFI_MAX_TRIES 50

// used from the mfrc255
#define RST_PIN 15
#define KEY_A 0x60
#define FLAG_BLOCK (byte)4
#define SETTINGS_PASSWORD_BLOCK_1 (byte)5
#define SETTINGS_WIFI_SSID_BLOCK_1 (byte)8
#define SETTINGS_WIFI_SSID_BLOCK_2 (byte)9
#define SETTINGS_WIFI_SSID_BLOCK_3 (byte)10
#define SETTINGS_WIFI_PASS_BLOCK_1 (byte)12
#define SETTINGS_WIFI_PASS_BLOCK_2 (byte)13
#define SETTINGS_WIFI_PASS_BLOCK_3 (byte)14
#define SETTINGS_API_AUTH_TOKEN_BLOCK_1 (byte)16
#define SETTINGS_API_AUTH_TOKEN_BLOCK_2 (byte)17
#define USER_NAME_BLOCK (byte)8
#define USER_UID_BLOCK (byte)9
#define SETTINGS_STRING_BUFFER_SIZE 50
#define PREFERENCES_STRING_PLACEHOLDER "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\0"

#define CARD_ENABLED_BIT (flags_buffer[0] & (1 << 7))
#define SETTINGS_FLAG_BIT (flags_buffer[0] & (1 << 6))

#define IS_CARD_ENABLED (0 != CARD_ENABLED_BIT)
#define IS_CARD_SETTINGS (0 != SETTINGS_FLAG_BIT)

// Halt communication with the card
#define RESET_LOOP \
  mfrc522.PICC_HaltA(); \
  mfrc522.PCD_StopCrypto1(); \
  lcd.clear(); \
  lcd.setCursor(11, 1); \
  lcd.print("Ready"); \
  return

Preferences preferences;

HTTPClient http;
String LOGIN_PATHNAME = String("/stamp");

LiquidCrystal_I2C lcd(0x27, 16, 2);  // initialize the Liquid Crystal Display object with the I2C address 0x27, 16 columns and 2 rows

// Learn more about using SPI/I2C or check the pin assigment for your board: https://github.com/OSSLibraries/Arduino_MFRC522v2#pin-layout
MFRC522DriverPinSimple ss_pin(5);
MFRC522DriverSPI driver{ss_pin}; // Create SPI driver
//MFRC522DriverI2C driver{};     // Create I2C driver
MFRC522 mfrc522{driver};         // Create MFRC522 instance

MFRC522::MIFARE_Key key;
byte blockBufferSize = 18;
byte actualBlockSize = 16;
byte flags_buffer[18];
byte tmpBuffer1[18];
byte tmpBuffer2[18];
byte uid_buffer[5] = { '\0' };
byte settings_string_buffer_1[SETTINGS_STRING_BUFFER_SIZE + 1] = { '\0' };
byte settings_string_buffer_2[SETTINGS_STRING_BUFFER_SIZE + 1] = { '\0' };

void setup() {

  // initializing lcd
  lcd.init();       // initialize the LCD
  lcd.clear();      // clear the LCD display
  lcd.backlight();  // Make sure backlight is on

  // initializing serial port
  Serial.begin(9600);
  if (!Serial) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Serial absent");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Serial present");
    Serial.println();
    Serial.println("Hello There");
  }
  lcd.setCursor(6, 1);
  lcd.print("continuing");
  Serial.println("continuing");
  delay(2000);

  // starting setup
  Serial.println("Starting setup");
  if(!QUICK_SETUP) {
    lcd.setCursor(0, 0);
    lcd.print("Starting setup");
    delay(2000);
  }

  // setupping preferences
  preferences.begin("settings", RO_MODE);
  if(!preferences.isKey("init")){
    preferences.end();
    preferences.begin("settings", RW_MODE);
    //preferences.putString("pref_pass", PREFERENCES_STRING_PLACEHOLDER);
    //preferences.putString("wifi_ssid", PREFERENCES_STRING_PLACEHOLDER);
    //preferences.putString("wifi_pass", PREFERENCES_STRING_PLACEHOLDER);
    //preferences.putString("api_server", PREFERENCES_STRING_PLACEHOLDER);
    //preferences.putString("api_auth_token", PREFERENCES_STRING_PLACEHOLDER);
    preferences.putString("pref_pass", "password");
    preferences.putString("wifi_ssid", "wifi_ssid");
    preferences.putString("wifi_pass", "wifi_pass");
    preferences.putString("api_server", "nfc.rosa.onl");
    preferences.putString("api_auth_token", "api_auth_token");
    preferences.putBool("init", true);

    preferences.end();
  }
  Serial.println("Ok init pref");
  Serial.println("continuing");
  if(!QUICK_SETUP) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Ok init pref");
    lcd.setCursor(6, 1);
    lcd.print("continuing");
    delay(2000);
  }

  // initializing nfc
  mfrc522.PCD_Init();    // Init MFRC522 board.
  MFRC522Debug::PCD_DumpVersionToSerial(mfrc522, Serial);
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  Serial.println("Ok init nfc");
  Serial.println("continuing");
  if(!QUICK_SETUP) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Ok init nfc");
    lcd.setCursor(6, 1);
    lcd.print("continuing");
    delay(2000);
  }

  // testing audio output
  Serial.println("Testing audio");
  if(!QUICK_SETUP) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Testing audio");
    delay(1000);
  }
  playMelody(welcomeMelody);
  Serial.println("continuing");
  if(!QUICK_SETUP) {
    delay(1000);
    lcd.setCursor(6, 1);
    lcd.print("continuing");
    delay(2000);
  }

  wifiConnect(!QUICK_SETUP);
  if(!QUICK_SETUP) {
    delay(2000);
  }

  Serial.println("Setup complete");
  if(!QUICK_SETUP) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Setup complete");
    delay(2000);
  }

  lcd.clear();
  lcd.setCursor(11, 1);
  lcd.print("Ready");
  Serial.println("Ready");
}

void loop() {

  delay(1000);

  if(! mfrc522.PICC_IsNewCardPresent() || ! mfrc522.PICC_ReadCardSerial()) { RESET_LOOP; };

  gatherUID();

  if(!gatherFlags()) { RESET_LOOP; };
  
  if(!IS_CARD_ENABLED) {
    Serial.println("Card is disabled");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Card is disabled");
    RESET_LOOP;
  };

  playMelody(readMelody);
  if(IS_CARD_SETTINGS) {
    loadSettings();
  } else {
    if(WiFi.status() != WL_CONNECTED) {
      Serial.println("\nUnable to connect to WiFi");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Connection error");
    } else {
      loginUser();
    }
  }

  delay(3500);

  RESET_LOOP;
}

bool wifiConnect(bool lcdLog) {
  preferences.begin("settings", RO_MODE);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(preferences.getString("wifi_ssid"), preferences.getString("wifi_pass"));
  Serial.println("Connecting to WiFi ");
  if(lcdLog) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting wifi");
  }
  for(int tries = 0; WIFI_MAX_TRIES > tries && WiFi.status() != WL_CONNECTED; tries++){
    if(0 == tries % 10) Serial.print(".");
    delay(100);
  }

  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("\nUnable to connect to WiFi");
    if(lcdLog) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Err init wifi");
      lcd.setCursor(6, 1);
      lcd.print("continuing");
    }
    preferences.end();
    return false;
  }

  Serial.println("\nConnected to WiFi");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("Ok init wifi");
  Serial.println("continuing");
  if(lcdLog) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Ok init wifi");
    lcd.setCursor(6, 1);
    lcd.print("continuing");
  }
  preferences.end();
  return true;
}

bool gatherFlags() {
  if(mfrc522.PCD_Authenticate(KEY_A, FLAG_BLOCK, &key, &(mfrc522.uid)) != 0) {
    Serial.println("Authentication failed for flags sector");
    return false;
  }

  if (mfrc522.MIFARE_Read(FLAG_BLOCK, flags_buffer, &blockBufferSize) != 0) {
    Serial.println("Unable to read flags sector");

    lcd.clear();
    lcd.setCursor(11, 1);
    lcd.print("Error");
    return false;
  }

  Serial.println("FLAGS");
  Serial.print("enabled: ");
  Serial.println(IS_CARD_ENABLED);
  Serial.print("settings: ");
  Serial.println(IS_CARD_SETTINGS);
  Serial.println("END OF FLAGS");

  return true;
}

bool gatherUID() {
  // Dump UID into serial and save to variable
  Serial.print("Card UID: ");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    uid_buffer[i] = mfrc522.uid.uidByte[i];
  }
  Serial.println();

  return true;
}

bool loginUser() {

  // authenticate sector
  if (mfrc522.PCD_Authenticate(KEY_A, USER_NAME_BLOCK, &key, &(mfrc522.uid)) != 0) {
    Serial.println("Authentication failed");
    return false;
  }

  if (mfrc522.MIFARE_Read(USER_NAME_BLOCK, tmpBuffer1, &blockBufferSize) != 0) {
    Serial.println("Card corrupted");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Card corrupted");
    lcd.setCursor(11, 1);
    lcd.print("Error");
    return false;
  }

  Serial.print("User name: ");
  for (byte i = 1; i <= tmpBuffer1[0]; i++) {
    tmpBuffer2[i - 1] = tmpBuffer1[i];
    Serial.print((char)tmpBuffer1[i], HEX);  // Print as character
  }
  tmpBuffer2[tmpBuffer1[0]] = '\0';
  Serial.println();
  
  if (mfrc522.MIFARE_Read(USER_UID_BLOCK, tmpBuffer1, &blockBufferSize) != 0) {
    Serial.println("Card corrupted");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Card corrupted");
    lcd.setCursor(11, 1);
    lcd.print("Error");
    return false;

  }

  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////////

  preferences.begin("settings", RO_MODE);
  http.begin(preferences.getString("api_server") + LOGIN_PATHNAME + "/" + String((char*)tmpBuffer1));
  http.begin("google.com");
  http.addHeader("Authorization", preferences.getString("api_auth_token"));
  int httpCode = http.GET();
  preferences.end();

  // httpCode will be negative on error
  if (httpCode > 0) {
    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    } else {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET/POST... code: %d\n", httpCode);
    }
  } else {
    Serial.printf("[HTTP] GET/POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();

  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////////

  playMelody(welcomeMelody);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print((char *)tmpBuffer2);
  lcd.setCursor(9, 1);
  lcd.print("Welcome");

  return true;
}

bool checkPrefPassword() {

  preferences.begin("settings", RO_MODE);
  String saved_password = preferences.getString("pref_pass");
  preferences.end();
  bool password_is_valid = true;

  if(mfrc522.PCD_Authenticate(KEY_A, SETTINGS_PASSWORD_BLOCK_1, &key, &(mfrc522.uid)) != 0) {
    Serial.println("Authentication failed for flags sector");
    return false;
  }

  if (mfrc522.MIFARE_Read(SETTINGS_PASSWORD_BLOCK_1, settings_string_buffer_1, &blockBufferSize) != 0) {
    Serial.println("Unable to read password block 1");
    return false;
  }

  if (mfrc522.MIFARE_Read(SETTINGS_PASSWORD_BLOCK_1, settings_string_buffer_1 + blockBufferSize, &blockBufferSize) != 0) {
    Serial.println("Unable to read password block 2");
    return false;
  }
  for(int i = 0; i < saved_password.length() && password_is_valid; i++) {
    if(saved_password[i] != settings_string_buffer_1[i]) {
      password_is_valid = false;
    }
  }

  return true;
}

bool loadWifiSsid() {
  if(mfrc522.PCD_Authenticate(KEY_A, SETTINGS_WIFI_SSID_BLOCK_1, &key, &(mfrc522.uid)) != 0) {
    Serial.println("Authentication failed for wifi ssid sector");
    return false;
  }
  Serial.println("Wifi ssid sector authenticated");

  if (mfrc522.MIFARE_Read(SETTINGS_WIFI_SSID_BLOCK_1, settings_string_buffer_1, &blockBufferSize) != 0) {
    Serial.println("Unable to read wifi ssid block 1");
    return false;
  }
  Serial.println("Read wifi ssid block 1");

  if (mfrc522.MIFARE_Read(SETTINGS_WIFI_SSID_BLOCK_2, settings_string_buffer_1 + actualBlockSize, &blockBufferSize) != 0) {
    Serial.println("Unable to read wifi ssid block 2");
    return false;
  }
  Serial.println("Read wifi ssid block 2");

  if (mfrc522.MIFARE_Read(SETTINGS_WIFI_SSID_BLOCK_3, settings_string_buffer_1 + actualBlockSize * 2, &blockBufferSize) != 0) {
    Serial.println("Unable to read wifi ssid block 3");
    return false;
  }
  Serial.println("Read wifi ssid block 3");

  parseStringBuffer(settings_string_buffer_1, settings_string_buffer_2, SETTINGS_STRING_BUFFER_SIZE);

  preferences.begin("settings", RW_MODE);
  preferences.putString("wifi_ssid", (char*)settings_string_buffer_2);
  preferences.end();

  return true;
}

bool loadWifiPass() {
  if(mfrc522.PCD_Authenticate(KEY_A, SETTINGS_WIFI_PASS_BLOCK_1, &key, &(mfrc522.uid)) != 0) {
    Serial.println("Authentication failed for wifi pass sector");
    return false;
  }
  Serial.println("Wifi pass sector authenticated");

  if (mfrc522.MIFARE_Read(SETTINGS_WIFI_PASS_BLOCK_1, settings_string_buffer_1, &blockBufferSize) != 0) {
    Serial.println("Unable to read wifi pass block 1");
    return false;
  }
  Serial.println("Read wifi pass block 1");

  if (mfrc522.MIFARE_Read(SETTINGS_WIFI_PASS_BLOCK_2, settings_string_buffer_1 + actualBlockSize, &blockBufferSize) != 0) {
    Serial.println("Unable to read wifi pass block 2");
    return false;
  }
  Serial.println("Read wifi pass block 2");

  if (mfrc522.MIFARE_Read(SETTINGS_WIFI_PASS_BLOCK_3, settings_string_buffer_1 + actualBlockSize * 2, &blockBufferSize) != 0) {
    Serial.println("Unable to read wifi pass block 3");
    return false;
  }
  Serial.println("Read wifi pass block 3");

  parseStringBuffer(settings_string_buffer_1, settings_string_buffer_2, SETTINGS_STRING_BUFFER_SIZE);

  preferences.begin("settings", RW_MODE);
  preferences.putString("wifi_pass", (char*)settings_string_buffer_2);
  preferences.end();

  return true;
}

bool loadApiAuthToken() {
  if(mfrc522.PCD_Authenticate(KEY_A, SETTINGS_API_AUTH_TOKEN_BLOCK_1, &key, &(mfrc522.uid)) != 0) {
    Serial.println("Authentication failed for api auth token sector");
    return false;
  }
  Serial.println("Api auth token sector authenticated");

  if (mfrc522.MIFARE_Read(SETTINGS_API_AUTH_TOKEN_BLOCK_1, settings_string_buffer_1, &blockBufferSize) != 0) {
    Serial.println("Unable to read api auth token block 1");
    return false;
  }
  Serial.println("Read api auth token block 1");

  if (mfrc522.MIFARE_Read(SETTINGS_API_AUTH_TOKEN_BLOCK_2, settings_string_buffer_1 + actualBlockSize, &blockBufferSize) != 0) {
    Serial.println("Unable to read api auth token block 2");
    return false;
  }
  Serial.println("Read api token block 2");

  parseStringBuffer(settings_string_buffer_1, settings_string_buffer_2, SETTINGS_STRING_BUFFER_SIZE);

  preferences.begin("settings", RW_MODE);
  preferences.putString("api_auth_token", (char*)settings_string_buffer_2);
  preferences.end();

  return true;
}

bool parseStringBuffer(byte* from, byte* to, int maxBufferSize) {

  for (byte i = 0; i <= from[0] && i < maxBufferSize; i++) {
    to[i] = from[i + 1];
  }
  to[from[0] + 1] = '\0';

  return true;
}

bool loadSettings() {

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Settings card");
  lcd.setCursor(0, 1);
  lcd.print("NO REMOVE CARD");

  Serial.println("Settings card detected.");
  Serial.println("-----------------------------------------");
  Serial.println("<< DO NOT REMOVE CARD UNTIL COMPLETION >>");
  Serial.println("<< DO NOT REMOVE CARD UNTIL COMPLETION >>");
  Serial.println("<< DO NOT REMOVE CARD UNTIL COMPLETION >>");
  Serial.println("-----------------------------------------");

  delay(1500);

  lcd.setCursor(0, 0);
  lcd.print("Loading settings");
  Serial.println("Loading settings.");

  delay(1000);

  if(!checkPrefPassword()) {
    Serial.println("Invalid password.");
    Serial.println("Its now safe to remove the card.");

    lcd.setCursor(0, 0);
    lcd.print("Error");
    lcd.setCursor(0, 1);
    lcd.print("REMOVE CARD");
    return false;
  }
  Serial.println("Password is valid.");
  delay(1000);

  if(!loadApiAuthToken()) {
    Serial.println("Its now safe to remove the card.");

    lcd.setCursor(0, 0);
    lcd.print("Error");
    lcd.setCursor(0, 1);
    lcd.print("REMOVE CARD");
    return false;
  }
  Serial.println("Loaded new api auth token.");
  delay(1000);

  // hopefully it works
  if(!loadWifiSsid()) {
    Serial.println("Its now safe to remove the card.");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error");
    lcd.setCursor(0, 1);
    lcd.print("REMOVE CARD");
    return false;
  }
  Serial.println("Loaded new wifi ssid.");
  delay(1000);

  if(!loadWifiPass()) {
    Serial.println("Its now safe to remove the card.");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error");
    lcd.setCursor(0, 1);
    lcd.print("REMOVE CARD");
    return false;
  }
  Serial.println("Loaded new wifi pass.");
  delay(1000);

  // refreshing the wifi connection
  wifiConnect(true);

  delay(500);
  Serial.println("Settings loaded.");
  Serial.println("Its now safe to remove the card.");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Settings loaded");
  lcd.setCursor(0, 1);
  lcd.print("REMOVE CARD");

  return true;
}
