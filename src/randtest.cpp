void drawTextPage(int pageNum) {
  display.setRotation(1);
  display.setFont(&FreeSans9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.firstPage();

  int charIndex = pageStarts[pageNum];

  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(10, 30); // Start with a small margin

    // Logic to print character by character and check for overflow
    for (int i = charIndex; longStory[i] != '\0'; i++) {
      display.print(longStory[i]);

      // If we hit the bottom of the screen
      if (display.getCursorY() > display.height() - 20) {
        // Record where the NEXT page should start if we haven't yet
        if (pageNum + 1 < 10) {
          pageStarts[pageNum + 1] = i + 1;
          if (pageNum + 1 > totalPages) totalPages = pageNum + 1;
        }
        break; // Stop drawing this page
      }
    }
  } while (display.nextPage());

  display.hibernate();
}