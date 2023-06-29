//Librerias

#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"
#include <StarterKitNB.h>


//Objetos y constantes generales

MAX30105 particleSensor; //Sensor BPM
StarterKitNB sk; //Modulo NB

int delay_medicion = 300000; //Tiempo (ms) de delay entre mediciones


//Constantes para modulo 4-20 mA

int NO_OF_SAMPLES = 128; //Cantidad de sampleos para una medicion de modulo 4-20 mA


//Constantes para sensor BPM

int bpm_time = 30000; //Tiempo de medicion bpm
int spo2_time = 30000; //Tiempo de medicion spo2

int min_bpm = 20; //Valor minimo aceptable BPM
int max_bpm = 250; //Valor maximo aceptable BPM

float min_temp = 20; //Valor minimo aceptable TEMPERATURA
float max_temp = 60; //Valor maximo aceptable TEMPERATURA

int bpmRead; //Valor medicion BPM
int tempRead; //Valor medicion TEMP
int fingerRead; //Valor validacion dedo


//Constantes Avg BPM

const byte RATE_SIZE = 4; //Cantidad de samples que se utilizan para una medicion
byte rates[RATE_SIZE]; //Arreglo de medicion
byte rateSpot = 0;
long lastBeat = 0; //Tiempo de ultimo latido detectado

float beatsPerMinute; //Variable temporal de BPM
int beatAvg; //Valor medicion AvgBPM


//Constantes SpO2

#define MAX_BRIGHTNESS 255

uint32_t irBuffer[100]; //Array valores sensor IR
uint32_t redBuffer[100];  //Array valores sensor LED ROJO

int32_t bufferLength; //Variables auxiliares
int32_t spo2; //Valor medicion SpO2
int8_t validSPO2; //Validador medicion SpO2
int32_t heartRate; //Valor medicion heartRate
int8_t validHeartRate; //Validacion medicion heartRate

int spo2Read; //Valor medicion SpO2


//Constantes para sensor fuerza

int rpm_time = 30000; //Tiempo de medicion RPM

int min_rpm = 3; //Valor minimo aceptable RPM
int max_rpm = 120; //Valor maximo aceptable RPM

float threshold = 1000; //Nivel minimo para considerar una respiracion

float rpmRead; //Valor medicion RPM


//Constantes NB

String msg = "";

String apn = "m2m.entel.cl";
String user = "entelpcs";
String pw = "entelpcs";
String clientId = "grupo5";
String userName = "55555";
String password = "55555";


// Funcion para iniciar sensor BPM
// None -> None
void init_bpm() { 

	Serial.println("Iniciando sensor BPM");

    	if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Inicia protocolo I2C en puertos default a 400kHz
	{
		Serial.println("No se ha encontrado el sensor MAX30102. Revisa el cableado. \n");
	}

	particleSensor.setup(); 
	particleSensor.setPulseAmplitudeRed(0x0A);
	particleSensor.setPulseAmplitudeGreen(0); 
	particleSensor.enableDIETEMPRDY(); 

	time_t timeout = millis();

	while (!Serial) {
		if ((millis() - timeout) < 5000) {
            delay(100);
        } else {
            break;
        }
	}

	Serial.print("Sensor BPM iniciado con exito \n \n");
}

// Funcion para iniciar modulo 4-20 mA
// None -> None
void init_current() { 

	Serial.print("\nIniciando modulo de corriente \n");

	pinMode(WB_IO1, OUTPUT | PULLUP); //En esta configuracion se cambio la libreria para utilizar el IO2 e IO3
	digitalWrite(WB_IO1, HIGH);
	adcAttachPin(WB_A1);
	analogSetAttenuation(ADC_11db);
	analogReadResolution(12);

	Serial.print("Modulo de corriente iniciado con exito \n \n");
}


// Funcion para iniciar conexion NB
// None -> None
void init_comms(){ //Inicio conecion NB Entel

    sk.Setup(true); 
	delay(500);
	sk.UserAPN(apn,user,pw);
	delay(500);
	sk.Connect(apn);
}


// Funcion para enviar mensaje
// String -> None
void sendMsg(String msg) { 

	if (!sk.ConnectionStatus()) //Chequea conexion a red NB
	{
		sk.Reconnect(apn);
	}

	if (!sk.LastMessageStatus) //Chequea conexion ThingsBoard
	{
		sk.ConnectBroker(clientId,userName,password);
	}
	
	sk.SendMessage(msg); 
}


// Funcion para obtener valor temperatura
// None -> int
int getTemperature() {

	Serial.print("Iniciando medicion de temperatura \n");
	
	particleSensor.setup(0); //LEDS off
	delay(3000);

	float temperature = particleSensor.readTemperature(); 

	if (temperature > min_temp && temperature < max_temp) { //Validacion
		Serial.print("Medicion de temperatura realizada: " + String(temperature) + "\n \n");
		particleSensor.setup();
		return temperature;
	} else {
		Serial.print("Medicion de temperatura incorrecta. Revise sensor BPM. \n \n");
		particleSensor.setup();
		return -1;
	}
}


// Funcion para validar deteccion de paciente
// None -> int
int getFinger() {

	long irValue = particleSensor.getIR();

	if (irValue > 50000) { //Validacion
		return 1;
	} else {
		return 0;
	}
}


// Funcion para obtener AvgBPM
// None -> int
int getAvgBPM() {
	if (getFinger() == 1) { 

		Serial.print("Iniciando medicion BPM \n");

		int end_time = millis() + bpm_time; 

		while (millis() < end_time) { 

			long irValue = particleSensor.getIR(); 

			if (checkForBeat(irValue) == true) { //Validacion
				long delta = millis() - lastBeat;
				lastBeat = millis(); 

				beatsPerMinute = 60 / (delta / 1000.0); //Obtiene valor BPM

				if (beatsPerMinute < max_bpm && beatsPerMinute > min_bpm) //Validacion 
				{
				rates[rateSpot++] = (byte)beatsPerMinute; 
				rateSpot %= RATE_SIZE; 

				
				beatAvg = 0; //Obtiene promedio de array
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


// Funcion para configurar sensor BPM para medir SpO2
// None -> None
void setupSpO2() {
	byte ledBrightness = 60; //Options: 0=Off to 255=50mA
	byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
	byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
	byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
	int pulseWidth = 411; //Options: 69, 118, 215, 411
	int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

	particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
}


// Funcion para medir SpO2
// None -> int 
int getSpO2() {

	Serial.print("Iniciando medicion de SpO2 \n");

	setupSpO2();
	int end_time = millis() + spo2_time;

	bufferLength = 100; //buffer de mediciones

	
	for (byte i = 0 ; i < bufferLength ; i++) //Guarda primeras 100 muestras
	{
		while (particleSensor.available() == false) 
		particleSensor.check(); 

		redBuffer[i] = particleSensor.getRed();
		irBuffer[i] = particleSensor.getIR();
		particleSensor.nextSample(); 
	}

	//Calcula SpO2 sobre primeras 100 muestras
	maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
	
	while (millis() < end_time){

		for (byte i = 25; i < 100; i++) { //Borra 25 muestras mas antiguas
			redBuffer[i - 25] = redBuffer[i];
			irBuffer[i - 25] = irBuffer[i];
		}

		for (byte i = 75; i < 100; i++) { //Toma 25 nuevas muestras
	
			while (particleSensor.available() == false)
				particleSensor.check(); 


			redBuffer[i] = particleSensor.getRed();
			irBuffer[i] = particleSensor.getIR();
			particleSensor.nextSample(); 
		}

		//Con 25 nuevas muestras, recalcula SpO2
		maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
	}

	if (validHeartRate == 1 && validSPO2 == 1) { //Validacion
		Serial.print("Medicion realizada: " + String(spo2) + "\n \n");
		return spo2;
	} else {
		Serial.print("Medicion SpO2 fallida \n \n");
		return -1;
	}
}


// Funcion para obtener muestra de corriente de modulo 4-20 mA
// None -> float
float getCurrentRead() {

	//Variables auxiliares
    int i; 
	int sensor_pin = WB_A1; 
	int mcu_ain_raw = 0;
	int average_adc_raw;
	float voltage_mcu_ain; 
	float current_sensor;

    for (i = 0; i < NO_OF_SAMPLES; i++) { //Obtiene medicion de NO_OF_SAMPLES cantidad de sampleos
		mcu_ain_raw += analogRead(sensor_pin);
	}

	average_adc_raw = mcu_ain_raw / NO_OF_SAMPLES; //Promedia valor de los sampleos

	current_sensor = average_adc_raw / 149.9*1000;

	return current_sensor;
}


// Funcion para obtener RPM
// None -> float
float getRPM() {

    int t_resp_final = millis() + rpm_time; //
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

	float final_rpm = resp*(60000/rpm_time);

	if (final_rpm > min_rpm && final_rpm < max_rpm) {
		Serial.print("Medicion RPM realizada con exito: " + String(final_rpm) + "\n \n");
		return final_rpm; 
	} else {
		Serial.print("Medicion RPM incorrecta. Revise que se encuentre bien instalado el sensor RPM \n \n");
		return -1;
	}
	
}


// Funcion para poner el sistema en modo hibernacion
// None -> None
void sleepDevice() { 
	Serial.print("Se ha iniciado el modo hibernacion \n \n");
	particleSensor.shutDown();
}


// Funcion para despertar al sistema de modo hibernacion
// None -> None
void awakeDevice() {
	Serial.print("Se ha despertado al dispositivo \n \n");
	particleSensor.wakeUp();
	init_bpm();
}


// Funcion para chequear errores de medicion
// float, int, int, int -> Bool
bool checkForError(float rpmRead, int bpmRead, int tempRead, int spo2Read) {
	if (bpmRead < 0) {
		return true;
	}

	if (rpmRead < 0) {
		return true;
	}

	if (tempRead < 0) {
		return true;
	}

	if (spo2Read < 0) {
		return true;
	}

	return false;
}



//////////////////// SETUP //////////////////////////

void setup() {
	Serial.begin(115200);
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
		init_current();
		rpmRead = getRPM();

		init_bpm();
		bpmRead = getAvgBPM();
		tempRead = getTemperature();
		spo2Read = getSpO2();
		fingerRead = getFinger();

		if (checkForError(rpmRead,bpmRead,tempRead,spo2Read)) { //Validacion 
			++attemps; //Se hace un nuevo intento
		} else {
			break;
		}
	}

	msg = "{\"temp\": " +String(tempRead)+ ",\"bpm\": " +String(bpmRead)+ ",\"finger\": " +String(fingerRead)+ ",\"rpm\": " +String(rpmRead)+ ",\"spo2\": " +String(spo2Read)+ ", \"status\": " + String(error_status)+ "}";

	init_comms(); //Se hace aca porque disminuye los problemas de comunicacion...
	sendMsg(msg);
	Serial.print("\n \n"+ msg + "\n \n");

	sleepDevice();
	
	delay(delay_medicion);
}