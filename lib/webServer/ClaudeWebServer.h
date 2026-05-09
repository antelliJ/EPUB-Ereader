#pragma once

// https://randomnerdtutorials.com/esp32-web-server-beginners-guide/
// https://randomnerdtutorials.com/esp32-mdns-arduino/
// https://randomnerdtutorials.com/esp32-web-server-littlefs/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>

const char *ssid     = "ESP32-Access-Point";
const char *password = "123456789";

// Device reachable at http://ereader.local
const char *mdnsName = "ereader";

// ---------------------------------------------------------------------------
// Minimal inline HTML/CSS served from flash (no LittleFS dependency)
// ---------------------------------------------------------------------------
static const char HTML_HEADER[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="en"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>eReader</title>
<style>
  :root{--bg:#f5f0e8;--card:#fff;--ink:#1a1a1a;--muted:#6b6b6b;--accent:#8b4513;--accent2:#d4a853;--danger:#c0392b;--radius:4px;--mono:'Courier New',monospace}
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--ink);font-family:Georgia,serif;min-height:100vh;padding:2rem 1rem}
  h1{font-size:1.6rem;letter-spacing:.05em;border-bottom:2px solid var(--ink);padding-bottom:.5rem;margin-bottom:1.5rem}
  h2{font-size:1rem;text-transform:uppercase;letter-spacing:.12em;color:var(--muted);margin-bottom:1rem}
  .card{background:var(--card);border:1px solid #ddd;border-radius:var(--radius);padding:1.25rem;margin-bottom:1.5rem;box-shadow:2px 2px 0 #e0d8cc}
  .book-row{display:flex;align-items:center;justify-content:space-between;padding:.6rem 0;border-bottom:1px solid #eee}
  .book-row:last-child{border-bottom:none}
  .book-title{font-size:.95rem;color:var(--ink);flex:1;font-family:var(--mono);overflow:hidden;text-overflow:ellipsis;white-space:nowrap;padding-right:1rem}
  .book-size{font-size:.75rem;color:var(--muted);margin-right:.75rem;white-space:nowrap}
  button{cursor:pointer;border:none;border-radius:var(--radius);padding:.35rem .8rem;font-size:.8rem;font-family:Georgia,serif;transition:opacity .15s}
  button:hover{opacity:.8}
  .btn-delete{background:var(--danger);color:#fff}
  .btn-goto{background:var(--accent);color:#fff}
  .upload-area{border:2px dashed var(--accent2);border-radius:var(--radius);padding:1.5rem;text-align:center;cursor:pointer;transition:background .2s}
  .upload-area.dragover{background:#fdf3e0}
  .upload-area p{color:var(--muted);font-size:.9rem;margin-bottom:.75rem}
  #file-input{display:none}
  .upload-btn{background:var(--accent);color:#fff;padding:.5rem 1.2rem;font-size:.9rem}
  #progress-wrap{display:none;margin-top:1rem}
  progress{width:100%;height:8px;border-radius:4px;appearance:none}
  progress::-webkit-progress-bar{background:#eee;border-radius:4px}
  progress::-webkit-progress-value{background:var(--accent);border-radius:4px}
  #status{font-size:.85rem;color:var(--muted);margin-top:.5rem;font-family:var(--mono)}
  .empty{color:var(--muted);font-size:.9rem;font-style:italic;padding:.5rem 0}
</style>
</head><body>
<h1>📖 eReader</h1>
)rawhtml";

static const char HTML_FOOTER[] PROGMEM = R"rawhtml(
</body></html>
)rawhtml";

// ---------------------------------------------------------------------------
// Upload form (appended after the book list card)
// ---------------------------------------------------------------------------
static const char HTML_UPLOAD[] PROGMEM = R"rawhtml(
<div class="card">
  <h2>Upload Book</h2>
  <div class="upload-area" id="drop-zone">
    <p>Drop an .epub file here, or</p>
    <input type="file" id="file-input" accept=".epub">
    <button class="upload-btn" onclick="document.getElementById('file-input').click()">Choose file</button>
  </div>
  <div id="progress-wrap">
    <progress id="progress" value="0" max="100"></progress>
    <div id="status">Uploading…</div>
  </div>
</div>
<script>
const dropZone = document.getElementById('drop-zone');
const fileInput = document.getElementById('file-input');

dropZone.addEventListener('dragover', e => { e.preventDefault(); dropZone.classList.add('dragover'); });
dropZone.addEventListener('dragleave', () => dropZone.classList.remove('dragover'));
dropZone.addEventListener('drop', e => { e.preventDefault(); dropZone.classList.remove('dragover'); uploadFile(e.dataTransfer.files[0]); });
fileInput.addEventListener('change', () => { if (fileInput.files[0]) uploadFile(fileInput.files[0]); });

function uploadFile(file) {
  if (!file || !file.name.endsWith('.epub')) { alert('Please select an .epub file'); return; }
  const wrap = document.getElementById('progress-wrap');
  const bar  = document.getElementById('progress');
  const stat = document.getElementById('status');
  wrap.style.display = 'block';
  stat.textContent = 'Uploading ' + file.name + '…';

  const formData = new FormData();
  formData.append('file', file, file.name);
  const xhr = new XMLHttpRequest();
  xhr.upload.addEventListener('progress', e => {
    if (e.lengthComputable) { bar.value = Math.round(e.loaded / e.total * 100); }
  });
  xhr.addEventListener('load', () => {
    if (xhr.status === 200) { stat.textContent = '✓ Upload complete. Reloading…'; setTimeout(() => location.reload(), 1500); }
    else { stat.textContent = '✗ Upload failed (' + xhr.status + ')'; }
  });
  xhr.addEventListener('error', () => { stat.textContent = '✗ Network error during upload'; });
  xhr.open('POST', '/upload');
  xhr.setRequestHeader('X-Filename', encodeURIComponent(file.name));
  xhr.send(formData);
  
}
</script>
)rawhtml";



// ---------------------------------------------------------------------------

class webServer {
private:
  static WebServer *server;
  static File       currentFile;

  // ── Handlers ──────────────────────────────────────────────────────────────

  static void handleRoot() {
    // Redirect root to the book list
    server->sendHeader("Location", "/books");
    server->send(302);
  }

  // GET /books — HTML page listing all epubs in LittleFS
  static void handleBookList() {
    String html = FPSTR(HTML_HEADER);
    html += F("<div class='card'><h2>Your Library</h2>");

    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) {
      html += F("<p class='empty'>Could not open filesystem.</p>");
    } else {
      bool found = false;
      File f = root.openNextFile();
      while (f) {
        String name = String(f.name());
        if (name.endsWith(".epub")) {
          found = true;
          size_t sz = f.size();
          String sizeStr;
          if (sz > 1024 * 1024)
            sizeStr = String(sz / 1024 / 1024.0f, 1) + " MB";
          else
            sizeStr = String(sz / 1024) + " KB";

          html += F("<div class='book-row'>");
          html += "<span class='book-title'>" + name + "</span>";
          html += "<span class='book-size'>" + sizeStr + "</span>";
          // Delete button — posts to /delete?file=<name>
          html += "<button class='btn-delete' onclick=\"deleteBook('" + name + "')\">Delete</button>";
          html += F("</div>");
        }
        f.close();
        f = root.openNextFile();
      }
      if (!found) {
        html += F("<p class='empty'>No books found. Upload one below.</p>");
      }
    }
    html += F("</div>");

    // Delete helper script
    html += F("<script>"
              "function deleteBook(name){"
              "if(!confirm('Delete '+name+'?'))return;"
              "fetch('/delete?file='+encodeURIComponent(name),{method:'POST'})"
              ".then(r=>r.text()).then(()=>location.reload())"
              ".catch(e=>alert('Delete failed: '+e));}"
              "</script>");

    html += FPSTR(HTML_UPLOAD);
    html += FPSTR(HTML_FOOTER);

    server->send(200, "text/html", html);
  }

  // POST /delete?file=<name>
  static void handleBookDelete() {
    if (!server->hasArg("file")) {
      server->send(400, "text/plain", "Missing file parameter");
      return;
    }
    String filename = "/" + server->arg("file");
    // Sanitise: must end in .epub and not contain path traversal
    if (!filename.endsWith(".epub") || filename.indexOf("..") >= 0) {
      server->send(400, "text/plain", "Invalid filename");
      return;
    }
    if (!LittleFS.exists(filename)) {
      server->send(404, "text/plain", "File not found");
      return;
    }
    if (LittleFS.remove(filename)) {
      server->send(200, "text/plain", "Deleted");
    } else {
      server->send(500, "text/plain", "Delete failed");
    }
  }

  // POST /upload — raw body upload, filename from X-Filename header
  // The JS sends the raw file bytes with XHR so we can track progress;
  // WebServer buffers the body, then this handler is called once.
  static void handleUpload() {
    // Retrieve filename from custom header (URL-encoded)
    String rawName = server->header("X-Filename");
    if (rawName.length() == 0) {
      server->send(400, "text/plain", "Missing X-Filename header");
      return;
    }

    // Simple URL-decode (handle %XX sequences)
    String filename = urlDecode(rawName);

    // Sanitise
    if (!filename.endsWith(".epub") || filename.indexOf("..") >= 0 || filename.indexOf("/") >= 0) {
      server->send(400, "text/plain", "Invalid filename");
      return;
    }

    String path = "/" + filename;

    // WebServer has the body in server->arg("plain") for raw POST
    // but for large files we use the streaming upload callback instead.
    // This handler is only reached after handleUploadBody has written everything.
    server->send(200, "text/plain", "OK: " + path);
  }

  // Streaming upload body handler (passed as 4th arg to server->on)
  // Called repeatedly as chunks arrive.
  static void handleUploadBody() {
    HTTPUpload &upload = server->upload();

    if (upload.status == UPLOAD_FILE_START) {
      currentFile.close(); // close any previous file (shouldn't be one, but just in case)
      String filename = urlDecode(upload.filename);
      if (!filename.endsWith(".epub") || filename.indexOf("..") >= 0) {
        Serial.println("[webServer] Rejected upload: bad filename");
        return;
      }
      String path = "/" + filename;
      Serial.printf("[webServer] Upload start: %s\n", path.c_str());
      if (LittleFS.exists(path)) {
        LittleFS.remove(path); // overwrite existing
        delay(10);
      }
      currentFile = LittleFS.open(path, FILE_WRITE);
      if (!currentFile) {
        Serial.println("[webServer] Failed to open file for writing");
      }

    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (currentFile) {
        size_t written = currentFile.write(upload.buf, upload.currentSize);
        if (written != upload.currentSize) {
          Serial.printf("[webServer] Write mismatch: %u vs %u\n", written, upload.currentSize);
        }
      }

    } else if (upload.status == UPLOAD_FILE_END) {
      if (currentFile) {
        currentFile.close();
        Serial.printf("[webServer] Upload complete: %u bytes\n", upload.totalSize);
      } else {
        Serial.println("[webServer] Upload ended but file was not open");
      }

    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      if (currentFile) {
        currentFile.close();
        // Remove partial file
        String path = "/" + upload.filename;
        LittleFS.remove(path);
        Serial.println("[webServer] Upload aborted, partial file removed");
      }
    }
  }

  // GET /goto?file=<name>&page=<n>
  // Stores the jump target so the main loop can act on it.
  // Returns JSON so the caller can confirm.
  static void handleBookGoto() {
    if (!server->hasArg("file")) {
      server->send(400, "application/json", "{\"error\":\"missing file\"}");
      return;
    }
    String file = "/" + server->arg("file");
    int    page = server->hasArg("page") ? server->arg("page").toInt() : 0;

    if (!LittleFS.exists(file)) {
      server->send(404, "application/json", "{\"error\":\"file not found\"}");
      return;
    }

    // Store request in static state so the main loop can pick it up.
    s_goto_file = file;
    s_goto_page = page;
    s_goto_pending = true;

    String json = "{\"ok\":true,\"file\":\"" + file + "\",\"page\":" + String(page) + "}";
    server->send(200, "application/json", json);
  }

  // GET /api/status — basic info (free heap, filesystem usage)
  // static void handleApiStatus() {
  //   size_t total = 0, used = 0;
  //   LittleFS.info(/* FSInfo */ *(new fs::FSInfo())); // placeholder — see note below
  //   // Simpler: just report heap
  //   String json = "{\"heap\":" + String(ESP.getFreeHeap()) +
  //                 ",\"psram\":" + String(ESP.getFreePsram()) + "}";
  //   server->send(200, "application/json", json);
  // }

  // Simple URL decoder for %XX sequences
  static String urlDecode(const String &src) {
    String decoded;
    decoded.reserve(src.length());
    for (size_t i = 0; i < src.length(); i++) {
      if (src[i] == '%' && i + 2 < src.length()) {
        char buf[3] = { src[i + 1], src[i + 2], 0 };
        decoded += (char)strtol(buf, nullptr, 16);
        i += 2;
      } else if (src[i] == '+') {
        decoded += ' ';
      } else {
        decoded += src[i];
      }
    }
    return decoded;
  }

public:
  // ── Pending goto request (poll from main loop) ────────────────────────────
  static String  s_goto_file;
  static int     s_goto_page;
  static bool    s_goto_pending;

  webServer() {}

  static void startWebServer(uint16_t port = 80) {
    if (server) {
      server->stop();
      delete server;
    }
    server = new WebServer(port);

    // ── Wi-Fi AP ─────────────────────────────────────────────────────────
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);

    IPAddress ip = WiFi.softAPIP();
    Serial.print("[webServer] AP IP: ");
    Serial.println(ip);

    // ── mDNS ─────────────────────────────────────────────────────────────
    if (!MDNS.begin(mdnsName)) {
      Serial.println("[webServer] mDNS failed — continuing without it");
    } else {
      MDNS.addService("http", "tcp", port); // no underscores
      Serial.printf("[webServer] mDNS: http://%s.local\n", mdnsName);
    }

    // ── Routes ───────────────────────────────────────────────────────────
    server->on("/",       HTTP_GET,  webServer::handleRoot);
    server->on("/books",  HTTP_GET,  webServer::handleBookList);
    server->on("/delete", HTTP_POST, webServer::handleBookDelete);
    server->on("/goto",   HTTP_GET,  webServer::handleBookGoto);

    // Upload: completion handler + streaming body handler
    server->on("/upload", HTTP_POST,
      webServer::handleUpload,       // called once after body is complete
      webServer::handleUploadBody    // called repeatedly with each chunk
    );

    // Collect the X-Filename header
    const char *headerNames[] = { "X-Filename" };
    server->collectHeaders(headerNames, 1);

    server->begin();
    Serial.printf("[webServer] Listening on port %u\n", port);
  }

  void stopWebServer() {
    if (server) {
      server->stop();
      delete server;
      server = nullptr;
    }
    MDNS.end();
    WiFi.softAPdisconnect(true);
    Serial.println("[webServer] Stopped");
  }

  void handleClient() {
    if (server) {
      server->handleClient();
      // MDNS.update(); // keep mDNS alive
    }
  }

  // Call from main loop to check for a pending goto request
  bool hasPendingGoto() const { return s_goto_pending; }
  String getPendingFile() const { return s_goto_file; }
  int    getPendingPage() const { return s_goto_page; }
  void   clearPendingGoto()    { s_goto_pending = false; s_goto_file = ""; s_goto_page = 0; }
};

// ── Static member definitions ─────────────────────────────────────────────────
WebServer *webServer::server       = nullptr;
File       webServer::currentFile;
String     webServer::s_goto_file  = "";
int        webServer::s_goto_page  = 0;
bool       webServer::s_goto_pending = false;
