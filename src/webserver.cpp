#include <ESP8266WebServer.h>
#include "pixelstick.h"

bool handleFileRead(String path); // send the right file to the client (if it exists)
void handleFileUpload();
void handleResult();

// void handleBrowseWifi();
int uploadSize;

String getContentType(String filename);

ESP8266WebServer server(80); // Create a webserver object that listens for HTTP request on port 80

void initWebserver()
{
  // Handle file upload
  // The first callback is called after the request has ended
  // The second callback handles the file upload
  server.on(
      "/upload", HTTP_POST, // When the browser posts to the page
      []() {
        handleResult(); // Tell the user we've uploaded the file and how big it was
      },
      handleFileUpload // Receive and save the file
  );

  server.onNotFound([]() {                              // If the client requests any URI
    if (!handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });

  server.serveStatic("/", LittleFS, "/index.html");                             // , "max-age=3600");
  server.serveStatic("/index.htm", LittleFS, "/index.html");                    // , "max-age=3600");
  server.serveStatic("/index.html", LittleFS, "/index.html");                   // , "max-age=3600");
  server.serveStatic("/config.json", LittleFS, "/config.json");                 // , "max-age=3600");
  server.serveStatic("/favicon.ico", LittleFS, "/favicon.ico");                 // , "max-age=3600");
  server.serveStatic("/css/pixelstick.css", LittleFS, "/css/pixelstick.css");   // , "max-age=3600");
  server.serveStatic("/js/pixelstick.js", LittleFS, "/js/pixelstick.js");       // , "max-age=3600");
  server.serveStatic("/images/batt-000.png", LittleFS, "/images/batt-000.png"); // , "max-age=3600");
  server.serveStatic("/images/batt-025.png", LittleFS, "/images/batt-025.png"); // , "max-age=3600");
  server.serveStatic("/images/batt-050.png", LittleFS, "/images/batt-050.png"); // , "max-age=3600");
  server.serveStatic("/images/batt-075.png", LittleFS, "/images/batt-075.png"); // , "max-age=3600");
  server.serveStatic("/images/batt-100.png", LittleFS, "/images/batt-100.png"); // , "max-age=3600");
  server.serveStatic("/images/menu.png", LittleFS, "/images/menu.png");         // , "max-age=3600");
  server.serveStatic("/images/x.png", LittleFS, "/images/x.png");               // , "max-age=3600");
  server.begin();                                                               // Start the server
  Serial.println(F("HTTP server started"));
}

void serviceClient()
{
  server.handleClient();
}

bool handleFileRead(String path)
{ // Send the right file to the client (if it exists)
  // Serial.println("handleFileRead: " + path);
  if (path.endsWith("/"))
    path += "index.html";                    // If a folder is requested, send the index file
  String contentType = getContentType(path); // Get the MIME type
  String pathWithGz = path + ".gz";
  if (LittleFS.exists(pathWithGz) || LittleFS.exists(path))
  {                                       // If the file exists, either as a compressed archive, or normal
    if (LittleFS.exists(pathWithGz))      // If there's a compressed version available
      path += ".gz";                      // Use the compressed verion
    File file = LittleFS.open(path, "r"); // Open the file
    server.streamFile(file, contentType); // Send it to the client

    file.close(); // Close the file again
    // Serial.println(String("\tSent file: ") + path);
    // Serial.println(String("\tSent size: ") + sent);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path); // If the file doesn't exist, return false
  return false;
}

String getContentType(String filename)
{ // convert the file extension to the MIME type
  if (filename.endsWith(".html"))
    return "text/html";
  else if (filename.endsWith(".htm"))
    return "text/html";
  else if (filename.endsWith(".css"))
    return "text/css";
  else if (filename.endsWith(".js"))
    return "application/javascript";
  else if (filename.endsWith(".png"))
    return "image/png";
  else if (filename.endsWith(".gif"))
    return "image/gif";
  else if (filename.endsWith(".jpg"))
    return "image/jpeg";
  else if (filename.endsWith(".ico"))
    return "image/x-icon";
  else if (filename.endsWith(".bmp"))
    return "image/bmp";
  else if (filename.endsWith(".xml"))
    return "text/xml";
  else if (filename.endsWith(".pdf"))
    return "application/x-pdf";
  else if (filename.endsWith(".zip"))
    return "application/x-zip";
  else if (filename.endsWith(".gz"))
    return "application/x-gzip";
  return "text/plain";
}

void handleResult()
{
  if (uploadSize != 0)
  {
    String s = F("<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\"");
    s += F("content=\"width=device-width, initial-scale=1.0, user-scalable=no\"/>");
    s += F("<title>SJR-PixelStick</title>");
    s += F("<style>body{text-align: center;font-family:verdana;}</style>");
    s += F("</head><body><div style=\"text-align:left;display:inline-block;min-width:260px;\">");
    s += F("<p><a href=\"/\">&lt;Back</a></p>");
    s += F("<h3>File Upload Successful</h3><p>");
    s += uploadSize;
    s += F(" bytes transferred</p>");
    s += F("</div></body></html>");
    server.send(200, "text/html", s);
  }
  else
  {
    server.send(500, "text/plain", "Error: Problem uploading the file");
  }
}

void handleFileUpload() // upload a new file to the LittleFS
{
  static File fsUploadFile; // Has to be static as there are repeated calls to this function for upload
  String filename;
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START)
  {
    uploadSize = 0;
    filename = upload.filename;
    if (!filename.startsWith("/bmp/"))
      filename = "/bmp/" + filename;
    fsUploadFile = LittleFS.open(filename, "w"); // Open the file for writing in LittleFS (create if it doesn't exist)
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (fsUploadFile)
    {
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (fsUploadFile)
    {                       // If the file was successfully created
      fsUploadFile.close(); // Close the file
      uploadSize = upload.totalSize;
    }
  }
}