/*
 * PFIT: "UNSAM Bicis Seguras"
 * Acher Moira - Ibero Lucio
 * Código del "Candado electrónico" (C.E.)
 *
 */

//Librerias utilizadas
#include <heltec.h>
#include "LoRaWan_APP.h"
#include <Wire.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h> 

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


//Definición de la pantalla LCD (dirección de memoria 0x27. 2 filas, 16 columnas)
int SDApin = GPIO_NUM_33;  			//Serial Data
int SCLpin = GPIO_NUM_34;  			//Serial Clock 
LiquidCrystal_I2C lcd(0x27, 16, 2); // Inicializa el objeto lcd


//Definición del teclado matricial (4x4)
const int ROWS = 4; // Número de filas
const int COLS = 4; // Número de columnas

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {GPIO_NUM_7, GPIO_NUM_6, GPIO_NUM_5, GPIO_NUM_4}; // Definir los pines de las filas
byte colPins[COLS] = {GPIO_NUM_3, GPIO_NUM_2, GPIO_NUM_1, GPIO_NUM_38};  // Definir los pines de las columnas
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);


//Definición del servomotor
Servo myservo; 
const int SERVO_Pin = GPIO_NUM_42;


//Definición del sistema de alarma
const int BUZZER_Pin = GPIO_NUM_35;  
const int TRIG_Pin = GPIO_NUM_47; 	// Emite la onda ultrasónica
const int ECHO_Pin = GPIO_NUM_48; 	// Recibe la onda reflejada
long duration; 						// Tiempo transcurrido desde la transmisión hasta la recepción de la onda
float distanceCm; 					// Valor mesurado por el sensor en cada iteración
float distanciaInicial=8.50;		// Se consideró el valor más bajo mesurado por el sensor en estado vacío 


//Variables utilizadas para comparación/lógica del código
String codigorecibido; 		//Almacena la lectura del token de Firebae
String alarma = "Alerta";   //String a comparar para ver si se debe activar la alarma
String bloqueo = "Block";   //String a comparar para ver si se debe activar el bloqueo
String liberar = "Liberada";   //String a enviar para liberar bici
bool CandadoEnUso = false;	//Permite saber si el candado está siendo ocupado por un usuario
bool AlarmaSeteada = false;	// Permite que se envie solamente una vez la señal de alarma
bool lora_idle = true;		//Permite saber si el módulo LoRa está listo para Tx
String tokenValidado= "";  // Almaceno el token validado para el uso del candado (para liberar la bici ver si se ingresa el mismo)


//Inicialización del "Candado electrónico"
void setup() {
	// Inicialización de LoRa
    Mcu.begin();
	RadioEvents.RxDone = OnRxDone;
    RadioEvents.TxDone = OnTxDone;
    Radio.Init( &RadioEvents );
    Radio.SetChannel( RF_FREQUENCY );
    Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                        LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                        LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                        0, true, 0, 0, LORA_IQ_INVERSION_ON, true );

    Radio.SetChannel(RF_FREQUENCY);
    Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                        LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                        LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                        true, 0, 0, LORA_IQ_INVERSION_ON, 3000 ); 
								   
	//Configuración de pines
	pinMode(BUZZER_Pin, OUTPUT);
	pinMode(TRIG_Pin, OUTPUT); 
	pinMode(ECHO_Pin, INPUT); 
	
	// Inicialización del LCD
    Wire.begin(SDApin, SCLpin);		// Comunicación I2C con los pines especificados
    lcd.init();  					
    lcd.backlight();   				// Enciende la luz de fondo.
    lcd.clear();
    lcd.setCursor(5, 0);  
    lcd.print("UNSAM"); 
    lcd.setCursor(1, 1);  
    lcd.print("Bicis  Seguras"); 		
    lcd.display();	
	
	// Inicialización del servo               
	myservo.attach(SERVO_Pin);  
	
	// Inicialización de la comunicación serial
	Serial.begin(9600); 			 
	Serial.println("Iniciado");
}

void loop(){
	if (CandadoEnUso && AlarmaSeteada==false){
		verificarAlarma();
	}
	
	if(lora_idle){				// Se pone el módulo en modo recepción
		lora_idle = false;
		Radio.Rx(0);
	}
	Radio.IrqProcess( );

	// Lectura del codigo ingresado por teclado y comparación con el token de firebase
	static String codigoIngresado = "";  //Almacena el código ingresado via teclado
	char key = keypad.getKey();
	if (key) {
		codigoIngresado += key;
		lcd.backlight();   				// Enciende la luz de fondo.
		lcd.clear();
		lcd.setCursor(0, 0);  
		lcd.print("Codigo ingresado");
		lcd.setCursor(5,1);
		lcd.print(codigoIngresado); 	
		lcd.display();	
		// Si se ingresaron 6 dígitos, se compara con el token de firebase
		if (codigoIngresado.length() == 6 && CandadoEnUso==false) {
			Serial.println("\nCodigo ingresado: " + codigoIngresado);
			if (codigoIngresado == codigorecibido.substring(0, 6)) {	//Coincidencia
				enviarBloqueo(); 				//Enviamos al transceptor la orden para bloquear el candado en DB
				tokenValidado=codigoIngresado;  // Almaceno el token validado para el uso del candado (para liberar la bici ver si se recibe el mismo)
				lcd.clear();
				lcd.setCursor(4, 0);  
				lcd.print("Correcto");
				lcd.display();
				CandadoEnUso=true;
				myservo.write(0);   			// Se mueve el servo a la posición abierta (0 grados)
				delay(1800); 					
				myservo.release();				// Espera seis segundos para colocar la bicicleta
				delay(6000);   
				myservo.write(430); 			// Se mueve el servo a la posición cerrada (360 grados)
				delay(1720);
				myservo.release();				// Espera seis segundos para colocar la bicicleta				
				lcd.clear();
		        lcd.noBacklight();        // Apagamos la pantalla LCD hasta q se ingrese nuevo codigo.
			} else {
				lcd.clear();
				lcd.setCursor(3, 0);  
				lcd.print("Incorrecto");
				lcd.display();	
				delay(2000); 					
				lcd.clear();
				lcd.setCursor(3, 0);  
				lcd.print("Ingrese un ");
				lcd.setCursor(5,1);
				lcd.print("codigo");
				lcd.display();
			}
		codigoIngresado = ""; // Se reinicia el código ingresado por teclado para poder comparar otra vez
		}
		if (codigoIngresado.length() == 6 && CandadoEnUso==true) {
			Serial.println("\nCodigo ingresado: " + codigoIngresado);
			if (codigoIngresado == tokenValidado) {	//Coincidencia, se debe liberar la bicicleta (el usuario se quiere ir)
				liberarBici(); 				//Enviamos al transceptor la orden de que se debe liberar la bici en la DB
			} else {
				lcd.clear();
				lcd.setCursor(3, 0);  
				lcd.print("Incorrecto");
				lcd.display();	
				delay(2000); 					
				lcd.clear();
				lcd.setCursor(3, 0);  
				lcd.print("Ingrese un ");
				lcd.setCursor(5,1);
				lcd.print("codigo");
				lcd.display();
			}
		codigoIngresado = ""; // Se reinicia el código ingresado por teclado para poder comparar otra vez
		}
	}
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr){
    memset(rxpacket, 0, BUFFER_SIZE);   // Limpiamos el buffer antes de copiar el nuevo paquete
    memcpy(rxpacket, payload, size );
    Serial.println("\nRecibí: ");
	  Serial.println(rxpacket);
    codigorecibido = rxpacket; 			// Almacenamos el token para su comparación con el ingresado por teclado
    lora_idle = true;
}

void liberarBici(){
	CandadoEnUso=false;
	AlarmaSeteada=false;
	sprintf(txpacket, "%s", liberar.c_str());
	Radio.Send((uint8_t *)txpacket, strlen(txpacket)); // Se envia el mensaje "Liberada" para que se cambie la situacion en la DB
	myservo.write(0);   			// Se mueve el servo a la posición abierta (0 grados)
	delay(1720); 					
	myservo.release();				// Espera seis segundos para quitar la bicicleta
	delay(6000);   
	myservo.write(360); 			// Se mueve el servo a la posición cerrada (360 grados)
	delay(1720);
	myservo.release();				
	Serial.printf("\nBici liberada. Candado listo para usar nuevamente");
	lcd.clear();
	lcd.setCursor(3, 0);  
	lcd.print("Ingrese un ");
	lcd.setCursor(5,1);
	lcd.print("codigo");
	lcd.display();
}

void OnTxDone(void){
	lora_idle = true; 
	Radio.Rx(0);
}

// Si el token recibido es igual al ingresado por teclado, se envia el texto "Block" para que se haga efectiva la ocupación del candado.
void enviarBloqueo(){
    sprintf(txpacket, "%s", bloqueo.c_str());
    Radio.Send((uint8_t *)txpacket, strlen(txpacket));
    Serial.printf("\nEnviando mensaje: %s", txpacket);     
}

// Si la distancia es menor a la típica(estan obstruyendo/vulnerando el pestillo), se activa la alarma.
void verificarAlarma(){
	int distancia=sensarDistancia(); 			// Valor actual mesurado por el sensor
	if (distancia >= distanciaInicial-1) { 		// Verificar si la distancia mesurada es mayor que la distancia que habia sin bici + tolerancia (la robaron)
		enviarAlarma();						
		Serial.println("\nAlarma enviada a APP"); 
		// Activamos la alarma sonora
		digitalWrite(BUZZER_Pin, HIGH);
		delay (2000);
		digitalWrite(BUZZER_Pin, LOW);
	}
}

//Funcion para verificar si están intentando vulnerar el candado
int sensarDistancia(){
	digitalWrite(TRIG_Pin, LOW); 			//Se envia un pulso LOW para distinguir del HIGH próximo	
	delayMicroseconds(2); //
	digitalWrite(TRIG_Pin, HIGH);			//Se envia un pulso HIGH de 10 uS 
	delayMicroseconds(10);
	digitalWrite(TRIG_Pin, LOW);
	duration = pulseIn(ECHO_Pin, HIGH);   	// Guardamos la lectura del pin de Echo (Tiempo de viaje de la onda)
	distanceCm = duration * 0.034/2;  		// Calculamos la distancia (velocidad del sonido en cm/us)
	return distanceCm; 
}

// Se envia el texto "Alerta" para que se active la notificación push en la APP.
void enviarAlarma(){
    sprintf(txpacket, "%s", alarma.c_str());
    Radio.Send((uint8_t *)txpacket, strlen(txpacket));
    Serial.printf("\nEnviando mensaje: %s", txpacket); 	
	AlarmaSeteada=true;	
}