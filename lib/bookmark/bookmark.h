#include <Arduino.h>
#include "LittleFS.h"
#include <vector>

#include "EpubReader.h"

#ifndef BOOKMARK_H
#define BOOKMARK_H


#define BOOKMARKS_DIR "/bookmarks"
#define FORMAT_VERSION 1

struct BookmarkData {
    uint16_t lastPage = 0;
    std::vector<uint16_t> bookmarks;
};

uint32_t hashPath(const String& path) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < path.length(); ++i) {
        hash = ((hash << 5) + hash) + path[i]; // hash * 33 + c -> djb2 algorithm
    }
    return hash;
}

String getBookmarkFilePath(const String& epubPath) {
    uint32_t hash = hashPath(epubPath);
    return String(BOOKMARKS_DIR) + "/" + String(hash, HEX) + ".bmk";
}

class BookmarkManager {
private:
    // static WebServer* server; // Static pointer to the web server for handling bookmark requests
    static String currentEpubPath; // Static variable to keep track of the current EPUB path for bookmark operations
    static String currentEpubTitle; // Static variable to keep track of the current EPUB title for bookmark operations
public:
    static void init() {
        if (!LittleFS.begin()) {
            Serial.println("An Error has occurred while mounting LittleFS");
            return;
        }

        if (!LittleFS.exists(BOOKMARKS_DIR)) {
            LittleFS.mkdir(BOOKMARKS_DIR);
        }
    }

    static bool saveBookmark(const String& epubPath, const BookmarkData& data) {
        String filePath = getBookmarkFilePath(epubPath);

        File file = LittleFS.open(filePath, FILE_WRITE);
        if (!file) {
            Serial.println("Failed to open bookmark file for writing");
            return false;
        }

        // Write header with format version
        file.write((uint8_t)FORMAT_VERSION);

        // write last page
        file.write((uint8_t*)&data.lastPage, sizeof(data.lastPage));

        // write number of bookmarks
        uint8_t numBookmarks = min((int)data.bookmarks.size(), 255);
        file.write(numBookmarks);

        // write bookmarks
        for (uint16_t page : data.bookmarks) {
            file.write((uint8_t*)&page, sizeof(page));
        }


        file.close();
        return true;
    }

    static bool loadBookmark(const String& epubPath, BookmarkData& outData) {
        String filePath = getBookmarkFilePath(epubPath);

        File file = LittleFS.open(filePath, FILE_READ);
        if (!file) {
            Serial.println("No bookmark file found, starting fresh");
            return false; // No bookmark file, not an error
        }

        // read and check format version
        uint8_t version = file.read();
        if (version != FORMAT_VERSION) {
            Serial.println("Bookmark file format version mismatch, ignoring");
            file.close();
            return false; // Version mismatch, ignore bookmark
        }

        // read last page
        file.read((uint8_t*)&outData, sizeof(outData.lastPage));

        // read number of bookmarks
        uint8_t numBookmarks = file.read();

        outData.bookmarks.clear();
        for (uint8_t i=0; i < numBookmarks; i++) {
            uint16_t page;
            file.read((uint8_t*)&page, sizeof(page));
            outData.bookmarks.push_back(page);
        }

        file.close();
        return true;
    }

    static bool toggleBookmark(BookmarkData& data, uint16_t page) { // false for remove, true for add
        for (size_t i = 0; i < data.bookmarks.size(); i++) {
            if (data.bookmarks[i] == page) {
                data.bookmarks.erase(data.bookmarks.begin() + i);
                Serial.printf("Removed bookmark for page %d\n", page);
                return false; // bookmark removed
            }
        }
        data.bookmarks.push_back(page);
        Serial.printf("Added bookmark for page %d\n", page);
        return true; // bookmark added
    }

    static bool isBookmarked(const BookmarkData& data, uint16_t page) {
        for (uint16_t bookmarkedPage : data.bookmarks) {
            if (bookmarkedPage == page) {
                return true;
            }
        }
        return false;
    }
};

#endif