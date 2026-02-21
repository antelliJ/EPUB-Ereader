#include "ZipFile.h"
#include "Arduino.h"

/**
 * @brief Prints the contents of a chapter from an EPUB to the serial monitor.
 *
 * @param epubPath The path to the EPUB file.
 * @param internalFile The path to the chapter file inside the EPUB.
 */
void printChapterToSerial(const char* epubPath, const char* internalFile) {
    ZipFile ZipFile(epubPath);
    
    size_t fileSize = 0;
    uint8_t *fileData = ZipFile.read_file_to_memory(internalFile, &fileSize);

    if (fileData != nullptr) {
        Serial.println("Chapter Content Start:");

        // read_file_to_memory uses calloc(size+1), is null-terminated, so we can print it as a string safely
        Serial.println((char*)fileData);

        Serial.println("Chapter Content End.");

        // free(fileData);
        ZipFile::free_file_memory(fileData);
    } else {
        Serial.println("Failed to read chapter from EPUB.");
    }
    vTaskDelete(NULL); // Delete the task when done
}