#include <Arduino.h>
#include <Ticker.h>

#include "WiFi.h"
#include <HTTPClient.h>
#include "time.h"

#include "DHTesp.h" // Click here to get the library: http://librarymanager/All#DHTesp

#include <credencial.h>

#ifndef ESP32
#pragma message(THIS EXAMPLE IS FOR ESP32 ONLY!)
#error Select ESP32 board.
#endif

#define  LM335_pin  15

unsigned long int tempo = millis();
byte intervalo_leitura = 30;
/** Initialize DHT sensor 1 */
DHTesp dhtSensor1;
/** Initialize DHT sensor 2 */
DHTesp dhtSensor2;
/** Task handle for the light value read task */
TaskHandle_t tempTaskHandle = NULL;
/** Pin number for DHT11 1 data pin */
byte dhtPin1 = 4;
/** Pin number for DHT11 2 data pin */
byte dhtPin2 = 18;
byte relePin1 = 19;
/** Ticker for temperature reading */
Ticker tempTicker;
/** Flags for temperature readings finished */
bool gotNewTemperature = false;
/** Data from sensor 1 */
TempAndHumidity sensor1Data;
/** Data from sensor 2 */
TempAndHumidity sensor2Data;

/* Flag if main loop is running */
bool tasksEnabled = false;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800;
const int   daylightOffset_sec = 0;
String pagina = "pg2";
int count = 1;
double Celsius;

/**
 * Task to reads temperature from DHT11 sensor
 * @param pvParameters
 *		pointer to task parameters
 */
void tempTask(void *pvParameters) {
	Serial.println("tempTask loop started");
	while (1) // tempTask loop
	{
		if (tasksEnabled && !gotNewTemperature) { // Read temperature only if old data was processed already
			// Reading temperature for humidity takes about 250 milliseconds!
			// Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
			sensor1Data = dhtSensor1.getTempAndHumidity();	// Read values from sensor 1
			sensor2Data = dhtSensor2.getTempAndHumidity();	// Read values from sensor 1
			//Celsius = analogRead(LM335_pin) * 0.489 - 273;
			gotNewTemperature = true;
		}
		vTaskSuspend(NULL);
	}
}

/**
 * triggerGetTemp
 * Sets flag dhtUpdated to true for handling in loop()
 * called by Ticker tempTicker
 */
void triggerGetTemp() {
	if (tempTaskHandle != NULL) {
		 xTaskResumeFromISR(tempTaskHandle);
	}
}

/**
 * Arduino setup function (called once after boot/reboot)
 */

void connectWifi(){
  if(WiFi.status() == WL_CONNECTED) {
	Serial.println("conectado");
  }
  else {
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED || millis()<=(tempo+1000)) {
    	delay(500);
    	Serial.print(".");
	}
	//Serial.println("Erro wifi 95");
	tempo = millis();
  }
  // Init and get the time
}

void loopDataLog(){
	if (WiFi.status() == WL_CONNECTED) {
		configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
		static bool flag = false;
		struct tm timeinfo1;
		if (!getLocalTime(&timeinfo1)) {
			Serial.println("Falha ao obter data/hora.");
			return;
		}
		char timeStringBuff[50]; //50 chars should be enough
		strftime(timeStringBuff, sizeof(timeStringBuff), "%F", &timeinfo1);
		String asString(timeStringBuff);
		asString.replace(" ", "-");
		//Serial.print("Time:");
		//Serial.println(asString);
		String urlFinal = "https://script.google.com/macros/s/"+GOOGLE_SCRIPT_ID+"/exec?"+"data=" + asString + "&tmp1="+String(sensor1Data.temperature,2)+"&umd1="+String(sensor1Data.humidity,1)+"&tmp2="+String(sensor2Data.temperature,2)+"&umd2="+String(sensor2Data.humidity,1)+"&pg="+pagina+"&tmp3="+String(Celsius);
		//Serial.print("POST data to spreadsheet:");
		//Serial.println(urlFinal);
		HTTPClient http;
		http.begin(urlFinal.c_str());
		http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
		int httpCode = http.GET(); 
		//Serial.print("HTTP Status Code: ");
		//Serial.println(httpCode);
		//---------------------------------------------------------------------
		//getting response from google sheet
		String payload;
		if (httpCode > 0) {
			payload = http.getString();
			//Serial.println("Payload: "+payload);    
		}
		//---------------------------------------------------------------------
		http.end();
		}
		else {
			//Serial.println("Erro wifi 133");
			connectWifi();
		}
	count++;
	//delay(1000);
}

void setup() {
	Serial.begin(115200);
	Serial.println("Example for 3 DHT11/22 sensors");
	/* pinMode(relePin1, OUTPUT);
	digitalWrite(relePin1, LOW); */
	connectWifi();
	// Initialize temperature sensor 1
	dhtSensor1.setup(dhtPin1, DHTesp::DHT11);
	// Initialize temperature sensor 2
	dhtSensor2.setup(dhtPin2, DHTesp::DHT11);

	// Start task to get temperature
	xTaskCreatePinnedToCore(
			tempTask,											 /* Function to implement the task */
			"tempTask ",										/* Name of the task */
			4000,													 /* Stack size in words */
			NULL,													 /* Task input parameter */
			5,															/* Priority of the task */
			&tempTaskHandle,								/* Task handle. */
			1);														 /* Core where the task should run */
	if (tempTaskHandle == NULL) {
		Serial.println("[ERROR] Failed to start task for temperature update");
	} else {
		// Start update of environment data every 30 seconds
		tempTicker.attach(intervalo_leitura, triggerGetTemp);
	}
	// Signal end of setup() to tasks
	tasksEnabled = true;
} // End of setup.


/**
 * loop
 * Arduino loop function, called once 'setup' is complete (your own code
 * should go here)
 */

void loop() {
	if(WiFi.status() != WL_CONNECTED) connectWifi();
	if (gotNewTemperature) {
		Serial.print(String(count) + "\t");
		Serial.print(String(sensor1Data.temperature,2) + "\t" + String(sensor1Data.humidity,1) + "\t");
		Serial.println(String(sensor2Data.temperature,2) + "\t" + String(sensor2Data.humidity,1) + "\t" + WiFi.status());

		//Serial.println(String(sensor2Data.temperature,2) + "\t" + String(sensor2Data.humidity,1) + "\t" + Celsius);
		//Serial.println("\t" + WiFi.status());
		gotNewTemperature = false;
/* 		if(sensor1Data.temperature <= 39.9){
			digitalWrite(relePin1, HIGH);
		}else if(sensor1Data.temperature >= 41.9){
			digitalWrite(relePin1, LOW);
		}
 */		loopDataLog();
	}
} // End of loop