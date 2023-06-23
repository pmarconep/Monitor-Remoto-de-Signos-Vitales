//Librerias

#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"
#include <StarterKitNB.h>

// Este código esta diseñado para leer signos vitales (RPM, BPM, Temperatura y SpO2) con una BaseBoard RAK19001 y un procesador Espressif ESP32-WROVER. El código considera el uso
// de 2 sensores y 3 modulos RAK.

// Sensores:
// - Sensor cardiaco MAX30102. El código debería funcionar con cualquier sensor de la familia MAX3010x. Este sensor se encarga de medir BPM, Temperatura y SpO2. Utiliza el protocolo I2C y debe
// 							conectarse a los pines default de la board. Es alimentado con 3.3V

// - Sensor de fuerza SF15-600. Este sensor es una simple resistencia variable dependiente de la tension de su superficie. El código debería funcionar con cualquier sensor que cumpla estas
//                              caracteristicas. Este se conecta al módulo RAK 4-20 mA. Por un lado a la salida 12V del módulo. Y por el otro a la conexion A1. Este sensor se encarga de calcular RPM.

// Módulos:
// - Módulo 4-20 mA RAK5801. Utilizad para comunicarse con SF15-600.

// - Módulo Buzzer RAK18001. No implementado hasta ahora

// - Módulo NB-IoT RAK5860. Utilizado para conectarse a la red NB de Entel.


// El programa inicializa todos los sensores y realiza una medicion con cada sensor. Revisa que las 4 mediciones sean correctas. En caso de ser correctas envía el mensaje a ThingsBoard con los datos.
// En caso de ser valores fuera de rangos. Se hace un nuevo intento. En total se llegan a hacer 4 intentos de medir. En caso de no lograr ninguna medición. El programa envía "-1" para poder ser interpretado como
// medicion erronea. Después de esto el programa duerme por el tiempo deseado (10 minutos) para realizar una nueva medicion. 

//Objetos

MAX30105 particleSensor; //Sensor BPM
StarterKitNB sk; //Modulo NB

//Constantes para modulo 4-20 mA

int NO_OF_SAMPLES = 128; //Configura la cantidad de sampleos para una medicion
int rpmRead; //Para guardar medicion RPM

//Constantes para sensor BPM

int min_bpm = 20; //Rangos para que la medicion de beats per minute sea considerada aceptable
int max_bpm = 250;

float min_temp = 20; //Rangos para que medicion de temperatura sea aceptable
float max_temp = 60;

int bpmRead; //Para guardar medicion BPM
int tempRead; //Para guardar medicion Temperatura
int fingerRead; //Para guardar medicion Presencia Dedo

//Constantes Avg BPM

const byte RATE_SIZE = 4; //Cantidad de samples que se utilizan para una medicion
byte rates[RATE_SIZE]; //Arreglo de medicion
byte rateSpot = 0;
long lastBeat = 0; //Para guardar el tiempo en que se midio el ultimo latido

float beatsPerMinute; //Para guardar BPM
int beatAvg; //Para guardar AvgBPM

//Constantes SpO2

#define MAX_BRIGHTNESS 255

uint32_t irBuffer[100]; //Array para medidas sensor IR
uint32_t redBuffer[100];  //Array para medidas sensor LED ROJO

int32_t bufferLength; //Variables auxiliares
int32_t spo2; //Para guardar spo2
int8_t validSPO2; //Indica si la medicion es valida
int32_t heartRate; //Para guardar heart rate (NO SE UTILIZA, PERO SE NECESITA PARA LA FUNCION DE SPO2)
int8_t validHeartRate; //Inidica si la medicion es valida

int spo2Read;

//Constantes para sensor fuerza

int min_rpm = 3; //Rangos para que la medicion de RPM se considere aceptable
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

#define BUZZER_CONTROL WB_IO4;

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

///////////////////////////
//FUNCIONES DE INICIACION//
///////////////////////////

void init_bpm() { //Inicio sensor BPM

	Serial.println("Iniciando sensor BPM");

    	if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Inicia protocolo I2C en puertos default a 400kHz
	{
		Serial.println("No se ha encontrado el sensor MAX30102. Revisa el cableado. \n");
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

	Serial.print("\nIniciando modulo de corriente \n");

	pinMode(WB_IO1, OUTPUT | PULLUP); //Configura pines para modulo 4-20 mA
	digitalWrite(WB_IO1, HIGH);
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

/////////////////////////////
//Funciones de Comunicacion//
/////////////////////////////

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

////////////////////////////////////
// Funcion Medicion de Temperatura//
////////////////////////////////////

int getTemperature() {

	Serial.print("Iniciando medicion de temperatura \n");
	
	particleSensor.setup(0); //Configura sensor con todas las LED apagadas para que medicion sea mas precisa
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

///////////////////////////////////////////
// Funcion determinacion si hay dedo o no//
///////////////////////////////////////////

int getFinger() {

	long irValue = particleSensor.getIR(); //Obtiene valor infrarojo sensor BPM

	if (irValue > 50000) { //Chequea que este valor sea mayor a 50000. Lo cual implica que hay un dedo o algo a alta temperatura en el sensor
		return 1;
	} else {
		return 0;
	}
}

////////////////////////////////
//Funcion Ritmo Cardiaco (BPM)//
////////////////////////////////

int getAvgBPM() {
	if (getFinger() == 1){ //Inicia medicion si detecta dedo

		Serial.print("Iniciando medicion BPM \n");

		int end_time = millis() + 30000; //Tiempo para medicion (30 segundos) (Quizas hacerla soft coded)

		while (millis() < end_time) { //Inicia medicion durante 30 segundos

			long irValue = particleSensor.getIR(); //Obtiene valor IR

			if (checkForBeat(irValue) == true) //Detecta si hay latido
			{
				long delta = millis() - lastBeat;
				lastBeat = millis(); //Calcula el tiempo desde el ultimo latido 

				beatsPerMinute = 60 / (delta / 1000.0); //Obtiene valor BPM

				if (beatsPerMinute < max_bpm && beatsPerMinute > min_bpm) //Chequea rango de medicion
				{
				rates[rateSpot++] = (byte)beatsPerMinute; //Guarda en array
				rateSpot %= RATE_SIZE; 

				
				beatAvg = 0; //Obtiene promedio de array
				for (byte x = 0 ; x < RATE_SIZE ; x++)
					beatAvg += rates[x];
				beatAvg /= RATE_SIZE;
				}
			}
		}

		Serial.print("Medicion finalizada: " + String(beatAvg) + "\n \n"); //Returns
		return beatAvg;
	} else {
		Serial.print("Medicion fallida. Dedo no detectado \n \n");
		return -1;
	}
}

//////////////////////////////////////////
//Funciones Saturacion de Oxigeno (SpO2)//
//////////////////////////////////////////

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

	Serial.print("Iniciando medicion de SpO2 \n");

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

///////////////////////////
//Funciones Sensor Fuerza//
///////////////////////////

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

////////////////////
//Funciones Buzzer//                               NO IMPLEMENTADAS
////////////////////

// void error_nofinger() { //Genera sonido de error en buzzer
 
// }

// void error_nobpm() {

// }

// void error_nocomms() {

// }

// void error_rpm() {

// }

// void install_mode() { //Ruido para identificar que se esta en install mode

// }

// void normal_mode() { //Ruido para identificar que se entro a modo normal

// }


// //Sensor Analogo de dolor

// //Install Mode

// void installTester() {
// 	//Loop en que se confirme que estan todos los sensores funcionando correctamente
// }

//////////////
//Sleep Mode//
//////////////

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

		error_status = 0;

		if (bpmRead < 0) {
			error_status = 1;
			//error_nobpm();
		}

		if (rpmRead < 0) {
			error_status = 1;
			//error_rpm();
		}

		if (tempRead < 0) {
			error_status = 1;
			//error_nofinger();
		}

		if (spo2Read < 0) {
			error_status = 1;
			//error_nobpm();
		}

		if (error_status == 1) {
			++attemps;
		} else {
			break;
		}
	}

	msg = "{\"temp\": " +String(tempRead)+ ",\"bpm\": " +String(bpmRead)+ ",\"finger\": " +String(fingerRead)+ ",\"rpm\": " +String(rpmRead)+ ",\"spo2\": " +String(spo2Read)+ ", \"status\": " + String(error_status)+ "}";

	init_comms();
	sendMsg();
	Serial.print("\n \n"+ msg + "\n \n");

	enterSleep();
	
	delay(2000);
}

/////////////////////////////////////////////////////