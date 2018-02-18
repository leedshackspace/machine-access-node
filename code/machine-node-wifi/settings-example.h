// Complete the appropriate fields below and save as "settings.h"

String _MACHINEUID_ = "";
String _PERMSURI_ = "";
int _PERMSPORT_ = 5000;

String ssid = "";
String pass = "";


/* Select an authorisation model.
 *  
 *  AUTH_MODE_PRESENT  The output is only enabled when a valid, 
 *                     authorised card is in reading distance of the
 *                     card reader. Once the card is removed the
 *                     output will disable.
 *                     Use this mode for machines which require
 *                     constant monitoring, such as the CNC Router
 *                     or the Laser Cutter.
 *  
 *  AUTH_MODE_LATCH    The output is enabled when a valid, authorised
 *                     card is presented and then remains enabled 
 *                     until LATCH_INPUT changes to the state
 *                     defined in latchDisableState, HIGH or LOW.
 *                     The hardware should provide a means of "logging
 *                     out" such as a momentary switch.
 *                     Use this mode for machines which can pretty
 *                     much run unattended, such as the 3D printers or
 *                     the donkey saw.
 */

int _AUTHMODE_ = AUTH_MODE_PRESENT;

// When PIN_LATCH_STATE enters the state below, the output will be disabled.
bool _LATCHDISABLESTATE_ = LOW;


