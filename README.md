# Monitor de Signos Vitales

Este código esta diseñado para leer signos vitales (RPM, BPM, Temperatura y SpO2) con una BaseBoard RAK19001 y un procesador Espressif ESP32-WROVER. El código considera el uso
de 2 modulos RAK y 2 sensores.

## Datos de contacto
- Pascual Marcone - pascual.marcone@ug.uchile.cl
- Ignacio Díaz - ignacio.diaz.cuevas@ug.uchile.cl
- Juan Pablo Contreras - juan.contreras.v@ug.uchile.cl

## Módulos
- Módulo 4-20 mA RAK5801. Utilizad para comunicarse con SF15-600.

- Módulo NB-IoT RAK5860. Utilizado para conectarse a la red NB de Entel.

## Sensores
- Sensor cardiaco MAX30102. El código debería funcionar con cualquier sensor de la familia MAX3010x. Este sensor se encarga de medir BPM, Temperatura y SpO2 (solo algunos de los sensores MAX3010x soportan SpO2). Este sensor utiliza el protocolo I2C (SDA y SCL) y debe conectarse a los pines default de I2C de la board. Es alimentado con 3.3V.
  
![Screenshot 2023-07-20 123352](https://github.com/pmarconep/Monitor-Remoto-de-Signos-Vitales-Grupo-5/assets/49997440/962f52e7-2624-4122-9e9f-2242309dd279)

- Sensor de fuerza SF15-600. Este sensor es una simple resistencia variable dependiente de la tension de su superficie. El código debería funcionar con cualquier sensor que cumpla estas caracteristicas. Este sensor tiene dos pines. Uno se conecta al módulo RAK 4-20 mA mediante el pin A1 para realizar una medición análoga del sensor. El ssegundo se alimenta con 12V, pin de voltaje que ofrece módulo 4-20 mA. Este sensor se utiliza para medir frecuencia respiratoria (RPM).

## Funcionamiento

El core del programa realiza mediciones de todas las variables. Posterior a esto se realiza un chequeo de que los resultados sean válidos y no esté fallando un sensor. En este caso, se realiza un nuevo intento de medición. De fallar 3 veces, el programa envía a ThingsBoard un mensaje de medición incorrecta. 

Cuando ya envía el mensaje, sea con mediciones exitosas o fallidas, espera el tiempo definido definido en delay_medicion

El programa mide 7 variables con los distintos sensores y envía estas mediante Narrow Band.

Para el funcionamiento de la comunicación Narrow Band, se debe tener la librería "StarterKitNB", y se debe modificar la ruta a esta libreria en el archivo "platformio.ini".

### Sensor MAX3010x
- Frecuencia Cardiaca (bpmRead): Realiza la medición durante 30 segundos para obtener un buen promedio.
  
- Temperatura (tempRead): Mide la temperatura de sensor como tal. Esto no es ideal para medir temperatura corporal. Se recomienda incorporar sensor para medir temperatura corporal.
  
- Oxigenación (spo2Read): Realiza la medición durante 30 segundos para obtener un buen promedio.
  
- Presencia humana (finger): Chequea si se detecta un ser humano mediante el output de la LED IR del sensor. 

### Sensor SF15-600
- Frecuencia Respiratoria (rpmRead): Mide durante 30 segundos y calcula la cantidad de veces que se pasa el nivel configurado de respiración. Entrega el valor multiplicado por dos.

### Varias
- Status de Medición (error_status): 0 si es que todas las mediciones están dentro de rangos esperados. 1 si algúna medición se detecta erronea después de los 3 intentos.
  
- Nivel de Batería (battery_percentage): Entrega el porcentaje de batería disponible en el dispositvo.

## Observaciones

- Se encuentra además un dashboard tipo para la integración del dispositivo con ThingsBoard.

## Links

- Tutorial como conectar con MAX301x - https://lastminuteengineers.com/max30102-pulse-oximeter-heart-rate-sensor-arduino-tutorial/#:~:text=MAX30102%20Module%20Pinout,-The%20MAX30102%20module&text=You%20can%20connect%20it%20to,an%20interrupt%20for%20each%20pulse

- Datasheet MAX30102 - https://www.analog.com/media/en/technical-documentation/data-sheets/MAX30102.pdf
  
- Datasheet SF15-600 - https://xonstorage.blob.core.windows.net/pdf/4158958_SF1560010kg_link.pdf

- Datasheet RAK19001 - https://docs.rakwireless.com/Product-Categories/WisBlock/RAK19001/Datasheet/

- Datahseet ESP32-WROVER - https://docs.rakwireless.com/Product-Categories/WisBlock/RAK11200/Datasheet/

- Datasheet RAK5801 (4-20 mA) - https://docs.rakwireless.com/Product-Categories/WisBlock/RAK5801/Datasheet/

- Datasheet RAK5860 (Narrow Band) - https://docs.rakwireless.com/Product-Categories/WisBlock/RAK5860/Datasheet/
