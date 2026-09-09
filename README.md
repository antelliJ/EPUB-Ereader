# DIY ESP32 E-reader
https://github.com/antelliJ/EPUB-Ereader

A fairly simple diy E reader built using the ESP32-S3 devboard, storing epub files in the flash (can be modified with wifi) and displaying the output to an epaper display.

![Shot of e-reader](<assets/MainDevice.png>)

It actually parses and unzips the zip files and the xhtml of the epubs

taken inspiration from [this project]([url](https://github.com/atomic14/diy-esp32-epub-reader)) by atomic14:
https://github.com/atomic14/diy-esp32-epub-reader
^ This makes use of the espressif framework.

## Quick Start
1. You can flash the latest firmware from the [Releases page](https://github.com/antelliJ/EPUB-Ereader/releases)
2. Flash it with esptool after connecting the microcontroller

(Make sure you have python installed)

```bash
pip install esptool

python -m esptool --chip esp32s3 --port COMx erase_flash

python -m esptool --chip esp32s3 --port COMx write_flash 0x0 epub-ereader.bin
```


<strong> Libraries used: </strong>
- [GXEPD2]([url](https://github.com/ZinggJM/GxEPD2)) - e-paper display driver (supports so many displays its actually crazy)
- [Adafruit GFX Library] (https://github.com/adafruit/Adafruit-GFX-Library) - gxepd2 uses this under the hood, but its methods are quite important
- [miniz](https://github.com/richgel999/miniz "Github Project") - zip (EPUB container) reading
- [tinyxml2](https://github.com/leethomason/tinyxml2 "Github Project") - for XHTML and NCX file parsing
- LittleFS (bundled with the Arduino-Esp32 core) - used for book and bookmark storage on the internal flash

This project uses the arduino framework on PlatformIO

## Features
- [x] Proper Epub file parsing - unzips and parses the XHTML of .Epubs, not only rendering `.txt` files
- [x] PSRAM centered architecture - buffers for decompression make use of the PSRAM so long chapters don't break the stack
- [x] table of content navigation - easy skipping through the various chapters
- [x] Bookmark web export - get a plain text version of your favorite pages wirelessly
- [x] WIFI book management - open the self hosted web server to manage you books (upload, delete)

- [x] Reuses an old DVD remote for navigation
- [ ] Currently no support for PNG images, progressive JPEG images, or CSS content (Generally rare in Epubs though).

Potential future features:
- [ ] deep sleep implementation to save even more power
- [ ] SD card reading for expanded storage and as another method to upload content
- [ ] rendering the cover image on book list


## Usage
### Reading books
Books on the file system are shown on boot in the book list. Opening will open the book to its last saved page.
The books live on the `spiffs` data partition, with any file ending in the `.epub` extension in the root directory being added to the book list. Bookmarks live in the `/bookmarks` folder of the file system using a proprietary .bmk filetype (its just a text file with the reference for each bookmark), and located by hash

### Schematic?
*Coming soon*

The connections I used:
| Signal          |     | ESP32-S3 GPIO |
| --------------- | --- | ------------- |
| EPD MOSI        |     | GPIO 11       |
| EPD SCK         |     | GPIO 12       |
| EPD CS          |     | GPIO 10       |
| EPD DC          |     | GPIO 17       |
| EPD RST         |     | GPIO 16       |
| EPD BUSY        |     | GPIO 4        |
|                 |     |               |
| Remote IN1      |     | GPIO 38       |
| Remote IN2      |     | GPIO 39       |
| Remote IN3      |     | GPIO 40       |
| Remote IND      |     | GPIO 21       |
| Remote OUT1     |     | GPIO 42       |
| Remote OUT2     |     | GPIO 2        |
| Remote OUT3     |     | GPIO 1        |

The remote uses its own multiplexing system, if you'd like to implement your own version
then create a function that returns a valid `UIAction` (ex. Up, Down, Menu, etc) that is set in the `loop()` method. This is then passed into `handleUserInteraction()`

| Remote Input    |     | UIAction      |What it does    |
| --------------- | --- | ------------- |----|
| Setup        |     | SETUP      | Change dither mode of images   |
| Options         |     | OPTIONS       | Open table of contents    |
| Audio         |     | FORCE_FULL_REFRESH        | make display refresh (treat ghosting)   |
| Subtitle         |     | BOOKMARK       | If on the booklist: Starts the webserver, If Reading a book: toggles bookmark on that page   |
| Disc Menu         |     | MENU       | If reading a book: goes to the book list, If in the webserver: stops server and restarts device    |
| Stop         |     | SAVE       |If in booklist: 'saves' the screen quality by forcing the screen to clear and refresh, preparring for shutdown. If reading, saves the current page progress into the book's bookmark file     |
| <<         |     | REWIND       | Skips back to last chapter of current book    |
| >>         |     | FAST_FORWARD       | Skips forward to next chapter of current book    |
| Up/Left         |     | UP       | If reading: Go to next page. If on booklist: go to next selection. If on table of contents: go to next selection    |
| Down/Right         |     | DOWN       | If reading: Go to previous page. If on booklist: go to previous selection. If on table of contents: go to previous selection    |
| Ok         |     | SELECT       | If in booklist: open book. If reading: sets the image fit mode (from fit to full size). If in table of contents: opens that chapter    |

### Book & Bookmark access
There is an option to enter a webserver hosted by the ESP.
Join the access point: "ESP32-Access-Point" with the password: "123456789"

It is made accessible using an mdns address, which is `ereader.local` once connected to the ESP's access point
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
