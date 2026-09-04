# DIY ESP32 E-reader
https://github.com/antelliJ/EPUB-Ereader

A fairly simple E reader built using the ESP32-S3 devboard, storing epub files in the flash (can be modified with wifi) and displaying the output to an epaper display

taken inspiration from [this project]([url](https://github.com/atomic14/diy-esp32-epub-reader)) by atomic14:
https://github.com/atomic14/diy-esp32-epub-reader
^ This makes use of the espressif framework.

This project uses the arduino framework on PlatformIO, with the graphics library [GXEPD2]([url](https://github.com/ZinggJM/GxEPD2))

Currently no support for PNG images, progressive JPEG images, or CSS content.

Currently all books are loaded from the data folder in LittleFS and manually loaded. Eventually there will be a file loader for SD Card content
  The data folder also currently contains other test files relevant for testing the unzipping, parsing, and rendering process

There is an option to enter a webserver hosted by the ESP.
It is made accessible using an mdns address, which is ereader.local once connected to the ESP's access point
this allows for managing the EPUB library (upload and delete files, view how much of the flash storage is used), the ability to view bookmarks of a given book, and make the device turn to a particular page

The table of contents is similarly implemented to atomic14's project (but I haven't tested it in a while)

The ereader can save the current page manually, and bookmark pages. Once finished a session, the user can clear the display for longevity.

## How does it work?
Like atomic14's project, this uses miniz to unzip the files into PSRAM

### Parsing:
This is done with tinyXML2

JPEG images are decoded using JPEGDEC, with a toggle for dithered rendering

### Rendering:
Done using GXEPD2: https://github.com/ZinggJM/GxEPD2
