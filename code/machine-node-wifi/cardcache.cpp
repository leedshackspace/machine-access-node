#include "Arduino.h"
#include "FS.h"
#include "cardcache.h"

#define BLANK_UID "        "


#define DEBUG

#ifdef DEBUG

#define DEBUG_PRINT(x)  Serial.print (x)
#define DEBUG_PRINTLN(x) Serial.println (x)

#else

#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)

#endif


/*! Starts the SPI File System used for file storage and copies the filename to a private
 *  class variable for use later on. There is no default filename/path.
 */
CardCache::CardCache (String file) {
        _file = file;
        SPIFFS.begin();
}

/*! \brief Add a card UID to the local cache. 
 *
 *  First checks if the UID is present in the cache already. If the entry is present
 *  in the cache it will not be added again.
 * 
 *  Returns true if the UID was added, returns false if UID was not added because it already exists. 
 */
 
 bool CardCache::addUid (String uid) {
        if (!this->doesUidExist(uid)) {
                // Card not in the cache already, so add it.
                this->writeUid(uid);
                DEBUG_PRINT(F("addToCache: Added new: "));
                DEBUG_PRINTLN(uid);
                return true; // Added the card
        } else {
                DEBUG_PRINT(F("addToCache: Already exists: "));
                DEBUG_PRINTLN(uid);
                return false; // Didn't add the card, already existed.
        }
}


/*! \brief Prepare the cache for use by checking for the presence of the cache file, and
 *         creating the file if it isn't found. Includes the option to "format" the filesystem. 
 *
 *  First checks if the UID is present in the cache already. If the entry is present
 *  in the cache it will not be added again.
 * 
 *  Returns true if the UID was added, returns false if UID was not added because it already exists. 
 */

void CardCache::setup (bool format) {

        FSInfo fs_info;
        SPIFFS.info(fs_info);

        DEBUG_PRINTLN(F("File System Data:"));
        DEBUG_PRINT(F("Total Space:\t"));
        DEBUG_PRINTLN(fs_info.totalBytes);
        DEBUG_PRINT(F("Used Space:\t"));
        DEBUG_PRINTLN(fs_info.usedBytes);
        DEBUG_PRINT(F("Free Space:\t"));
        DEBUG_PRINTLN(fs_info.totalBytes - fs_info.usedBytes);
        DEBUG_PRINTLN("File: " + _file);

        // We've been asked to format the free space, this will force the rest of the function
        // to fail to "couldn't read the file." and clear the cache.
        if (format) {
                SPIFFS.format() ? DEBUG_PRINTLN("setup: Formatted OK.") : DEBUG_PRINTLN("setup: Free space format failed.");
        }
        // Open the file in read mode.
        File f = SPIFFS.open(_file, "r+");

        // If the opening fails it probaby doesn't exist, make the file.
        if (!f) {
                DEBUG_PRINTLN(F("Cache doesn't exist. Creating it."));

                File f = SPIFFS.open(_file, "w+");
                if (!f) {
                        DEBUG_PRINTLN(F("Couldn't create cache."));
                }
                // Write the title. We aren't storing a Card UID on the first "row"
                // which allows us to use 0 as false/not found when referring to a location.
                f.println("AUTH UID");

        } else {
                DEBUG_PRINTLN(F("Current authorised UIDs"));
                // We have opened the file, now print everything in it to the serial port.
                while (f.available()) {
                        // Read until the newline character.
                        String line = f.readStringUntil('\n');
                        DEBUG_PRINTLN(String(f.position() - 10) + "\t" + line);
                }

        }

        f.close();
}

/*! Clear the cache. Try to remove the file, then run the setup to establish a new cache
 *  if successful. Returns true if cache was removed, false if the cache couldn't be removed.
 */

bool CardCache::clearCache () {
        if (SPIFFS.remove(_file)) {
                DEBUG_PRINTLN(F("clearCache: Deleted cache."));
                this->setup();
                return true;
        } else {
                DEBUG_PRINTLN(F("clearCache: Could not delete."));
                return false;
        }
}

/*! \brief Remove a card UID from the local cache. 
 *
 *  Searches through the cache for all instances of the provided UID, and 
 *  continues searching until the doesUidExist call returns false.
 *  Returns the number of entries it deleted.
 *  In theory this extended search shouldn't be nessecary as if the UID already
 *  exists it cannot be added, but we're covering all the bases here in case there is
 *  a hiccup somewhere along the line.
 */
int CardCache::delUid (String uid) {
        unsigned long location = this->doesUidExist(uid);
        int i = 0;
        while (location) {
                DEBUG_PRINT(F("delUid: Deleting UID: "));
                DEBUG_PRINTLN(uid + " at position " + location);
                this->scrubPosition (location);
                location = this->doesUidExist(uid);
                i++;
        }
        DEBUG_PRINT(F("delUid: Num entries deleted: "));
        DEBUG_PRINTLN(i);
        return i;
}


/*! \brief Checks if a UID is present in the cache.
 *
 *  Searches through the cache for the first instance of the UID provided in the cache.
 *  This function will only return the first instance, and not anything after. Currently
 *  it is limited in counting 8-byte UIDs - storing longer UIDs may result in a problem.
 *  Returns the location of the start of the entry, or 0, which indicates not found.
 */
unsigned long CardCache::doesUidExist (String uid) {
        File f = SPIFFS.open(_file, "r");
        if (!f) {
                DEBUG_PRINTLN(F("Couldn't open permissions file."));
                return false; // 0
        } else {
                // we could open the file
                while (f.available()) {
                        //Lets read line by line from the file
                        String line = f.readStringUntil('\n');
                        String compare = uid + String("\r");
                        if (line == compare)
                        {
                                // Current hardcoding of card length.
                                // TODO: Detect previous EOL markers and run up to there, allow for
                                // non-uniform card length detection.
                                int location = f.position() - 10;
                                f.close();
                                return location;
                        }
                }
                f.close();
                return false; // 0
        }
}

/*! \brief Erases the UID entry at the instructed position.
 *
 *  Overwrites 8 bytes after the provided position with blanks (leaving the EOL markers)
 *  to erase a UID.
 *  Returns true if successful, false if not.
 */
bool CardCache::scrubPosition (unsigned long pos) {
        File f = SPIFFS.open(_file, "r+");
        if (!f) {
                DEBUG_PRINTLN(F("Couldn't open permissions file."));
                return false;
        } else {
                if (f.seek(pos, SeekSet)) {
                        f.println(BLANK_UID);
                        f.close();
                        return true;
                } else {
                        DEBUG_PRINTLN("Couldn't delete UID.");
                        f.close();
                        return false;
                }

        }
}

/*! \brief Writes a UID to the cache at the first available space.
 *
 *  When UIDs are removed they are overwritten with spaces, so it's important
 *  to reuse these gaps rather than just tagging onto the end of the cache file. 
 *  This method will search through for the first blank line, then fill it, or run
 *  through to the end and append the UID.
 *  Returns true if successfully added, returns false if the UID couldn't be added.
 */
 
bool CardCache::writeUid (String uid) {
        File f = SPIFFS.open(_file, "r+");
        if (!f) {
                DEBUG_PRINTLN(F("Couldn't open permissions file."));
                return false;
        } else {
                while (f.available()) {
                        // Read line by line and check for blanked entries.
                        // If we find a blank line, use that instead of creating
                        // a new line.
                        String line = f.readStringUntil('\n');
                        String compare = BLANK_UID + String('\r');
                        if (line == compare)
                        {
                                f.seek(-10, SeekCur);
                                f.println(uid);
                                f.close();
                                return true;
                        }
                }
                // There were no blank lines, create a new one.
                f.println(uid);
                f.close();
                return true;
        }
}
