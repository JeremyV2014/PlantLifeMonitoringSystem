/*
 *  Authored by Jeremy Maxey-Vesperman with consultation from Zachary Goldasich
 *  Submitted 12/14/2017
 */

// Include necessary header files
#include "Arduino.h"
#include "SensorMetrics.h"
#include <SPI.h>
#include <math.h>

/* Definitions */

// Delays
#define SENSOR_STABILIZE_DELAY 500 // ms

// ADC reference info
#define ADC_RESOLUTION 1024 // both adcs are 10-bit resolution
#define EXTERN_ADC_V 2.84 // MCP3008 is setup to reference voltage from DIO pin on ESP8266
#define INTERN_ADC_V 1.1 // ESP8266 internal adc has 1.1V LDO for voltage referencing

// External ADC (MCP3008)
#define EXTERN_ADC_SPI_FREQ 18000 // clock frequency for SPI comms with external adc
#define SENSOR_EN_PIN 4 // pin for powering the external adc chip and sensors
#define SS_PIN 15 // Slave Select pin for external adc
#define PHOTORESISTOR_CH 1 // channel of external adc that photoresistor is connected to
#define THERMISTOR_CH 2 // channel of external adc that thermistor is connected to
#define MOISTURE_SENSOR_CH 4 // channel of external adc that moisture sensor is connected to

// Photoresistor info (WDYJ GM5539)
#define PHOTORESISTOR_R2 9830 // photoresistor voltage divider circuit R2 value
// Best fit line equation values generated from test measurements
#define PHOTORESISTOR_M -1.1116
#define PHOTORESISTOR_B 7.3113

// Thermistor info (Honeywell 192-103LET-A01)
#define THERMISTOR_R2 9930 // thermistor voltage divider circuit R2 value
#define THERMISTOR_BETA 3974 // Beta value of thermistor ()
#define THERMISTOR_R0 10000
#define THERMISTOR_T0 298.15

// Moisture sensor info (XCSOURCE TE215)
#define MOISTURE_SENSOR_R1 406100 // moisture sensor voltage divider circuit R1 value
#define MOISTURE_SENSOR_R2 129900 // moisture sensor voltage divider circuit R2 value
#define MOISTURE_MIN 2800 // Output (mV) at 0% moisture
#define MOISTURE_MAX 500 // Output (mV) at 100% moisture

// Battery voltage divider circuit info
#define BATTERY_R1 389700
#define BATTERY_R2 128100

// Constants for temperature conversions and calculations
#define ROOM_TEMP_K 298.15
#define TEMP_F_MULT 1.8
#define TEMP_F_CONST 459.67
#define TEMP_C_CONST 273.15

/* Constants */

const int SensorMetrics::_SENSOR_DESCRIPTIONS_SIZE = 4;
const String SensorMetrics::_SENSOR_DESCRIPTIONS[_SENSOR_DESCRIPTIONS_SIZE][2] = {	{"&emsp;&emsp;Light Intensity: ", " lux<br>"},
											                                                              {"&emsp;&emsp;Temperature: ", "ºF<br>"},
											                                                              {"&emsp;&emsp;Moisture Level: ", "%<br>"},
											                                                              {"&emsp;&emsp;Battery Level: ", "V<br>"}};

/* Functions */

// Function for turning on the external ADC and sensors
void SensorMetrics::externADCOn() {
	digitalWrite(SS_PIN, HIGH); // deselect the ADC (needs a falling edge)
	digitalWrite(SENSOR_EN_PIN, HIGH); // turn on the ADC and sensors
	delay(SENSOR_STABILIZE_DELAY); // give time for adc to turn on and sensor voltages to stabilize
}

// Function for turning off the external ADC and sensors
void SensorMetrics::externADCOff() {
	digitalWrite(SS_PIN, LOW); // save energy by turning off the pin
	digitalWrite(SENSOR_EN_PIN, LOW); // turn off the ADC and sensors
}

// Function for setting up pins and SPI for external ADC and sensors
void SensorMetrics::setupExternADC() {
	pinMode(SENSOR_EN_PIN, OUTPUT);
	pinMode(SS_PIN, OUTPUT);
	
	SPI.begin();
	SPI.setBitOrder(MSBFIRST); // MSB bit sent first
	SPI.setDataMode(SPI_MODE0);
	SPI.setFrequency(EXTERN_ADC_SPI_FREQ); // 18kHz / 18 = 1ksa/s (1ms readings)
}

// Function for getting readings from the ESP8266's internal ADC
// ADC has an internal voltage reference of 1.1V
int SensorMetrics::readInternADC() {
  int reading = analogRead(A0); // read from A0 (ADC pin)
  
  return reading; // return the reading 
}

// Function for reading from external SPI ADC chip
// ADC has voltage reference of digital pin output voltage on ESP8266
int SensorMetrics::readExternADC(int ch) {
  if (ch > 7 || ch < 0) { // MCP3008 has 8 channels
    return -1; // return -1 if passed invalid channel argument
  }

  digitalWrite(SS_PIN, LOW); // select ADC
  SPI.transfer(B00000001); // send 7 leading 0's and the START bit
  // send control bits (single-ended + channel address shifted into upper 4 bits)
  uint8_t reading_upper = SPI.transfer((ch + 8) << 4); // store null bit + upper 2 bits of adc reading
  uint8_t reading_lower = SPI.transfer(0); // store lower byte of adc reading

  digitalWrite(SS_PIN, HIGH); // deselect ADC

  // clear null bit, shift first byte of reading into upper byte of int, bitwise OR second byte with lower byte of int
  return (((reading_upper & 3) << 8) + reading_lower); // return 10-bit ADC reading for specified channel
}

// Function to convert adc value into voltage measurement
float SensorMetrics::getVMeas(int adc, bool externADC) {
  // Compute first part of vMeas by getting adc reading ratio
  float vMeas = ((float(adc) + 1.0) / float(ADC_RESOLUTION)); // resolution is same between internal and external chip

  // If we are reading from external adc...
  if (externADC) {
    vMeas *= EXTERN_ADC_V; // multiply by external adc reference voltage
  }
  // Otherwise...
  else {
    vMeas *= INTERN_ADC_V; // multiply by internal adc reference voltage
  }

  return vMeas; // return the voltage measured
}

// Function to calculate input voltage to voltage divider circuit
float SensorMetrics::getVDivVin(float r1, float r2, float vDrop, bool vDropAcrossR1) {
  // Calculate input voltage to divider circuit (AKA battery voltage) based on known R1 and R2
  float vIn = (vDrop * (r1 + r2));

  // Divide by appropriate resistor value
  if (vDropAcrossR1) {
    vIn /= r1;
  }
  else {
    vIn /= r2;
  }

  return vIn; // return the input voltage of the voltage divider
}

// Function to convert measured voltage into R1 value of a voltage divider circuit
float SensorMetrics::getVDivR1(float vMeas, int r2, bool externADC) {
  float r1;

  // Use appropriate reference voltage based on which adc took the reading...
  if (externADC) {
    r1 = ((EXTERN_ADC_V * r2) - (vMeas * r2)) / vMeas;
  }
  else {
    r1 = ((INTERN_ADC_V * r2) - (vMeas * r2)) / vMeas;
  }
  
  return r1; // return the value of r1 in Ohms (Ω)
}

// Function to get light intensity reading (Lux) from photoresistor
float SensorMetrics::getLux (bool externADC) {
  int prADC;
  float lux, vMeas, photoResistor;

  // Read the adc channel that the photoresistor is attached to
  if (externADC) {
    prADC = readExternADC(PHOTORESISTOR_CH);
  }
  else {
    prADC = readInternADC();
  }

  vMeas = getVMeas(prADC, externADC); // convert adc reading to voltage
  // Get value of photoresistor based on vMeas and the value of the known R2 resistor in the voltage divider circuit
  photoResistor = getVDivR1(vMeas, PHOTORESISTOR_R2, externADC);

  // Get light intensity in lux using photoresistor value and variables from line of best fit for the sensor
  lux = pow(photoResistor, PHOTORESISTOR_M) * pow(10, PHOTORESISTOR_B); // *** m and b are unique to the photoresistor

  return lux; // return the light intensity value in lux
}

// Function to get temperature in Kelvin from thermistor
float SensorMetrics::getTempK (bool externADC) {
  int thADC;
  float tempK, vMeas, thermistor;

  // Read the adc channel that the thermistor is attached to
  if (externADC) {
    thADC = readExternADC(THERMISTOR_CH);
  }
  else {
    thADC = readInternADC();
  }

  vMeas = getVMeas(thADC, externADC); // convert adc reading to voltage
  // Get value of thermistor based on vMeas and the value of the known R2 resistor in the voltage divider circuit
  thermistor = getVDivR1(vMeas, THERMISTOR_R2, externADC); 

  // Return the room temp in Kelvin constant if thermistor value = the thermistor r0 value (resistance of thermistor @ room temp in K)
  // This prevents divide by 0 error in conversion formula in the event that reading is exactly room temperature (ln(1) = 0...)
  if (thermistor == THERMISTOR_R0) {
    tempK = ROOM_TEMP_K;
  }
  // Otherwise... use the conversion formula to get temp in K from the resistance value of thermistor
  else {
    tempK = (((THERMISTOR_T0 * THERMISTOR_BETA) / (log(THERMISTOR_R0 / thermistor))) / (((THERMISTOR_BETA) / (log(THERMISTOR_R0 / thermistor))) - THERMISTOR_T0));
  }

  return tempK; // return the temperature reading in Kelvin
}

// Function to get temperature in Fahrenheit from thermistor
float SensorMetrics::getTempF (bool externADC) {
  float tempK, tempF;
  
  tempK = getTempK(externADC); // get the temperature in Kelvin
  tempF = ((tempK * TEMP_F_MULT) - TEMP_F_CONST); // convert to Fahrenheit from Kelvin
  
  return tempF; // return the temperature reading in Fahrenheit
}

// Function to get temperature in Celsius from thermistor
float SensorMetrics::getTempC (bool externADC) {
  float tempK, tempC;
  
  tempK = getTempK(externADC); // get the temperature in Kelvin
  tempC = (tempK + TEMP_C_CONST); // convert to Celsius from Kelvin
  
  return tempC; // return the temperature reading in Celsius
}

// Function to get moisture level %
float SensorMetrics::getMoistureLvl(bool externADC) {
  int moistADC;
  float moistLvl, vMoist, vMeas;

  moistADC = readExternADC(MOISTURE_SENSOR_CH); // get reading from external adc
  vMeas = getVMeas(moistADC, externADC); // convert to voltage using appropriate reference voltage
  vMoist = ((getVDivVin(MOISTURE_SENSOR_R1, MOISTURE_SENSOR_R2, vMeas)) * 1000); // calculate input voltage (mV) divider circuit with R1 drop
  moistLvl = map(vMoist, MOISTURE_MIN, MOISTURE_MAX, 0, 100); // map millivolts to %

  return moistLvl; // return moisture % reading
}

// Function to get the battery voltage
float SensorMetrics::getBatteryLvl() {
  int batADC;
  float vBat, vMeas;
  
  batADC = readInternADC(); // get reading from internal adc
  vMeas = getVMeas(batADC, false); // convert to voltage using appropriate reference voltage
  vBat = getVDivVin(BATTERY_R1, BATTERY_R2, vMeas); // calculate input voltage to divider circuit with R2 drop
  
  return vBat; // return the battery voltage reading
}

float* SensorMetrics::getSensors() {
	float lux, temp, moisture, battery;
	
	setupExternADC(); // setup external ADC
	externADCOn(); // turn on the ADC and sensors
	
	lux = getLux(); // get light intensity reading
	temp = getTempF(); // get temperature reading
	moisture = getMoistureLvl(); // get moisture level reading
	battery = getBatteryLvl(); // get battery level reading
	
	// Create array of sensor values
	float * sensors = new float[4];
	
	sensors[0] = lux;
	sensors[1] = temp;
	sensors[2] = moisture;
	sensors[3] = battery;
	
	externADCOff(); // turn off the ADC and sensors
	
	return sensors; // return array of sensor values
}

// ©2017 Jeremy Maxey-Vesperman
