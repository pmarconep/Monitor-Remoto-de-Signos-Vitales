//Dependencias

#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"
#include <StarterKitNB.h>

//Objetos

MAX30105 particleSensor; //Sensor BPM
StarterKitNB sk; //Modulo NB

//Constantes para modulo 4-20 mA

int NO_OF_SAMPLES = 128;
int rpmRead;

//Constantes para sensor BPM

int min_bpm = 20;
int max_bpm = 250;

float min_temp = 20;
float max_temp = 60;

int bpmRead;
int tempRead;
int fingerRead;

//Constantes Avg BPM

const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;

//Constantes SpO2

#define MAX_BRIGHTNESS 255

uint32_t irBuffer[100]; //infrared LED sensor data
uint32_t redBuffer[100];  //red LED sensor data

int32_t bufferLength; //data length
int32_t spo2; //SPO2 value
int8_t validSPO2; //indicator to show if the SPO2 calculation is valid
int32_t heartRate; //heart rate value
int8_t validHeartRate; //indicator to show if the heart rate calculation is valid

int spo2Read;

//Constantes para sensor fuerza

int min_rpm = 3;
int max_rpm = 120;

float threshold = 1000; //Nivel de fuerza en que se considera una respiracion

//Constantes NB

String msg = "";

String apn = "m2m.entel.cl";
String user = "entelpcs";
String pw = "entelpcs";
String clientId = "grupo5";
String userName = "55555";
String password = "55555";

//Constantes Buzzer

#define BUZZER_CONTROL  WB_IO4;

//////////////////////////////////////////////////////////////////////

// FUNCIONES //

//////////////////////////////////////////////////////////////////////


// FUNCIONES DE INICIACION

void init_bpm() { //Inicio sensor BPM

	Serial.println("Iniciando sensor BPM \n");

    	if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Inicia protocolo I2C en puertos default a 400kHz
	{
		Serial.println("No se ha encontrado el sensor MAX30102. Revisa el cableado. \n");
		while (1);
	}

	particleSensor.setup(); //Configura sensor a configurcion default
	particleSensor.setPulseAmplitudeRed(0x0A);
	particleSensor.setPulseAmplitudeGreen(0); //Apaga LED verde
	particleSensor.enableDIETEMPRDY(); //Permite leer la temperatura del sensor

	time_t timeout = millis(); //Debug
	while (!Serial)
	{
		if ((millis() - timeout) < 5000)
		{
            delay(100);
        }
        else
        {
            break;
        }
	}

	Serial.print("Sensor BPM iniciado con exito \n \n");
}

void init_current() { //Inicio modulo corriente 4-20 mA

	Serial.print("Iniciando modulo de corriente \n");

	pinMode(WB_IO3, OUTPUT | PULLUP); //Configura pines para modulo 4-20 mA
	digitalWrite(WB_IO3, HIGH);
	adcAttachPin(WB_A1);
	analogSetAttenuation(ADC_11db);
	analogReadResolution(12);

	Serial.print("Modulo de corriente iniciado con exito \n \n");

}

void init_comms(){ //Inicio conecion NB Entel

    sk.Setup(true); //Conexion a red NB
	delay(500);
	sk.UserAPN(apn,user,pw);
	delay(500);
	sk.Connect(apn);
}

//Funciones de Comunicacion

void sendMsg() { //Funcion para enviar mensaje

	if (!sk.ConnectionStatus()) //Chequea conexion a red NB
	{
		sk.Reconnect(apn);
	}

	if (!sk.LastMessageStatus) //Chequea conexion ThingsBoard
	{
		sk.ConnectBroker(clientId,userName,password);
	}
	
	sk.SendMessage(msg); //Envia mensaje
}

// Funcion Medicion de Temperatura

int getTemperature() {

	Serial.print("Iniciando medicion de temperatura \n");
	
	particleSensor.setup(0);
	delay(3000);

	float temperature = particleSensor.readTemperature(); //Obtiene valor temperatura de sensor BPM

	if (temperature > min_temp && temperature < max_temp) { //Chequea que valor este dentro de rango esperado
		Serial.print("Medicion de temperatura realizada: " + String(temperature) + "\n \n");
		particleSensor.setup();
		return temperature;
	} else {
		Serial.print("Medicion de temperatura incorrecta. Revise sensor BPM. \n \n");
		particleSensor.setup();
		return -1;
	}
}

// Funcion determinacion si hay dedo o no

int getFinger() {

	long irValue = particleSensor.getIR(); //Obtiene valor infrarojo sensor BPM

	if (irValue > 50000) { //Chequea que este valor sea mayor a 50000. Lo cual implica que hay un dedo o algo a alta temperatura en el sensor
		return 1;
	} else {
		return 0;
	}
}

//Funcion Ritmo Cardiaco (BPM)

int getAvgBPM() {
	if (getFinger() == 1){

		Serial.print("Iniciando medicion BPM \n");

		int end_time = millis() + 30000;

		while (millis() < end_time) {

			long irValue = particleSensor.getIR();

			if (checkForBeat(irValue) == true)
			{
				//We sensed a beat!
				long delta = millis() - lastBeat;
				lastBeat = millis();

				beatsPerMinute = 60 / (delta / 1000.0);

				if (beatsPerMinute < 255 && beatsPerMinute > 20)
				{
				rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
				rateSpot %= RATE_SIZE; //Wrap variable

				//Take average of readings
				beatAvg = 0;
				for (byte x = 0 ; x < RATE_SIZE ; x++)
					beatAvg += rates[x];
				beatAvg /= RATE_SIZE;
				}
			}
		}

		Serial.print("Medicion finalizada: " + String(beatAvg) + "\n \n");
		return beatAvg;
	} else {
		Serial.print("Medicion fallida. Dedo no detectado \n \n");
		return -1;
	}
}

//Funciones Saturacion de Oxigeno (SpO2)

void setupSpO2() {
	byte ledBrightness = 60; //Options: 0=Off to 255=50mA
	byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
	byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
	byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
	int pulseWidth = 411; //Options: 69, 118, 215, 411
	int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

	particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
}

int getSpO2() {

	Serial.print("Iniciando medicion de SpO2");

	setupSpO2();
	int end_time = millis() + 30000;

	bufferLength = 100; //buffer length of 100 stores 4 seconds of samples running at 25sps

	//read the first 100 samples, and determine the signal range
	for (byte i = 0 ; i < bufferLength ; i++)
	{
		while (particleSensor.available() == false) //do we have new data?
		particleSensor.check(); //Check the sensor for new data

		redBuffer[i] = particleSensor.getRed();
		irBuffer[i] = particleSensor.getIR();
		particleSensor.nextSample(); //We're finished with this sample so move to next sample
	}

	//calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
	maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
	
	while (millis() < end_time){
		//dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
		for (byte i = 25; i < 100; i++)
		{
		redBuffer[i - 25] = redBuffer[i];
		irBuffer[i - 25] = irBuffer[i];
		}

		//take 25 sets of samples before calculating the heart rate.
		for (byte i = 75; i < 100; i++)
		{
		while (particleSensor.available() == false) //do we have new data?
			particleSensor.check(); //Check the sensor for new data


		redBuffer[i] = particleSensor.getRed();
		irBuffer[i] = particleSensor.getIR();
		particleSensor.nextSample(); //We're finished with this sample so move to next sample

		//send samples and calculation result to terminal program through UART
		}

		//After gathering 25 new samples recalculate HR and SP02
		maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
	}

	if (validHeartRate == 1 && validSPO2 == 1) {
		Serial.print("Medicion realizada: " + String(spo2) + "\n \n");
		return spo2;
	} else {
		Serial.print("Medicion SpO2 fallida \n \n");
		return -1;
	}
}


//Funciones Sensor Fuerza

float getCurrentRead() {

    int i; //Seteo de constantes y variables para medicion
	int sensor_pin = WB_A1; 
	int mcu_ain_raw = 0;
	int average_adc_raw;
	float voltage_mcu_ain; 
	float current_sensor;

    for (i = 0; i < NO_OF_SAMPLES; i++) //Obtiene medicion de NO_OF_SAMPLES cantidad de sampleos
	{
		mcu_ain_raw += analogRead(sensor_pin);
	}
	average_adc_raw = mcu_ain_raw / NO_OF_SAMPLES; //Promedia valor de los sampleos

	current_sensor = average_adc_raw / 149.9*1000; 

	return current_sensor;
}

int getRPM() {

    int t_resp_final = millis() + 30000; //
    float current = 0;
    int resp = 0;
    int resp_status = 0;

	Serial.print("Iniciando medicion RPM \n");

    while (millis() < t_resp_final) {
        current = getCurrentRead();

        if (current <= threshold) {
            resp_status = 0;
        }

        if (current > threshold) {
            if (resp_status == 0) {
                ++resp;
            }
            resp_status = 1;
        }
    }

	if (resp*2 > min_rpm && resp*2 < max_rpm) {
		Serial.print("Medicion RPM realizada con exito: " + String(resp*2) + "\n \n");
		return resp*2; 
	} else {
		Serial.print("Medicion RPM incorrecta. Revise que se encuentre bien instalado el sensor RPM \n \n");
		return -1;
	}
	
}

//Funciones Buzzer

void error_nofinger() { //Genera sonido de error en buzzer
 
}

void error_nobpm() {

}

void error_nocomms() {

}

void error_rpm() {

}

void install_mode() { //Ruido para identificar que se esta en install mode

}

void normal_mode() { //Ruido para identificar que se entro a modo normal

}


//Sensor Analogo de dolor

//Install Mode

void installTester() {
	//Loop en que se confirme que estan todos los sensores funcionando correctamente
}

//Sleep Mode

void enterSleep() {  //Esto entra en modo maximo de ahorro de bateria
	Serial.print("Se ha iniciado el modo hibernacion \n \n");
	particleSensor.shutDown();
}

void awakeDevice() { //Esto despierta todo lo que fue puesto en modo sleep
	Serial.print("Se ha despertado al dispositivo \n \n");
	particleSensor.wakeUp();
	init_bpm();
}



//////////////////// SETUP //////////////////////////

void setup() {
	Serial.begin(115200);
    // init_comms();
    init_current();
    init_bpm();
}

/////////////////////////////////////////////////////

////////////////// LOOP /////////////////////////////

void loop() {

	//Agregar if para install mode

	Serial.print("Iniciando Loop \n");

	int attemps = 0;
	int error_status = 0;

	awakeDevice();

	while (attemps < 4) {
		rpmRead = getRPM();
		bpmRead = getAvgBPM();
		tempRead = getTemperature();
		spo2Read = getSpO2();
		fingerRead = getFinger();

		error_status = 0;

		if (bpmRead < 0) {
			error_status = 1;
			error_nobpm();
		}

		if (rpmRead < 0) {
			error_status = 1;
			error_rpm();
		}

		if (tempRead < 0) {
			error_status = 1;
			error_nofinger();
		}

		if (spo2Read < 0) {
			error_status = 1;
			error_nobpm();
		}

		if (error_status == 1) {
			++attemps;
		} else {
			break;
		}
	}

	msg = "{\"temp\": " +String(tempRead)+ ",\"bpm\": " +String(bpmRead)+ ",\"finger\": " +String(fingerRead)+ ",\"rpm\": " +String(rpmRead)+ ",\"spo2\": " +String(spo2Read)+ ", \"status\": " + String(error_status)+ "}";
						//Falta agregar fecha y hora de medicion. Quizas GPS. Y senal de alerta despues de 20 - 40 mediciones sin dedo enviar 1 que signifique alerta.

	// sendMsg();
	Serial.print("\n \n"+ msg + "\n \n");

	enterSleep();
	
	delay(2000);
}

/////////////////////////////////////////////////////