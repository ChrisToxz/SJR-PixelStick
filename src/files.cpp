#include "pixelstick.h"

FileInfo currentFile;

uint16_t read16(File &f) // Read a 2-byte int from the file (little-endian)
{
  uint16_t result;
  f.read((uint8_t *)&result, 2);
  return result;
}

uint32_t read32(File &f) // Read a 4-byte int from the file (little-endian)
{
  uint32_t result;
  f.read((uint8_t *)&result, 4);
  return result;
}

String getBMPList()
{
  Dir dir = LittleFS.openDir("/bmp"); // Get object to iterate over all files in /bmp
  String s = "";

  while (dir.next())
  {
    if (!dir.fileName().endsWith(".bmp"))
      continue;
    if (s.length() > 0)
      s += ":";
    s += dir.fileName(); 
  }
  return s;
}

void getBmpInfo(char *path)
{
  File bmpFile; // The file we want info for

  strlcpy(currentFile.path, path, sizeof(currentFile.path));
  currentFile.status = VALID; // Assume the file is good

  if (!(bmpFile = LittleFS.open(path, "r"))) // Open the file
  {
    Serial.print(F("Error opening file: "));
    Serial.println(path);
    currentFile.status = OPEN_ERROR;
    return;
  }

  // Parse BMP header to get the information we need
  if (read16(bmpFile) == 0x4D42) // BMP file signature ("BM") check
  {
    currentFile.fileSize = read32(bmpFile);       // Size of file
    read32(bmpFile);                              // Discard the four reserved bytes
    currentFile.bmpImageoffset = read32(bmpFile); // Start of image data
    // In very old Windows BMP files, the DiB header size is 12 bytes and the width and height values
    // are 16-bit signed integers (OS/2 BMP unsigned). Most Windows BMP files have a 40-byte DIB header
    // and 32-bit signed integers for width and height - we are assuming this at the moment.
    if (int i = read32(bmpFile) != 40)
    { // DIB Header size
      Serial.print(F("Unexpected BMP header size: "));
      Serial.println(i);
    }
    currentFile.bmpWidth = read32(bmpFile);  // Image width (implicit cast to u_int16t)
    currentFile.bmpHeight = read32(bmpFile); // Image height (implicit cast to u_int16t)

    currentFile.planeCount = read16(bmpFile); // No. of planes in the image
    currentFile.bitDepth = read16(bmpFile);   // Bit depth of the image
    currentFile.compMethod = read32(bmpFile); // Compression method
    bmpFile.close();
    if (currentFile.planeCount != 1)
      currentFile.status |= BAD_PLANES;
    if (currentFile.bitDepth != 24)
      currentFile.status |= BAD_BITDEPTH;
    if (currentFile.compMethod != 0)
      currentFile.status |= BAD_COMPRESSION;
  }
  else
  {
    currentFile.status = BAD_SIGNATURE;
    Serial.print(F("Invalid signature: "));
    Serial.println(path);
  }
}

// Returns BMP file info as a single string
String getBMPInfoString(char *filename)
{
  String s = "";
  
  getBmpInfo(filename);
  if (currentFile.status == VALID)
  {
    s += String(currentFile.bmpWidth) + ":";
    s += String(currentFile.bmpHeight);
  }
  else
  {
    s = (F("?Bitmap file error: "));
    s += String(currentFile.status);
  }
  return s;
}

String getSystemInfo()
{
  FSInfo fsinfo;
  String s = "";
  int filecount = 0;

  LittleFS.info(fsinfo);
  s += F("File system total capacity: ");
  s += fsinfo.totalBytes;
  s += F(" bytes<br>File system capacity used: ");
  s += fsinfo.usedBytes;
  s += F(" bytes (");
  s += fsinfo.totalBytes - fsinfo.usedBytes;
  s += F(" bytes ");
  s += 100 * (fsinfo.totalBytes - fsinfo.usedBytes) / fsinfo.totalBytes;
  s += F("% free)<br><br>");
  s += F("<table style=\"width:100%\">");
  s += F("<tr><th style=\"width:70%;text-align:left\">Bitmap files</th><th style=\"width:30%;text-align:right\">File size</th></tr>");
  Dir dir = LittleFS.openDir("/bmp");
  while (dir.next())
  {
    s += F("<tr>");
    s += F("<td>");
    s += dir.fileName();
    s += F("</td><td style=\"text-align:right\">");
    s += dir.fileSize();
    s += F("</td></tr>");
    filecount++;
  }
  s += F("</table><p>Total files: ");
  s += filecount;
  return s;
}