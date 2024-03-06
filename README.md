**RESUMEN DEL PROYECTO**

El objetivo principal de este proyecto es el estudio y diseño de un sistema de bicicleteros seguros a implementarse en la Universidad Nacional de General San Martín (UNSAM), particularmente, en el Campus Miguelete. Se pretende dar soluciones al personal docente y no docente, facilitando su transporte y seguridad a la hora de dejar las bicicletas en el predio.

**ORGANIZACIÓN DEL REPO**

*Main*

El el Main se va a enocntrar el archvio .apk que permite descargarse la APP UNSAM Bicis Seguras. 

*Proyecto_3*

En este branch se van a encontrar los archivos candado_v8_proyecto_3 y transceptor_v8_proyecto_3. Estos archvios contienen el código utilizado para programar las placas Heltec WiFi LoRa 32 V3. Para visualizarlos y correrlos es necesario tener instalado el Arduino IDE junto con las librerías mencionadas a continaución: 

  - Firebase ESP32 Client, by Mobitz
  - Heltec ESP32 Dev-boards, by Heltect Automation
  - Keypad, by Mark Stanley

Además es necesario instalar el board Heltec ESP32 Series Dev-boards. Para más información consultar el siguiente links: https://docs.heltec.org/en/node/esp32/esp32_general_docs/quick_start.html 

*Proyecto Final*

En este branch se encontraran los archivos que permiten programar las placas Heltec WiFi LoRa 32 V3 para funcionar como candado y transceptor. Para visualizarlos y correrlos es necesario tener instalado el Arduino IDE junto con las librerías mencionadas a anteriormente sumado a este: 

  - ESP32 Servo, by Kevin Harrington y John K. Bennett.

La diferencia con los archivos de Proyecto 3 está en la incroporación de la alarma y del control del pestillo de seguridad con el servo motor. 
