/*
 *  Authored by Jeremy Maxey-Vesperman with consultation from Zachary Goldasich
 *  Submitted 12/14/2017
 */

#ifndef SMTP_h
#define SMTP_h

// Include the necessary libraries
#include "Arduino.h"
#include <ESP8266WiFi.h>

/* SMTP class definition */
class SMTP {
	public:
		/* Constructors */
		SMTP(); // default constructor inits to default timeout and recipient address
		SMTP(String recipientAddr); // constructor that inits to different recipient address
		
		/* Public Functions and Methods */
		bool sendUpdateEmail(String subject, String body); // function for sending update email
		int getTimeout(); // function for getting the current timeout setting
		void updateRecipientAddr(String newAddr); // method for updating the recipient address
		void updateTimeout(int newTimeout); // method for updating the timeout
	private:
		/* Private Instance Variables */
		WiFiClientSecure _smtpClient; // secure TCP client object
		int _timeout; // how long to wait for a server response
		String _recipientAddr; // email address of the recipient
		String _emailStatusMsg; // output status message
		bool f_EmailSuccessful; // output status flag
	
		/* Private Functions and Methods */
		void sendCmd(String command, int expectedResponse, String cmdFriendlyName); // method for sending commands and validating response codes
		int readResponse(); // function for reading server responses
		String genInvalResponse(String command, int responseCode); // function for generating an invalid server response error message
};

#endif

// ©2017 Jeremy Maxey-Vesperman
