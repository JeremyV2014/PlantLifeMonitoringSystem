/*
 *  Authored by Jeremy Maxey-Vesperman with consultation from Zachary Goldasich
 *  Submitted 12/14/2017
 */

//Include wifi library for ESP8266
#include <ESP8266WiFi.h>
#include "SMTP.h"
#include "SensorMetrics.h"

/* Definitions */
#define RED_LED_PIN 0
#define BLUE_LED_PIN 2
#define CONFIG_UDPATE_PIN 5

#define BAUD_RATE 115200

#define DEFAULT_NETWORK_NAME "PlantLifeMonitor"
#define DEFAULT_NETWORK_PASS "PlzSenpaiGiveMeA"
#define DEFAULT_HOSTNAME "Plant_Node_1"

#define STARTUP_WAIT 5000
#define RETRY_ATTEMPTS 3
#define RETRY_DELAY 5000
#define TIMEOUT_INCREMENT 500
#define SLEEP_TIME 30e6 // 30e6 µSec is 30 seconds

#define SMTP_SUBJECT_LINE "Plant Alert!"

/* Global variables */
/* *** INSERT PLANT MONITOR EMAIL ADDRESS HERE *** */
SMTP updater("PLANT@MONITOR.EMAIL");
int attempts = 1;
bool f_Sent = false;

void setup() {

  // startup delay
  // wait so that a normal human has time to connect the serial monitor before program starts
  delay(STARTUP_WAIT);
  // begin serial communication at the defined baud rate
  Serial.begin(BAUD_RATE);

  // Wait for serial to initialize.
  while(!Serial) { }

  pinMode(0, OUTPUT); // red LED
  pinMode(2, OUTPUT); // blue LED
  digitalWrite(0, HIGH);
  digitalWrite(2, HIGH);

  // Turn on red LED to indicate sensor reading is in progress (remove for actual product implementation)
  digitalWrite(RED_LED_PIN , LOW);
  float * sensors = SensorMetrics::getSensors(); // read all sensors and store them in a float array
  digitalWrite(RED_LED_PIN, HIGH); // turn off the red LED (remove for actual product implementation)

  // Init variables for creating SMTP message
  String emailSubject = SMTP_SUBJECT_LINE; // Subject line of email message
  String emailBody = "";

  emailBody += "<h2><u>Plant Node 1</u></h2><br>";
  emailBody += "<h3>&emsp;Configuration:</h3><br>";
  emailBody += "&emsp;&emsp;<b>Mode:</b> Monitor<br>";
  emailBody += "&emsp;&emsp;<b>Update Interval:</b> " + String(SLEEP_TIME/1000000) + " seconds<br>";
  emailBody += "<h3>&emsp;Sensor Readings:</h3><br>";

  for(int i = 0; i < SensorMetrics::_SENSOR_DESCRIPTIONS_SIZE; i++) {
    emailBody += ("<b>" + SensorMetrics::_SENSOR_DESCRIPTIONS[i][0] + "</b>" + sensors[i] + SensorMetrics::_SENSOR_DESCRIPTIONS[i][1]);
  }

  delete [] sensors; // Clean up memory

  WiFi.forceSleepWake(); // workaround for Deep Sleep Mode bug #2186 
  WiFi.hostname(DEFAULT_HOSTNAME); // change hostname to something more friendly
  // attempt to connect to the specified wireless network
  WiFi.begin(DEFAULT_NETWORK_NAME, DEFAULT_NETWORK_PASS);
  
  Serial.print("Connecting...");

  // Wait to be connected
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  // Let the user know we've connected and what our IP address is
  Serial.print("Connected!\tIP Address: ");
  Serial.println(WiFi.localIP());
  
  // Turn on blue LED to indicate an attempt to email (remove for actual product implementation)
  digitalWrite(BLUE_LED_PIN, LOW);
  // atempt to send the email up to 3 times before waiting for next cycle
  while(attempts <= RETRY_ATTEMPTS && !f_Sent) {
    Serial.println("\r\nAttempt #" + String(attempts));
    if(updater.sendUpdateEmail(emailSubject, emailBody)) { // if we successfully sent the update email...
      f_Sent = true; // indicate to the loop that the email sent successfully
    }
    else {
      // increment the timeout by TIMEOUT_INCREMENT per attempt to try to resolve comms issues
      int timeout = updater.getTimeout();
      timeout += TIMEOUT_INCREMENT;
      updater.updateTimeout(timeout);
      attempts++;
      // wait 5 seconds before trying again
      delay(RETRY_DELAY);
    }
  }
  digitalWrite(BLUE_LED_PIN, HIGH); // turn off the blue LED (remove for actual product implementation)

  Serial.println("Going into deep sleep for " + String(SLEEP_TIME/1000000) + " seconds");
  ESP.deepSleep(SLEEP_TIME, WAKE_RF_DEFAULT);
}

/* main loop */
void loop() {

}

// ©2017 Jeremy Maxey-Vesperman
