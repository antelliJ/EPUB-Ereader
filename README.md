# DIY ESP32 E-reader

taken inspiration from [this project]([url](https://github.com/atomic14/diy-esp32-epub-reader)) by atomic14:
https://github.com/atomic14/diy-esp32-epub-reader

This project uses the arduino framework on PlatformIO, with the graphics library [GXEPD2]([url](https://github.com/ZinggJM/GxEPD2))

Currently no support for images, or CSS content,

Currently all books are loaded from the data folder in LittleFS and manually loaded. Eventually there will be a file loader for SD Card content
  The data folder also currently contains other test files relevant for testing the unzipping, parsing, and rendering process

## How does it work?
Like atomic14's project, this uses miniz to unzip the files into PSRAM

### Parsing:
This is done with tinyXML2

### Rendering:
Done using GXEPD2: https://github.com/ZinggJM/GxEPD2
