/*
 *  Authored by Jeremy Maxey-Vesperman with consultation from Zachary Goldasich
 *  Submitted 12/14/2017
 */

#ifndef SensorMetrics_h
#define SensorMetrics_h

// Include the necessary libraries
#include "Arduino.h"
#include <SPI.h>
#include <math.h>

/* SensorMetrics class definition */
class SensorMetrics {
	public:
		/* Public Functions and Methods */
		static void externADCOn();
		static void externADCOff();
		static void setupExternADC();
		static float* getSensors(); // function to get all sensor readings
		static float getLux (bool externADC=true); // function to get light intensity reading (Lux) from photoresistor
		static float getTempK (bool externADC=true); // function to get temperature in Kelvin from thermistor
		static float getTempF (bool externADC=true); // function to get temperature in Fahrenheit from thermistor
		static float getTempC (bool externADC=true); // function to get temperature in Celsius from thermistor
		static float getMoistureLvl(bool externADC=true); // function to get moisture level %
		static float getBatteryLvl(); // function to get the battery voltage
		
		/* Public Class Constants */
    static const int _SENSOR_DESCRIPTIONS_SIZE;
		static const String _SENSOR_DESCRIPTIONS[][2];
	private:
		/* Private Functions and Methods */
		static int readInternADC(); // function for getting readings from the ESP8266's internal ADC
		static int readExternADC(int ch); // function for reading from external SPI ADC chip
		static float getVMeas(int adc, bool externADC=true); // function to convert adc value into voltage 
		static float getVDivVin(float r1, float r2, float vDrop, bool vDropAcrossR1=false); // function to calculate input voltage to voltage divider circuit
		static float getVDivR1(float vMeas, int r2, bool externADC); // function to convert measured voltage into R1 value of a voltage divider circuit
};

#endif

// ©2017 Jeremy Maxey-Vesperman
