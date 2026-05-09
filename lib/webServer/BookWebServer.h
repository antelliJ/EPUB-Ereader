//https://randomnerdtutorials.com/esp32-web-server-beginners-guide/
// https://randomnerdtutorials.com/esp32-mdns-arduino/
// https://randomnerdtutorials.com/esp32-web-server-littlefs/

#include <Arduino.h>

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>

const char *ssid = "ESP32-Access-Point";
const char *password = "123456789";

// Device is available at: http://ereader.local
const char* mdnsName = "ereader";




// inline HTML
// ----------------------------------------------------------------------------------------
static const char HTML_UPLOAD[] PROGMEM = R"rawhtml(
<div class='card'>
    <h2>Upload Book</h2>
    <div class="upload-area" id='drop-zone'>
        <p>Drag and drop your book here or click to select a file.</p>
        <input type="file" id="file-input" accept=".epub">
        <button class="upload-btn" onclick="document.getElementById('file-input').click()">Choose File</button>
    </div>
    <div id='upload-status'>
        <progress id='progress' value='0' max='100'></progress>
        <div id='status'>Uploading...</div>
    </div>
</div>

)rawhtml";



// ----------------------------------------------------------------------------------------


class webServer {
    private:
        static WebServer* server;
        static File currentFile; // Keep track of the currently open file during upload

        // on method likes when its static
        static void handleRoot() {
            String html = "<h1>ESP32 Web Server</h1><p>You are connected</p>";
            server->send(200, "text/html", html);
        }

        // return the list of books (options to delete and upload new books)
        static void handleBookList();
        // jump to a page in current selected book (will open EpubReader?)
        static void handleBookGoto();
        // Function for uploading files
        static void handleWebUpload() {
            HTTPUpload& upload = server->upload();
            if (upload.status == UPLOAD_FILE_START) {
                Serial.printf("UploadStart: %s\n", upload.filename.c_str());
                String filename = "/littlefs/" + upload.filename;
                // open the file for writing in LittleFS
                currentFile = LittleFS.open(filename, FILE_WRITE);
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                Serial.printf("UploadWrite: %d bytes\n", upload.currentSize);
                if (currentFile) {
                    currentFile.write(upload.buf, upload.currentSize);
                }
            } else if (upload.status == UPLOAD_FILE_END) {
                Serial.printf("UploadEnd: %d bytes\n", upload.totalSize);
                if (currentFile) {
                    currentFile.close();
                }
            }
        }
    public:
        webServer();
        // void startWebServer();
        void stopWebServer();
        static void startWebServer(uint16_t port = 80) {
            if (server) {
                delete server;
            }

            server = new WebServer(port);

            WiFi.mode(WIFI_AP);
            WiFi.softAP(ssid, password);

            Serial.println("Access Point Started");
            IPAddress IP = WiFi.softAPIP();
            Serial.print("AP IP address: ");
            Serial.println(IP);

            // Set mDNS
            if (!MDNS.begin(mdnsName)) {
                Serial.println("Error setting up MDNS responder!");
                while(1) {
                delay(1000);
                }
            }

            MDNS.addService("_http", "_tcp", 80);


            server->on("/", webServer::handleRoot);

            server->begin();
            Serial.println("Web server started");
        }

        void handleClient() {
            if (server) {
                server->handleClient();
            }
        }
};
