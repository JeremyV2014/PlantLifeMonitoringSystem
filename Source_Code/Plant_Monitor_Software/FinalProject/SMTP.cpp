/*
 *  Authored by Jeremy Maxey-Vesperman with consultation from Zachary Goldasich
 *  Submitted 12/14/2017
 */

// Include necessary header files
#include "Arduino.h"
#include "SMTP.h"
#include <ESP8266WiFi.h>

/* Definitions */
// Only support connection to Google's SMTP server through SSL socket
#define SMTP_SERVER "smtp.gmail.com"
#define SMTP_PORT 465
#define DEFAULT_RESPONSE_TIMEOUT 750 // Default wait time for responses from server


// Various SMTP commands and friendly names of commands
#define HELO_CMD "HELO Seedling"
#define AUTH_LOGIN_CMD "AUTH LOGIN"
#define USERNAME_CMD "Username"
#define PASSWORD_CMD "Password"
/* *** MODIFY THIS TO SPECIFY THE EMAIL ACCOUNT FOR THE MONITOR *** */
#define MAIL_FROM_CMD "MAIL FROM: <PLANT@MONITOR.EMAIL>" // No support for sending from different address atm
#define RCPT_TO_CMD_1 "RCPT TO: <"
#define RCPT_TO_CMD_2 ">"
#define DATA_CMD "DATA"
#define FROM_INPUT "From: Plant Life Monitor <plantlifemonitor@gmail.com>"
#define TO_INPUT "To: "
#define SUBJECT_INPUT "Subject: "
#define EOM_CMD "EOM"
#define QUIT_CMD "QUIT"
#define SMTP_TERMINATE_CHAR "." // EOM character for body of email
// ToDo: REMOVE THIS AND FORCE USER TO SPECIFY RECIPIENT ADDRESS AT SETUP
// *** MODIFY THIS TO SPECIFIY THE DEFAULT RECEIPIENT EMAIL ADDRESS ***
#define DEFAULT_RECIPIENT_ADDR "DEFAULT@RECEIPIENT.HERE" // Default recipient address

// Base64 encoded form of username and password for sender mailbox
// *** MODIFY THESE TO MAKE THIS WORK WITH YOUR EMAIL ACCOUNT ***
#define B64_USERNAME "INSERT_B64_ENCODED_USERNAME"
#define B64_PASSWORD "INSERT_B64_ENCODED_PASSWORD"

// SMTP server response codes
#define RESP_SERVICE_READY 220
#define RESP_ACTION_OKAY 250
#define RESP_AUTH_LOGIN 334
#define RESP_AUTHENTICATED 235
#define RESP_START_MAIL 354
#define RESP_CLOSE 221

// Formatting for error messages
#define INVAL_RESP_1 "Did not receive response "
#define INVAL_RESP_2 " to "
#define INVAL_RESP_3 " command."

// Diagnostics messages
#define EMAIL_SUCCESS_MSG "Email Sent Successfully!"
#define SMTP_CONNECT_SUCCESS_MSG "Successfully connected to SMTP Server!"
#define SMTP_CONNECT_FAILED_MSG "Failed to connect to SMTP server"
#define HELO_MSG "Issuing HELO command"
#define AUTH_LOGIN_MSG "Issuing AUTH LOGIN command"
#define USERNAME_MSG "Issuing USERNAME command"
#define PASSWORD_MSG "Issuing PASSWORD command"
#define MAIL_FROM_MSG "Issuing MAIL FROM command"
#define RCPT_TO_MSG "Issuing RCPT TO command: "
#define DATA_MSG "Issuing DATA command"
#define BODY_MSG "Sending body of message..."
#define EOM_MSG "Sending EOM character"
#define QUIT_MSG "Issuing QUIT command"

/* Variables */
WiFiClientSecure _smtpClient; // create a secure client object
int _timeout;
String _recipientAddr;
// Output message and flag
String _emailStatusMsg;
boolean f_EmailSuccessful;

/* Constructors */
// Default constructor uses default recipient address and timeout
SMTP::SMTP()
  : _recipientAddr(DEFAULT_RECIPIENT_ADDR), _timeout(DEFAULT_RESPONSE_TIMEOUT)
{
}

// Constructor that initializes recipient address to the one specified
SMTP::SMTP(String recipientAddr)
  : _recipientAddr(recipientAddr), _timeout(DEFAULT_RESPONSE_TIMEOUT)
{
}

/* Functions */

/* Method for sending update email via SMTP */
bool SMTP::sendUpdateEmail(String subject, String body) {
  // reinit output variables of the instance
  _emailStatusMsg = EMAIL_SUCCESS_MSG;
  f_EmailSuccessful = true;

  // if we successfully connect to the SMTP server...
  if (_smtpClient.connect(SMTP_SERVER, SMTP_PORT)) { // connect via SSL socket
    Serial.println(SMTP_CONNECT_SUCCESS_MSG);
    
    // create the recipient command string from the specified recipient address
    String recipientCmd = (RCPT_TO_CMD_1 + _recipientAddr + RCPT_TO_CMD_2);

    Serial.println(HELO_MSG);
    sendCmd(HELO_CMD, RESP_SERVICE_READY, HELO_CMD); // issue HELO command
    Serial.println(AUTH_LOGIN_MSG);
    sendCmd(AUTH_LOGIN_CMD, RESP_AUTH_LOGIN, AUTH_LOGIN_CMD); // issue authentication command
    Serial.println(USERNAME_MSG);
    sendCmd(B64_USERNAME, RESP_AUTH_LOGIN, USERNAME_CMD); // send username
    Serial.println(PASSWORD_MSG);
    sendCmd(B64_PASSWORD, RESP_AUTHENTICATED, PASSWORD_CMD); // send password
    Serial.println(MAIL_FROM_MSG);
    sendCmd(MAIL_FROM_CMD, RESP_ACTION_OKAY, MAIL_FROM_CMD); // specify address we are sending from
    Serial.println(RCPT_TO_MSG + recipientCmd);
    sendCmd(recipientCmd, RESP_ACTION_OKAY, recipientCmd); // specify address of the receiver
    Serial.println(DATA_MSG);
    sendCmd(DATA_CMD, RESP_START_MAIL, DATA_CMD); // issue DATA command to start sending message
    
    Serial.println(BODY_MSG); 

    _smtpClient.println(FROM_INPUT); // send the From line
    delay(_timeout);
    _smtpClient.println(SUBJECT_INPUT + subject); // send the Subject line
    delay(_timeout);
    _smtpClient.println(TO_INPUT + _recipientAddr); // send the To line
	  delay(_timeout);
    _smtpClient.println("Mime-Version: 1.0;\r\nContent-Type: text/html; charset=\"ISO-8859-1\r\nContent-Transfer-Encoding: 7bit;");
    delay(_timeout);
	  _smtpClient.println();
    delay(_timeout);
    _smtpClient.println("<html>\r\n<body>\r\n" + body + "\r\n</body></html>"); // send message
    delay(_timeout);

    Serial.println(EOM_MSG);
    sendCmd(SMTP_TERMINATE_CHAR, RESP_ACTION_OKAY, EOM_CMD); // send terminating character to end message
    Serial.println(QUIT_MSG);
    sendCmd(QUIT_CMD, RESP_CLOSE, QUIT_CMD); // issue QUIT command to terminate session
    
    // output status message upon completion and return flag
    Serial.println(_emailStatusMsg);

    _smtpClient.stop(); // 
    
    return f_EmailSuccessful;
  }
  else { // otherwise... output error and return false
    Serial.println(SMTP_CONNECT_FAILED_MSG);
    return false;
  }
}

/* Getter method to return timeout for connecting to SMTP server */
int SMTP::getTimeout() {
  return _timeout;
}

/* Setter method to update recipient email address */
// SHOULD ADD SOME WAY TO VERIFY VALID EMAIL FORMAT
void SMTP::updateRecipientAddr(String newAddr) {
  _recipientAddr = newAddr;
}

/* Setter method to update timeout for connecting to SMTP server */
void SMTP::updateTimeout(int newTimeout) {
  if(newTimeout > 0) { // if new timeout is a valid timeout setting...
    _timeout = newTimeout; // update
  }
  else { // otherwise...
    // allows for reseting to default without knowing the default setting
    _timeout = DEFAULT_RESPONSE_TIMEOUT; // reset to default
  }
  
}

/* Issues SMTP command and checks the response if sending email hasn't already failed */
void SMTP::sendCmd(String command, int expectedResponse, String cmdFriendlyName) {
  if (f_EmailSuccessful) { // if we haven't failed yet...
    _smtpClient.println(command); // issue the specified command
    if (readResponse() != expectedResponse) { // if the response code is not the expected one...
        _emailStatusMsg = genInvalResponse(cmdFriendlyName, expectedResponse); // update status message
        f_EmailSuccessful = false; // set the success flag to false
      }
  }
}

/* Reads entire response returned by the server. Returns the response code to the caller. */
int SMTP::readResponse() {
  String response = "";
  int i = 0;
  
  delay(_timeout); // wait specified amount of time for a response

  while(_smtpClient.available()) { // while there is something to read...
      char c = _smtpClient.read(); // read the character...
      Serial.print(c); // output the character...

      if(i < 3) { // first 3 characters are the first response code
        response += c;
        i++;
      }
    }
  
  if (response == "") { // return -1 if response not received before timeout
    return -1;
  }

  // else... return response code converted to int or 0 if no valid response string in first 3 characters
  return response.toInt();
}

/* Generates the invalid response string from response code expected and the command sent */
String SMTP::genInvalResponse(String command, int responseCode) {
  return (INVAL_RESP_1 + String(responseCode) + INVAL_RESP_2 + command + INVAL_RESP_3);
}

// ©2017 Jeremy Maxey-Vesperman
