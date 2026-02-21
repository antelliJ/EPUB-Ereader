const char* longStory = 
"This story happened a long time ago in a galaxy far, far away. It is already over. Nothing can be done to change it."
"It is a story of love and loss, brotherhood and betrayal, courage and sacrifice and the death of dreams. It is a story of the blurred line between our best and our worst."
"It is the story of the end of an age."
"A strange thing about stories-Though this all happened so long ago and so far away that words cannot describe the time or the distance, it is also happening right now. Right here."
"It is happening as you read these words."
"This is how twenty-five millennia come to a close. Corruption and treachery have crushed a thousand years of peace. This is not just the end of a republic; night is falling on civilization itself."
"This is the twilight of the Jedi."
"The end starts now..\n\n"

"This is the start of a very long text. On an e-paper display, "
"we have to be careful about how much we draw. Unlike a standard LCD, "
"this screen won't just scroll. We have to calculate the 'bounds' "
"of our text. \n\n"
"When we reach the bottom, we wait for the user to press a button. "
"The ESP32-S3 will then clear the buffer, calculate the next set of words, "
"and perform a refresh. Since you have a 5.83-inch screen, you have "
"plenty of room (648x480 pixels), which fits roughly 20-25 lines of text "
"depending on the font size. \n\n"
"PAGE 2 START: To make this work, we use the getCursorY() function. "
"If the current Y position + the height of the next line > display.height(), "
"we 'break' the loop and save our current position in the character array. "
"This creates a seamless reading experience without cutting words in half.\n\n"



"1Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."
"2Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."
"3Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."
"4Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";