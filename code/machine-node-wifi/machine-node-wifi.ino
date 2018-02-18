/*  Machine Node Wifi
 *  Check if a card is permitted to use a machine.
 *
 */
#define DEBUG
#define HTTP_DEBUG

#define AUTH_MODE_PRESENT 0
#define AUTH_MODE_LATCH   1


#include <ESP8266WiFi.h>
#include <MFRC522.h>
#include <RestClient.h>
#include <SPI.h>
#include <ArduinoJson.h>

#include "cardcache.h"
#include "settings.h"

struct rfidCard {
        String uid;
        bool canUse, canInduct;
        unsigned long firstSeen;
};


#define SPI_RFID_SS         D8
#define LED_OK              D1
#define LED_NOTAUTH         D2
#define RELAY               D3
#define LATCH_INPUT         D4

#define CARD_READ_RETRIES   8
#define READ_DELAY          100       // ms
#define WAIT_PERIOD         100       // ms

#define CARD_CLEAR_CACHE_1  "64ACC99A"
#define CARD_CLEAR_CACHE_2  "0498C19A"

RestClient client = RestClient(_PERMSURI_.c_str(), _PERMSPORT_);
MFRC522 rfid(SPI_RFID_SS);

CardCache cache("/perms.txt");
rfidCard currentCard = {"", false, false, 0};

int p = 0; // for aesthetic printing of "card still present" dots.


/*! Read the UID of the card presented to the RFID reader, returns a String.
 */

String readCardUID() {
        String uid = "";

        for (byte i = 0; i < rfid.uid.size; i++)
        {
                char buf [2];
                sprintf(buf,"%02X", rfid.uid.uidByte[i]);
                uid = uid + String(buf);
        }

        return uid;
}

/*! \brief Wait for a card to be presented to the RFID reader.
 *
 *  The RFID reader often "forgets" that it has a card present, and returns
 *  and incorrect result so this function will check multiple times for card
 *  presense before it gives up.
 *  This has the effect of a slight lag while running through the loop, but it
 *  doesn't get in the way of program execution.
 *  Returns false when no longer waiting for a card, returns true when no card present.
 */

bool waitingForCard() {
        int failedReads = 0;
        while(1)
        {
                if (failedReads > CARD_READ_RETRIES) {
                        return true;
                }
                // Is there a new card?
                if (!rfid.PICC_IsNewCardPresent())
                {
                        delay(READ_DELAY);
                        failedReads++;
                        continue;
                }
                // Can we read the serial?
                if (!rfid.PICC_ReadCardSerial())
                {
                        delay(WAIT_PERIOD);
                        failedReads++;
                        continue;
                }
                // Not waiting anymore.
                return false;
        }
}

/*! \brief GET the card permissions from the server
 *
 *  Assembles a GET request and sends it to the server. Returns a string which should contain JSON.
 */
String requestCardPermissionsJson (String carduid) {
        String response;

        // Make sure we aren't sent any HTML...
        client.setContentType("application/json");

        // Build the required string
        String requesturi = "/canuse/" + _MACHINEUID_ +"/" + carduid;
        Serial.println("GET " + _PERMSURI_ + requesturi);

        // Actually do the GET
        client.get(requesturi.c_str(), &response);
        Serial.println("Returned: " + response);
        return response;
}

/*! \brief Processes an incoming JSON string from the server and determines permissions.
 *
 *  Takes a JSON object array and a pointer to an rfidCard struct which will be modified
 *  during the processing. Returns true if the processing was a success, and false if there
 *  was a problem.
 *  The struct referenced by the pointer is _only_ modified if the JSON processing was successful.
 *  Expects:
 *  [{"canuse":1|0, "caninduct":1|0}]
 */

bool processPermissionsJson (String json, rfidCard* card) {

        const size_t bufferSize = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(2) + 30;
        DynamicJsonBuffer jsonBuffer(bufferSize);

        JsonArray& root = jsonBuffer.parseArray(json);
        if (!root.success())
        {
                Serial.println(F("Error, could not parse returned JSON."));
                return false;
        } else
        {
                card->canUse = root[0]["canuse"];
                card->canInduct = root[0]["caninduct"];
                Serial.println(F("Parsed JSON OK."));
                Serial.println("CanUse: " + String(card->canUse) + ", CanInduct: " + String(card->canInduct));
                return true;
        }
}


/*! Performs the actions required to either enable or disable
 *  the output, along with the associated UI changes.
 */
void setOutputState (bool permitted) {
        if (permitted == true)
        {
                digitalWrite(RELAY, true);
                digitalWrite(LED_NOTAUTH, false);
        } else
        {
                digitalWrite(RELAY, false);
                digitalWrite(LED_NOTAUTH, true);
        }
}


void checkCard (rfidCard* card) {
        rfidCard checkingCard = *card;
        // Try a few times to get the information from the server
        // if there is no response and the card UID doesn't exist
        // in the
        for (int i = 1; i <= 3; i++)
        {
                // GET the JSON from the server
                String cardInfo = requestCardPermissionsJson(checkingCard.uid);
                // Attempt to process the data, load it into the currentCard struct.
                // Failure to GET from the server or
                if (processPermissionsJson(cardInfo, &checkingCard))
                {
                        if (checkingCard.canInduct) {
                                // Potential to break out here.
                                // Induct a user.
                        }
                        // Does currentCard.canUse = 1? Add it to the cache :else: delete the card
                        checkingCard.canUse ? cache.addUid(checkingCard.uid) : cache.delUid(checkingCard.uid);
                        break;

                } else
                {
                        checkingCard.canInduct = false;
                        // We can't make any sense of the response we've
                        // got from the server, so let's give the local cache a poke
                        if (cache.doesUidExist(checkingCard.uid))
                        {
                                // Non-zero response means the uid exists.
                                checkingCard.canUse = true;
                                Serial.println(F("Card found in cache. Output enabled."));
                                break;
                        } else
                        {
                                checkingCard.canUse = false;
                                Serial.println(F("Card not found in cache. Output disabled."));
                                break;
                        }
                }
        } // for
        *card = checkingCard;
}



/*  Push the usage time log to the server as a JSON string.
 *  Returns TRUE if the server replies 200, returns FALSE otherwise.
 */

bool sendLog (unsigned long elapsed, String cardUid) {
        String body, response;

        // Create the JSON string we will be sending to the server.
        const size_t bufferSize = JSON_OBJECT_SIZE(3);
        DynamicJsonBuffer jsonBuffer(bufferSize);

        JsonObject& root = jsonBuffer.createObject();

        root["machineuid"] = _MACHINEUID_;
        root["carduid"] = cardUid;
        root["elapsed"] = elapsed;

        root.printTo(body);

        // Tell server we're sending JSON.
        client.setContentType("application/json");

        // Build the request string
        String requesturi = "/log/new";
        Serial.println("POST " + _PERMSURI_ + requesturi);
        Serial.println(body);
        //body =  "machineuid=" + _MACHINEUID_ + "&carduid=" + cardUid + "&elapsed=" + elapsed;
        // Actually do the POST, grab the returned status code.
        //int status = client.post(requesturi.c_str(), body.c_str(), &response);
        int status = client.post(requesturi.c_str(), body.c_str(), &response);
        Serial.println(String(status) + " Returned: " + response);

        // If successful (HTTP 200) return true.
        if (status == 200)
        {
                return true;
        } else
        {
                return false;
        }

}


unsigned long calculateElapsedSeconds (unsigned long startTime) {
        unsigned long elapsedTimeSeconds =  (millis() - startTime) / 1000;
        Serial.println("\r\nTime Output Enabled: " + String(elapsedTimeSeconds) + " s");
        return elapsedTimeSeconds;
}


/*  Wipe all the details of the referenced rfidCard, ready
 *  for the next card to come along.
 */
void wipeCard (rfidCard* card) {
        card->canUse = false;
        card->canInduct = false;
        card->uid = "";
        card->firstSeen = 0;
}


void setup() {
        // Above all else, make sure that we turn off the output relay first.
        pinMode(RELAY, OUTPUT);
        digitalWrite(RELAY, LOW);

        // Now we can do the normal setup stuff. Start the Serial, SPI and init the rfid
        // Also increase the antenna gain for the RFID for more reliable reading at distance
        Serial.begin(115200);
        SPI.begin();
        rfid.PCD_Init();
        rfid.PCD_SetAntennaGain(112);

        cache.setup();

        // The latch input will go high or low once the process has finished.
        // This allows the user to remove their card on long, unattended jobs
        // such as 3D printing or the power hacksaw.
        pinMode(LATCH_INPUT, INPUT);

        pinMode(LED_OK, OUTPUT);
        pinMode(LED_NOTAUTH, OUTPUT);
        digitalWrite(LED_OK, LOW);
        digitalWrite(LED_NOTAUTH, LOW);

        Serial.println(F("Machine Node"));
        Serial.println("Machine UID: " + _MACHINEUID_);

        Serial.print(F("Connecting to wireless network: "));
        Serial.print(ssid);
        WiFi.begin(ssid.c_str(), pass.c_str());
        int i = 0;
        while (WiFi.status() != WL_CONNECTED && i < 20)
        {
                i++;
                delay(500);
                Serial.print(".");
        }

        // Wifi connected.
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("Wifi connection failure.");
        } else {
            Serial.println("Wifi connected.");
            digitalWrite(LED_OK, HIGH);
        }
        setOutputState(false);
}

void loop() {
        if (!waitingForCard())
        {
                // Card here, grab the UID from it.
                String presentedCard = readCardUID();
                // If this is a new card then check its credentials.
                // Otherwise it's probably fine.
                if (presentedCard == CARD_CLEAR_CACHE_1 || presentedCard == CARD_CLEAR_CACHE_2)
                {
                        // One of the hard-coded "Special" cards has been presented.
                        // These are used to force the cache to be deleted.
                        setOutputState(false);
                        digitalWrite (LED_OK, LOW);
                        digitalWrite (LED_NOTAUTH, LOW);
                        cache.setup(true); // Format
                        delay(1000);
                        digitalWrite(LED_OK, HIGH);
                        digitalWrite (LED_NOTAUTH, HIGH);
                }
                if (currentCard.uid!= presentedCard)
                {
                        currentCard.firstSeen = millis();
                        currentCard.uid = presentedCard;
                        Serial.print(F("\r\nRead Card: "));
                        Serial.println(presentedCard);
                        checkCard(&currentCard);
                        p = 0;
                } else {
                        if (p > 40)
                        {
                                // If card is not allowed (canUse == false) print exclamation mark
                                // to indicate that this time isn't being tracked.
                                currentCard.canUse ? Serial.print(". ") : Serial.print(". !");
                                Serial.println( String((millis() - currentCard.firstSeen) / 1000));
                                p = 0;
                        } else
                        {
                                Serial.print(".");
                                p++;
                        }
                }
        } else if (!rfid.PICC_IsNewCardPresent())
        {
                // Card has gone away.
                switch (_AUTHMODE_) {
                case AUTH_MODE_PRESENT:
                        if (currentCard.canUse && currentCard.uid != "")
                        {
                                setOutputState(false);
                                // Calculate seconds elapsed from milliseconds, send it to the server for logging.
                                sendLog(calculateElapsedSeconds(currentCard.firstSeen), currentCard.uid);
                                wipeCard(&currentCard);
                        }
                        break;
                case AUTH_MODE_LATCH:
                        if (digitalRead(LATCH_INPUT) == _LATCHDISABLESTATE_)
                        {
                                sendLog(calculateElapsedSeconds(currentCard.firstSeen), currentCard.uid);
                                wipeCard(&currentCard);
                        }
                        break;
                }
        }
        // Update the current output state for the card presented.
        setOutputState(currentCard.canUse);

        // Update the Wifi Status LED. Lit is OK.
        WiFi.status() == WL_CONNECTED ? digitalWrite(LED_OK, HIGH) : digitalWrite(LED_OK, LOW);

}
