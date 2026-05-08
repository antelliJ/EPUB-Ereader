//https://randomnerdtutorials.com/esp32-web-server-beginners-guide/
// https://randomnerdtutorials.com/esp32-mdns-arduino/
// https://randomnerdtutorials.com/esp32-web-server-littlefs/

#include <Arduino.h>

#include <Wifi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

const char *ssid = "ESP32-Access-Point";
const char *password = "123456789";

class webServer {
    private:
        static WebServer* server;

        // on method likes when its static
        static void handleRoot() {
            String html = "<h1>ESP32 Web Server</h1><p>You are connected</p>";
            server->send(200, "text/html", html);
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
