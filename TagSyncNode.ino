// Made by a student from Kinatakutan National High School

#include <WiFi.h>
#include <SD.h>
#include <SPI.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <MFRC522.h>             
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <vector>
#include <map>

// ====== Configuration for DS1302 RTC ======
#define DS1302_CE_PIN  12
#define DS1302_IO_PIN  13
#define DS1302_CLK_PIN 14

// ====== Configuration for the RGB LED =======
#define RED 27
#define GREEN 26
#define BLUE 25

// Initialize ThreeWire and RTC instance
ThreeWire rtcWire(DS1302_CLK_PIN, DS1302_IO_PIN, DS1302_CE_PIN);
RtcDS1302<ThreeWire> rtc(rtcWire);

// ====== Chat WebSocket Setup ======
AsyncWebSocket ws("/ws"); // WebSocket endpoint for chat

std::map<uint32_t, String> wsUsernames;

// Global AsyncWebServer instance on port 80
AsyncWebServer server(80);

// Set this to true for Station mode, false for AP mode
#define USE_STATION true

// Replace with your network credentials (for station mode)
const char* ssid     = "YourSSID";
const char* password = "YourPassword";

// ====== Static IP configuration ======
IPAddress local_IP(192, 168, 0, 3); // Set your desired static IP
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// Define pins for the MFRC522 (adjust as needed so they don’t conflict with SD card SPI)
#define RST_PIN   22    // Reset pin for MFRC522
#define SS_PIN    21    // Slave select pin for MFRC522
#define RFID_DATA_BLOCK 4  // The block used to store/read the owner name on the RFID card

// Create an MFRC522 instance
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Global file object for handling uploads
File uploadFile;

// Global variable for a new name from serial input.
String newName = "";

// Utility: return MIME type based on file extension
String getContentType(const String& filename) {
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".xlsx")) return "application/vnd.ms-excel";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/pdf";
  else if (filename.endsWith(".docx"))
    return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
  else if (filename.endsWith(".pptx"))
    return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
  return "text/plain";
}

// ====== WebSocket Event Handler for Chat ======
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
               void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WebSocket client connected: %u\n", client->id());
  }
  else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WebSocket client disconnected: %u\n", client->id());

    wsUsernames.erase(client->id());
  }
  else if (type == WS_EVT_DATA) {
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if (info->opcode == WS_TEXT) {
      String msg = "";
      for (size_t i = 0; i < len; i++) {
        msg += (char) data[i];
      }
      // If the client isn't registered yet, expect a registration message
      if (wsUsernames.find(client->id()) == wsUsernames.end()) {
        if (msg.startsWith("register:")) {
          String desiredName = msg.substring(9);
          desiredName.trim();
          bool taken = false;
          // Check if the name is already in use
          for (auto &entry : wsUsernames) {
            if (entry.second == desiredName) {
              taken = true;
              break;
            }
          }
          if (taken || desiredName == "") {
            client->text("error: Name already in use or invalid. Try another one.");
          } else {
            wsUsernames[client->id()] = desiredName;
            client->text("success: Registered as " + desiredName);
            Serial.printf("Client %u registered as %s\n", client->id(), desiredName.c_str());
          }
        } else {
          client->text("error: Please register first with 'register:YourName'");
        }
      } else {
        // Already registered, treat the incoming message as a chat message.
        String username = wsUsernames[client->id()];
        String chatMessage = username + ": " + msg;
        // Broadcast the chat message to all connected WebSocket clients.
        server->textAll(chatMessage);
        Serial.println("Chat message: " + chatMessage);
      }
    }
  }
}

// ====== WiFi Initialization with Static IP ======
void initWiFi() {
  if (USE_STATION) {
    // Set static IP configuration before connecting
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
      Serial.println("Static IP configuration failed.");
    }
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      digitalWrite(GREEN, LOW);
      digitalWrite(RED, HIGH);
      digitalWrite(BLUE, LOW);
      delay(500);
      Serial.print(".");
      retries++;
      digitalWrite(GREEN, LOW);
      digitalWrite(RED, LOW);
      digitalWrite(BLUE, LOW);
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected to WiFi!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nFailed to connect. Switching to AP mode.");
      WiFi.mode(WIFI_AP);
      WiFi.softAP("ESP32_FileShare");
      Serial.print("AP IP address: ");
      Serial.println(WiFi.softAPIP());
    }
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32_FileShare");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
  }
}

// Asynchronous file upload handler
void handleUpload(AsyncWebServerRequest *request, const String &filename,
                  size_t index, uint8_t *data, size_t len, bool final) {
  if (index == 0) {
    Serial.printf("Upload Start: %s\n", filename.c_str());
    // Ensure filename starts with '/'
    String path = "/uploads/" + filename;
    uploadFile = SD.open(path, FILE_WRITE);
    if (!uploadFile) {
      Serial.println("Failed to open file for writing");
      return;
    }
  }
  if (uploadFile) {
    if (len > 0)
      uploadFile.write(data, len);
    if (final) {
      uploadFile.close();
      Serial.printf("Upload Success: %s\n", filename.c_str());
    }
  }
}

// Generate a dynamic HTML page listing all files in /uploads
String generateFileList() {
  String output = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>File List</title>";
  output += "<style>body { font-family: Arial, sans-serif; background-color: #1a1a1a; color: #00ffcc; } ";
  output += "h1 { text-align: center; } ul { list-style: none; padding: 0; } ";
  output += "li { margin: 10px; text-align: center; } a { color: #00ffcc; text-decoration: none; }</style>";
  output += "</head><body><h1>Uploaded Files</h1><ul>";

  File root = SD.open("/uploads");
  if (root) {
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        String fname = file.name(); // e.g., "/uploads/filename.txt"
        String displayName = fname;
        if (fname.startsWith("/uploads/"))
          displayName = fname.substring(String("/uploads/").length());
        output += "<li><a href=\"/download?file=" + displayName + "\">" + displayName + "</a></li>";
      }
      file = root.openNextFile();
    }
    root.close();
  }
  output += "</ul></body></html>";
  return output;
}

void setupServer() {
  // Serve static files from SD card under /src. The root ("/") serves /src/index.html.
  server.serveStatic("/", SD, "/src/").setDefaultFile("index.html");

  // (Optional) Serve raw files in /uploads if needed.
  server.serveStatic("/uploads", SD, "/uploads/");

  // Dedicated futuristic HTML page endpoint (stored as /src/futuristic.html on SD)
  server.on("/futuristic", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/src/futuristic.html", "text/html");
  });

  // File upload endpoint; files get stored in /uploads.
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Upload complete");
  }, handleUpload);

  // File listing endpoint – dynamically lists files in /uploads.
  server.on("/files", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", generateFileList());
  });

  // File download endpoint – forces download with original filename & extension.
  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Bad Request: file parameter missing");
      return;
    }
    String filename = request->getParam("file")->value();
    String path = "/uploads/" + filename;
    if (!SD.exists(path)) {
      request->send(404, "text/plain", "File not found");
      return;
    }
    AsyncWebServerResponse *response = request->beginResponse(SD, path, getContentType(path));
    // This header forces the browser to download the file with the original filename.
    response->addHeader("Content-Disposition", "attachment; filename=" + filename);
    request->send(response);
  });

  // Chat HTML page endpoint (assuming you'll provide chat.html later)
  server.on("/chat", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/src/chat.html", "text/html");
  });

    // New endpoint: GET_ATTRIB_ATTENDANCE - Downloads attendance.csv
  server.on("/GET_ATTRIB_ATTENDANCE", HTTP_GET, [](AsyncWebServerRequest *request) {
    String path = "/attendance/attendance.csv"; 
    if (!SD.exists(path)) {
      request->send(404, "text/plain", "Attendance file not found.");
      return;
    }
    AsyncWebServerResponse *response = request->beginResponse(SD, path, "text/csv");
    response->addHeader("Content-Disposition", "attachment; filename=attendance.csv");
    request->send(response);
  });

  // New endpoint: REM_ATTRIB_ATTENDANCE - Deletes attendance.csv
  server.on("/REM_ATTRIB_ATTENDANCE", HTTP_GET, [](AsyncWebServerRequest *request) {
    String path = "/attendance/attendance.csv";
    if (SD.exists(path)) {
      SD.remove(path);
      // Optionally, recreate a blank attendance.csv file:
      File file = SD.open(path, FILE_WRITE);
      if (file) file.close();
      request->send(200, "text/plain", "Attendance file deleted and reset.");
    } else {
      request->send(404, "text/plain", "Attendance file not found.");
    }
  });


  // Add WebSocket handler
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.begin();
  Serial.println("HTTP server started");
}

// Optional: Minimal RTOS task (ESPAsyncWebServer handles clients asynchronously)
void fileServerTask(void *parameter) {
  for (;;) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);
  // Initialize SD card (make sure wiring & CS pin are set correctly)
  if (!SD.begin()) {
    Serial.println("SD Card Mount Failed");
    digitalWrite(BLUE, HIGH);
    digitalWrite(GREEN, LOW);
    digitalWrite(RED, LOW);
    return;
    
  }
  Serial.println("SD Card mounted successfully.");

  // Create /uploads folder if it doesn't exist.
  if (!SD.exists("/uploads")) {
    SD.mkdir("/uploads");
    Serial.println("Created /uploads folder.");
  }
  delay(1000);
  // Create /attendance folder for attendance CSV logging.
  if (!SD.exists("/attendance")) {
    SD.mkdir("/attendance");
    Serial.println("Created /attendance folder.");
  }
  delay(500);
  if (!SD.exists("/attendance/attendance.csv")) {
    File file = SD.open("/attendance/attendance.csv", FILE_WRITE);
    if (file) {
        file.close(); // Close immediately to create an empty file
    }
  }

  // Initialize SPI for MFRC522 (may share SPI bus with SD; ensure CS pins are distinct)
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("MFRC522 initialized.");

  // Initialize RTC
  rtc.Begin();
  if (!rtc.IsDateTimeValid()) {
    // If RTC lost power or time is not set, set it to the compile time.
    rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
    Serial.println("RTC was not valid, so it was set to compile time.");
  } else {
    Serial.println("RTC is valid.");
  }

  initWiFi();
  setupServer();

  // (Optional) Create an RTOS task for the file server (not strictly necessary)
  xTaskCreatePinnedToCore(
    fileServerTask,   /* Task function */
    "FileServerTask", /* Name of task */
    4096,             /* Stack size */
    NULL,             /* Parameter */
    1,                /* Priority */
    NULL,             /* Task handle */
    1                 /* Core */
  );
}

void loop() {
  // Check for Serial input to set a new name for the next scanned card.
  digitalWrite(RED, LOW);
  digitalWrite(BLUE, HIGH);
  digitalWrite(GREEN, LOW);

  if (Serial.available() > 0) {
    newName = Serial.readStringUntil('\n');
    newName.trim();
    if (newName.length() > 0) {
      Serial.print("New name set via Serial: ");
      Serial.println(newName);
    }
  }

  // Process RFID card scanning
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    // Prepare authentication key (default key)
    MFRC522::MIFARE_Key key;
    for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

    byte block = RFID_DATA_BLOCK;
    byte buffer[18];
    byte size = sizeof(buffer);

    // Authenticate using Key A
    MFRC522::StatusCode status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.print("Authentication failed: ");
      Serial.println(mfrc522.GetStatusCodeName(status));
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      delay(1000);
      return;
    }

    // Read the current data (owner name) from the designated block.
    status = mfrc522.MIFARE_Read(block, buffer, &size);
    String oldName = "";
    if (status == MFRC522::STATUS_OK) {
      char nameBuffer[17];
      memcpy(nameBuffer, buffer, 16);
      nameBuffer[16] = '\0';
      oldName = String(nameBuffer);
      oldName.trim();
    } else {
      Serial.print("Reading failed: ");
      Serial.println(mfrc522.GetStatusCodeName(status));
    }

    // If a new name has been set via Serial, update the card.
    if (newName.length() > 0) {
      // Prepare a 16-byte buffer with the new name (pad with zeros if necessary)
      byte dataBlock[16];
      memset(dataBlock, 0, 16);
      int len = newName.length();
      if (len > 16) len = 16;
      newName.toCharArray((char*)dataBlock, len + 1);

      status = mfrc522.MIFARE_Write(block, dataBlock, 16);
      if (status == MFRC522::STATUS_OK) {
        Serial.print("Card update: Previous Owner: ");
        Serial.print(oldName);
        Serial.print(" | New Owner: ");
        Serial.println(newName);
      } else {
        Serial.print("Writing failed: ");
        Serial.println(mfrc522.GetStatusCodeName(status));
      }
      newName = ""; // Clear after processing
    } else {
      // Otherwise, log attendance by appending the stored name and current timestamp to the CSV file.
      File csvFile = SD.open("/attendance/attendance.csv", FILE_APPEND);
      if (csvFile) {
        // Get current date/time from RTC
        RtcDateTime now = rtc.GetDateTime();
        char dateBuffer[30];
        sprintf(dateBuffer, "%04d-%02d-%02d %02d:%02d:%02d", now.Year(), now.Month(), now.Day(), now.Hour(), now.Minute(), now.Second());
        csvFile.print(oldName);
        csvFile.print(",");
        csvFile.println(dateBuffer);
        csvFile.close();
        Serial.print("Attendance logged for: ");
        Serial.print(oldName);
        Serial.print(" at ");
        Serial.println(dateBuffer);
        digitalWrite(GREEN, HIGH);
        digitalWrite(RED, LOW);
        digitalWrite(BLUE, LOW);
      } else {
        Serial.println("Failed to open attendance CSV file.");
      }
    }
    

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(1000);  // Delay to prevent multiple reads in rapid succession.
  }

  // Nothing else needed in loop; the web server and WebSocket run asynchronously.
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
