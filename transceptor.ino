/*
 * PFIT: "UNSAM Bicis Seguras"
 * Acher Moira - Ibero Lucio
 * Código del "Transceptor"
 *
 */
 
//Librerias utilizadas
#include <heltec.h> //Librería utilizada para el uso de la placa de desarrollo Heltec
#include "LoRaWan_APP.h"
#include "WiFi.h" // Librería utilizada para la conexión WiFi
#include <FirebaseESP32.h> //Librería utilizada para la conexión entre la placa de desarrollo Heltec y Firebase
#include "addons/TokenHelper.h" //Librería auxiliar de FirebaseESP32. Permite autenticarse y conectarse a Firebase utilizando la autenciación por Email y Password. 

//Configuración de los parámetros LoRa
#define RF_FREQUENCY                                915000000 // Hz
#define TX_OUTPUT_POWER                             14        // dBm
#define LORA_BANDWIDTH                              0         // [0: 125 kHz, 1: 250 kHz, 2: 500 kHz, 3: Reservado]
#define LORA_SPREADING_FACTOR                       12        // [SF7..SF12]
#define LORA_CODINGRATE                             4         // [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Tx & Rx
#define LORA_SYMBOL_TIMEOUT                         100       // Simbolos
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false
#define BUFFER_SIZE                                 10 

char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];
static RadioEvents_t RadioEvents;

// Definición de los objetos para Firebase
FirebaseData firebase_data;
FirebaseConfig firebase_config;
FirebaseAuth firebase_auth;
const char *candado_nodo = "Tornavias1"; // Nombre del nodo de DB que refiere al candado


//Variables utilizadas para comparación/lógica del código
bool lora_idle=true;					// Permite saber si el módulo LoRa está listo para Tx
bool mandarToken=true;					// Detiene o inicializa el envio de tokens
bool alarmaSeteadaDB=false;				// Se verifica su valor para enviar la notificación push
bool recienLiberada=false;				// Se verifica que el usuario acaba de desbloquear su bicicleta (evita recibir paquetes no deseados)
String tokenActual="";					// Almacena el valor del campo "token" de la DB (para compararlo con el próximo valor)

//Leo el valor del token de Firebase (antes de que el candado esté en uso) cada 3 segundos
unsigned long lastExecutionTime = 0;
const unsigned long executionInterval = 3000; // Intervalo de ejecución en milisegundos

//Inicialización del "Transceptor"
void setup() {
	// Inicialización del microcontrolador (pantalla OLED y conexión WiFi-Firebase) 
	Mcu.begin();
	Heltec.begin(true, false, true);		
	Heltec.display->setFont(ArialMT_Plain_16);
	Heltec.display->clear();
	Heltec.display->drawString(0, 0, "Búsqueda WiFi");
	Heltec.display->display();
    Serial.begin(9600);   	// Inicialización de la comunicación serial
    Serial.println("Iniciando búsqueda WiFi");
	WiFi.disconnect(true); 			// Desconectamos la red anterior
	WiFi.mode(WIFI_STA); 			// Modo "station"
	WIFI_Firebase_Setup();

	// Inicialización de LoRa
	RadioEvents.TxDone = OnTxDone;
	RadioEvents.RxDone = OnRxDone;
	Radio.Init( &RadioEvents );
	Radio.SetChannel( RF_FREQUENCY );
	Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
						LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                        LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                        true, 0, 0, LORA_IQ_INVERSION_ON, 3000 ); 

	Radio.SetChannel(RF_FREQUENCY);
	Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                        LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                        LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                        0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
}

void loop(){
  unsigned long currentTime = millis();
	if(lora_idle && mandarToken) {			// Si está en modo Tx y se están generando tokens
    if (currentTime - lastExecutionTime >= executionInterval) {
      // Ejecutar el código cada 3 segundos
      lastExecutionTime = currentTime;
      String tokenNuevo=leerTokenDB();	// Almacenamos el valor del campo "token" de la DB
      if (tokenNuevo!=tokenActual){		// Si cambió el valor del token->se envia el nuevo código
        mandarTokens(tokenNuevo);
        tokenActual=tokenNuevo;
      }
    }
  }
	Radio.IrqProcess();
}


void WIFI_Firebase_Setup(void) {
	// Búsqueda WiFi
	WiFi.setAutoConnect(true);
	// La línea a continuación se utiliza para establecer la conexión WiFi a la placa de desarrollo Heltec WiFi 32 LoRa V3. Descomentela e ingrese los datos pedidos. 
  //WiFi.begin("Ingrese el SSID de la Red WiFi","Ingrese la contraseña de la Red WiFi");
	delay(100);

	while(WiFi.status() != WL_CONNECTED){
		Serial.println("Conectandose...");
    // La línea a continuación se utiliza para establecer la conexión WiFi a la placa de desarrollo Heltec WiFi 32 LoRa V3. Descomentela e ingrese los datos pedidos. 
    //WiFi.begin("Ingrese el SSID de la Red WiFi","Ingrese la contraseña de la Red WiFi");}
    delay(500);
	}

	if(WiFi.status() == WL_CONNECTED){
		Serial.println("Conectado");
		Heltec.display->drawString(0, 30, "Conectado");
		Heltec.display->display();
		Serial.println("WiFi Setup OK");
	}

	// Configuramos la conexión a Firebase. Descomente las líneas e ingrese los datos pedidos. 
	/*
  firebase_config.database_url = "Ingrese la URL a la base de datos";
	firebase_config.api_key = "Igrese el API Key de la base de datos";
	firebase_auth.user.email = "Ingrese el email de usuario con el que quiere conectarse a la base de datos";
  firebase_auth.user.password = "Ingrese la contraseña asociada al email ingresado";
  */
	firebase_config.token_status_callback = tokenStatusCallback;
	Firebase.begin(&firebase_config, &firebase_auth);
	Firebase.reconnectWiFi(true);
	Serial.println("Firebase Setup OK");
}


void OnTxDone( void ){
	if (recienLiberada==false){				// Verificamos que la bicicleta no haya sido recientemente liberada
		Serial.println("\nTX realizada");
		Radio.Rx(0);     					// Se pone el módulo en modo recepción
	}
	else {
		recienLiberada=false;				// Se liberó la bicicleta recientemente, se debe reiniciar el envio de tokens para que otro usuario pueda usar el candado.
	}
}


void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr ){
	memset(rxpacket, 0, BUFFER_SIZE);   // Limpiamos el buffer antes de copiar el nuevo paquete
	memcpy(rxpacket, payload, size );	
  Serial.println("\nRecibí: ");
	Serial.println(rxpacket);
	// Verificamos si el paquete recibido es la señal de bloqueo
	if (isBloqueo(rxpacket)) { 
		mandarBloqueo(); 
	}
	// Verificamos si está activa la alarma en DB. Si no lo está y se recibió la señal de alarma, se debe enviar la notificación push en la APP
	if (alarmaSeteadaDB == false){
		if (isAlert(rxpacket)){
			mandarAlerta(); 
		}
	}
  // Verificamos si el paquete recibido es el token de desbloqueo
  if (mandarToken==false){    
    liberarBicicleta(rxpacket);
  }
}


// Función para verificar si el paquete recibido contiene el texto "Block"
bool isBloqueo(char* packet) {
	if (strstr(packet, "Block") != NULL) {
		Serial.printf("\r\n Bloqueo recibido\n");
		return true; 
	}
	return false;
}



// El paquete recibido es el bloqueo (se introdujo correctamente el token en el candado), seteamos "tokenvalidado" en TRUE en la DB. 
void mandarBloqueo() {
	String firebase_path = "candados/" + String(candado_nodo) + "/tokenvalidado";
	Firebase.setBool(firebase_data, firebase_path.c_str(),true);
	Serial.println("Bloqueo realizado en la DB");
	mandarToken=false;  // Se detiene el envío de tokens
	lora_idle=true;
}


// Función para verificar si el paquete recibido contiene el texto "Alerta"
bool isAlert(char* packet) {
	if (strstr(packet, "Alerta") != NULL) {
		Serial.printf("\r\n Alerta recibida\n");
		return true; 
	}
	return false; 
}


// El paquete recibido es la alerta (se intentó vulnerar el candado), seteamos "alarma" en TRUE en la DB. 
void mandarAlerta() {
	String alert_path = "candados/" + String(candado_nodo) + "/alarma";
	Firebase.setBool(firebase_data, alert_path.c_str(), true);
	Serial.println("Flag alarma seteado en DB");
	alarmaSeteadaDB=true;
}


void liberarBicicleta(char* packet){
  // Verificamos si el paquete contiene el texto "Liberada"
   if (strstr(packet, "Liberada") != NULL){
    String firebase_path_locked = "candados/" + String(candado_nodo) + "/locked";
    Firebase.setBool(firebase_data, firebase_path_locked.c_str(),false);
    mandarToken=true;	
    lora_idle=true;
    alarmaSeteadaDB=false;
    recienLiberada=true;
    Serial.println("Candado liberado en DB. Listo para usar nuevamente");
  }
}

// Se envia un token si ocurrió un cambio en el valor del campo "token" en DB 
void mandarTokens(String tokenNuevo) {
	sprintf(txpacket, "%s", tokenNuevo.c_str());
	Radio.Send((uint8_t *)txpacket, strlen(txpacket));
	Serial.printf("Enviando: \"%s\"", txpacket);
	Heltec.display->clear();
	Heltec.display->drawString(0, 0, "Token enviado:");
	Heltec.display->drawString(0, 30, tokenNuevo);
	Heltec.display->display();
}


// Funcion para la lectura del token de la DB
String leerTokenDB(){
	String firebase_path = "candados/" + String(candado_nodo) + "/token";
	Firebase.getString(firebase_data, firebase_path.c_str());
	String token = firebase_data.stringData();
	return token;
}
