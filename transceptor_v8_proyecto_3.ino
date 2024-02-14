#include <heltec.h>
#include "LoRaWan_APP.h"
#include "WiFi.h"
#include <FirebaseESP32.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"

//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

#define RF_FREQUENCY                                915000000 // Hz
#define TX_OUTPUT_POWER                             14        // dBm
#define LORA_BANDWIDTH                              0         // [0: 125 kHz, 1: 250 kHz, 2: 500 kHz, 3: Reserved]
#define LORA_SPREADING_FACTOR                       12         // [SF7..SF12]
#define LORA_CODINGRATE                             4         // [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         0         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false
#define RX_TIMEOUT_VALUE                            1000
#define BUFFER_SIZE                                 10 // Define the payload size here

char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];

int16_t rssi,rxSize;

bool lora_idle=true;
bool locked = false;
bool mandarToken=true;
bool recienLiberada=false;
bool alarmaSeteadaDB=false;
bool tokenValidadoDBPrev=false;
const char *candado_nodo = "Tornavias1"; // Nombre del nodo del candado
String tokenActual="";

// Objeto Firebase global
FirebaseData firebase_data;
FirebaseConfig firebase_config;
FirebaseAuth firebase_auth;

static RadioEvents_t RadioEvents;
void WIFISetUp(void);
void mandarTokens(void);
void mandarBloqueo(void);
void mandarAlerta(void);
void OnTxDone( void );
void OnRxDone ( void );
void bloquearcandado( void );

void setup() {
    Heltec.begin(true, false, true);
    Heltec.display->setFont(ArialMT_Plain_16);
    Serial.begin(115200); // Inicia la comunicación serial
    Serial.println("Iniciando búsqueda WiFi");
    Heltec.display->clear();
    Heltec.display->drawString(0, 0, "Búsqueda WiFi");
    Heltec.display->display();
    Mcu.begin();

    WIFISetUp ();

    // Configuramos la conexión a Firebase
    firebase_config.database_url = "unsam-bicis-seguras-default-rtdb.firebaseio.com";
    firebase_config.api_key = "AIzaSyDZs2d7q86g5qNU7jmTEPDs4_mMe9rdAXM";
    firebase_auth.user.email = "acher.ibero@gmail.com";
    firebase_auth.user.password = "passpfit16*";
  	
    // Assign the callback function for the long running token generation task.
    firebase_config.token_status_callback = tokenStatusCallback; //--> see addons/TokenHelper.h

    Firebase.begin(&firebase_config, &firebase_auth);
  	Firebase.reconnectWiFi(true);

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

void loop()
{
  delay(1000);
  checkFirebaseLockedStatus(); // Verifico el estado del candado en Firebase

	if(lora_idle == true && mandarToken == true) // Si está en modo de transmisión (Tx) y están generando tokens
	{
    String tokenNuevo=leerTokenDB();
    if (tokenNuevo!=tokenActual){
      mandarTokens(tokenNuevo);
      tokenActual=tokenNuevo;
    }
	}
  Radio.IrqProcess();
}



void WIFISetUp(void) //Conecto a WiFi para poder conectarme a Firebase
{
	// Set WiFi to station mode and disconnect from an AP if it was previously connected
	WiFi.disconnect(true);
	delay(100);
	WiFi.mode(WIFI_STA);
	WiFi.setAutoConnect(true);
	WiFi.begin("Fibertel WiFi329 2.4GHz","00412924680");
	delay(100);

	while(WiFi.status() != WL_CONNECTED)
	{
		delay(1000);
		Serial.println("Connecting...");
	}

	if(WiFi.status() == WL_CONNECTED)
	{
		Serial.println("Connecting...OK.");
    Heltec.display->drawString(0, 30, "Conectado");
    Heltec.display->display();
	}
	else
	{
		Serial.println("Connecting...Failed");
	}
	Serial.println("WIFI Setup done");
	delay(500);
}


void OnTxDone( void )
{
  if (recienLiberada==false){
    Serial.println("\nTX done...");
    lora_idle = true;
    Radio.Rx(0);     // Pone el módulo en modo de recepción
  }
  else {
    recienLiberada=false;
  }
}

void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr ){
  rssi=rssi;
  rxSize=size;
  memset(rxpacket, 0, BUFFER_SIZE);   // Limpiar el buffer antes de copiar el nuevo paquete
  memcpy(rxpacket, payload, size );	
  Serial.println("\nEntre en modo recepción... Recibi:  ");
  Serial.println(rxpacket);
  if (isBloqueo(rxpacket)) { 
      mandarBloqueo(); //mandarToken pasó a false, dejo de mandar despues de recibir bloqueo
  }
  if (alarmaSeteadaDB == false){
    if (isAlert(rxpacket)){
      mandarAlerta(); //aviso a firebase que ponga el flag en true para que se active la notifacion push
    }
  }
}

// Función para verificar si el paquete recibido es un bloqueo
bool isBloqueo(char* packet) {
    if (strstr(packet, "Block") != NULL) {
      Serial.printf("\r\n Bloqueo recibido\n");
    	return true; 
    }
	return false; // El paquete no contiene "Block"
}

void mandarBloqueo() {
  // El paquete recibido es el bloqueo, actualizo la base de datos y detengo el envío de tokens
	String firebase_path = "candados/" + String(candado_nodo) + "/tokenvalidado";
	Firebase.setBool(firebase_data, firebase_path.c_str(),true);
	Serial.println("Bloqueo realizado en la DB");
  mandarToken=false; // Dejo de enviar tokens
  lora_idle=true;
}

// Función para verificar si el paquete recibido es una alerta
bool isAlert(char* packet) {
    if (strstr(packet, "Alerta") != NULL) {
      Serial.printf("\r\n Alerta recibida\n");
    	return true; 
    }
	return false; // El paquete no contiene "Alerta"
}

void mandarAlerta() {
  // Marca la alarma en Firebase Realtime Database
	String alert_path = "candados/" + String(candado_nodo) + "/alarma";
	Firebase.setBool(firebase_data, alert_path.c_str(), true);
	Serial.println("Flag alarma seteado en DB");
  alarmaSeteadaDB=true;
}

void checkFirebaseLockedStatus() {
  String firebase_path = "candados/" + String(candado_nodo) + "/locked";
  Firebase.getBool(firebase_data, firebase_path.c_str());
  bool lockedDB = firebase_data.boolData(); 

  String firebase_path_token_validado = "candados/" + String(candado_nodo) + "/tokenvalidado";
  Firebase.getBool(firebase_data, firebase_path_token_validado.c_str());
  bool tokenValidadoDB = firebase_data.boolData();

  // Verifica si ha habido un cambio en el estado de "locked" o "tokenvalidado" de la DB
  if ((locked!=lockedDB) && (tokenValidadoDB!=tokenValidadoDBPrev)) {
    liberaronBici();
    mandarToken=true;
    lora_idle=true;
    alarmaSeteadaDB=false;
    recienLiberada=true;
  }
  tokenValidadoDBPrev = tokenValidadoDB;   // Actualiza el estado previo de "tokenvalidado"
  locked = lockedDB;   // Actualiza el estado de "locked"
}

void liberaronBici(){
  String mensaje = "Liberada";
  Radio.Send((uint8_t*)mensaje.c_str(), mensaje.length());
  Serial.println("Liberando Bici");
}

void mandarTokens(String tokenNuevo) {
  sprintf(txpacket, "%s", tokenNuevo.c_str());
  Serial.printf("Sending packet \"%s\"", txpacket);
  Radio.Send((uint8_t *)txpacket, strlen(txpacket));
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "Token enviado:");
  Heltec.display->drawString(0, 30, tokenNuevo);
  Heltec.display->display();
}

String leerTokenDB(){
  String firebase_path = "candados/" + String(candado_nodo) + "/token";
  Firebase.getString(firebase_data, firebase_path.c_str());
  String token = firebase_data.stringData();
  return token;
}