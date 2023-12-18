#include <heltec.h>
#include "LoRaWan_APP.h"
#include "WiFi.h"
#include <FirebaseESP32.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"

//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"


#define RF_FREQUENCY                                915000000 // Hz
#define TX_OUTPUT_POWER                             5        // dBm
#define LORA_BANDWIDTH                              0         // [0: 125 kHz,
                                                              //  1: 250 kHz,
                                                              //  2: 500 kHz,
                                                              //  3: Reserved]
#define LORA_SPREADING_FACTOR                       7         // [SF7..SF12]
#define LORA_CODINGRATE                             1         // [1: 4/5,
                                                              //  2: 4/6,
                                                              //  3: 4/7,
                                                              //  4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         0         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false
#define RX_TIMEOUT_VALUE                            1000
#define BUFFER_SIZE                                 30 // Define the payload size here

char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];

double txNumber;
bool lora_idle=true;
const char *candado_nodo = "Tornavias1"; // Nombre del nodo del candado

// Objeto Firebase global
FirebaseData firebase_data;
FirebaseConfig firebase_config;
FirebaseAuth firebase_auth;

//======================================== Millis variable to read data from firebase database.
unsigned long sendDataPrevMillis = 0;
const long intervalMillis = 10000; //--> Read data from firebase database every 10 seconds.
//======================================== 

// Boolean variable for sign in status.
bool signupOK = false;

static RadioEvents_t RadioEvents;
void OnTxDone( void );
void OnTxTimeout( void );
void WIFISetUp(void) //Conecto a WiFi para poder conectarme a Firebase
{
	// Set WiFi to station mode and disconnect from an AP if it was previously connected
	WiFi.disconnect(true);
	delay(100);
	WiFi.mode(WIFI_STA);
	WiFi.setAutoConnect(true);
	WiFi.begin("Fibertel WiFi329 2.4GHz","00412924680");
	delay(100);

	byte count = 0;
	while(WiFi.status() != WL_CONNECTED)
	{
		//count ++;
		delay(1000);
		Serial.println("Connecting...");
	}

	if(WiFi.status() == WL_CONNECTED)
	{
		Serial.println("Connecting...OK.");
    Heltec.display->drawString(0, 30, "Conectado");
    Heltec.display->display();
//		delay(500);
	}
	else
	{
		Serial.println("Connecting...Failed");
		//while(1);
	}
	Serial.println("WIFI Setup done");
	delay(500);
}


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

    txNumber=0;

    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    
    Radio.Init( &RadioEvents );
    Radio.SetChannel( RF_FREQUENCY );
    Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                                   LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                                   LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                                   true, 0, 0, LORA_IQ_INVERSION_ON, 3000 ); 
   }



void loop()
{
	if(lora_idle == true)
	{
    delay(1000);
		txNumber += 0.01;

    String firebase_path = "candados/" + String(candado_nodo) + "/token";
    
    if (Firebase.getString(firebase_data, firebase_path.c_str())){
        if (firebase_data.dataType() == "string"){
            String token = firebase_data.stringData();
            Serial.println("Firebase Token: " + token);

            sprintf(txpacket, "%s", token.c_str());
            Serial.printf("\r\nSending packet \"%s\"", txpacket);
            Serial.println("...");

            Radio.Send((uint8_t *)txpacket, strlen(txpacket));
            Heltec.display->clear();
            Heltec.display->drawString(0, 0, "Token enviado:");
            Heltec.display->drawString(0, 30, token);
            Heltec.display->display();
            lora_idle = false;
        }
    }
    else{
        Serial.println(firebase_data.errorReason());
    }
	}
  Radio.IrqProcess( );
}

void OnTxDone( void )
{
	Serial.println("TX done......");
	lora_idle = true;
}

void OnTxTimeout( void )
{
    Radio.Sleep( );
    Serial.println("TX Timeout......");
    lora_idle = true;
}