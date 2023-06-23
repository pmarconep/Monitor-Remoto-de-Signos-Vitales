# Monitor de Signos Vitales


Este código esta diseñado para leer signos vitales (RPM, BPM, Temperatura y SpO2) con una BaseBoard RAK19001 y un procesador Espressif ESP32-WROVER. El código considera el uso
de 2 sensores y 3 modulos RAK.

## Sensores:
- Sensor cardiaco MAX30102. El código debería funcionar con cualquier sensor de la familia MAX3010x. Este sensor se encarga de medir BPM, Temperatura y SpO2. Utiliza el protocolo I2C y debe
							              conectarse a los pines default de la board. Es alimentado con 3.3V

- Sensor de fuerza SF15-600. Este sensor es una simple resistencia variable dependiente de la tension de su superficie. El código debería funcionar con cualquier sensor que cumpla estas
                             caracteristicas. Este se conecta al módulo RAK 4-20 mA. Por un lado a la salida 12V del módulo. Y por el otro a la conexion A1. Este sensor se encarga de calcular RPM.

## Módulos:
- Módulo 4-20 mA RAK5801. Utilizad para comunicarse con SF15-600.

- Módulo Buzzer RAK18001. No implementado hasta ahora

- Módulo NB-IoT RAK5860. Utilizado para conectarse a la red NB de Entel.

## Código

El programa inicializa todos los sensores y realiza una medicion con cada sensor. Revisa que las 4 mediciones sean correctas. En caso de ser correctas envía el mensaje a ThingsBoard con los datos.
En caso de ser valores fuera de rangos. Se hace un nuevo intento. En total se llegan a hacer 4 intentos de medir. En caso de no lograr ninguna medición. El programa envía "-1" para poder ser interpretado como
medicion erronea. Después de esto el programa duerme por el tiempo deseado (10 minutos) para realizar una nueva medicion. 
