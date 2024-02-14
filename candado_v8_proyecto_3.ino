#include <heltec.h>
#include "LoRaWan_APP.h"
#include <Wire.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h> 

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

//Configuración de la pantalla LCD (dirección  0x27 y 16 columnas x 2 filas)
int SDApin = GPIO_NUM_33;  			//Serial Data
int SCLpin = GPIO_NUM_34;  			//Serial Clock 
LiquidCrystal_I2C lcd(0x27, 16, 2); // Inicializa el objeto LCD

const int ROWS = 4; // Número de filas
const int COLS = 4; // Número de columnas

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {GPIO_NUM_38, GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_3}; // Definir los pines de las filas
byte colPins[COLS] = {GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7};  // Definir los pines de las columnas

// DESCOMENTAR Servo myservo;  // Crea un objeto de tipo Servo

const int ledPin = 45; // Pin al que está conectado el LED

//Alarma usando sensor de distancia
// DESCOMENTAR #define BUZZER_PIN  35  // Pin digital donde está conectado el buzzer activo
// DESCOMENTAR const int trigPin = 47; //Poner pin. Emite la onda ultrasónica
// DESCOMENTAR const int echoPin = 48; //Poner pin. Recibe la onda reflejada
// DESCOMENTAR long duration; // Guarda el tiempo de viaje de las ondas ultrasónicas (tiempo transcurrido desde la transmisión y recepción de la onda de pulso)
// DESCOMENTAR float distanceCm;

bool CandadoEnUso = false;

String codigorecibido; 
String tokenvalidado; 
String alarma = "Alerta";  //String a comparar para ver si se debe activar la alarma
String bloqueo = "Block";  //String a comparar para ver si se debe activar el bloqueo

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];

static RadioEvents_t RadioEvents;

int16_t rssi,rxSize;

bool lora_idle = true;

void setup() {
    pinMode(ledPin, OUTPUT); // Configura el pin del LED como salida
	  // DESCOMENTAR pinMode(trigPin, OUTPUT); // Configura el pin del trigger del sensor como salida
	  // DESCOMENTAR pinMode(echoPin, INPUT); // Configura el pin de echo del sensor como entrada
    // DESCOMENTAR pinMode(BUZZER_PIN, OUTPUT);        // Configura el pin del buzzer como salida
    Serial.begin(115200); // Inicia la comunicación serial
    Wire.begin(SDApin, SCLpin);		// Inicializa la comunicación I2C con los pines especificados
    lcd.init();  					// Inicializar el LCD
    lcd.backlight();   		//Encender la luz de fondo.
    lcd.clear();
    lcd.setCursor(5, 0);  // Establece el cursor en la primera línea
    lcd.print("UNSAM"); 
    lcd.setCursor(1, 1);  // Establece el cursor en la primera línea
    lcd.print("Bicis  Seguras"); 		
    lcd.display();	
    Mcu.begin();
    
    rssi=0;
  
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
                                
  //myservo.attach(42);  // El pin de control del servo está conectado al pin 42
  Serial.println("Iniciado");
}

void loop()
{
  if (CandadoEnUso==true){
    // DESCOMENTAR verificarAlarma();
  }
  else{
    lcd.backlight();   		//Enciende la luz de fondo para ingresar codigos
  }

  if(lora_idle)
  {
    lora_idle = false;
    Radio.Rx(0);
  }
  Radio.IrqProcess( );

  static String codigoIngresado = ""; // Almacena el código ingresado por el teclado
  char key = keypad.getKey();

  if (key) {
    // Agrega el dígito al código ingresado
    codigoIngresado += key;
    lcd.clear();
    lcd.setCursor(0, 0);  // Establece el cursor en la primera línea
    lcd.print("Codigo ingresado");
    lcd.setCursor(5,1);
    lcd.print(codigoIngresado); 	
    lcd.display();	
    // Si se ingresaron 6 dígitos, realiza la comparación automáticamente
    if (codigoIngresado.length() == 6) {
    Serial.println("\nCodigo ingresado:");
    Serial.println(codigoIngresado);
      if (codigoIngresado == codigorecibido.substring(0, 6)) {
        tokenvalidado=codigoIngresado; //Guardamos el token valido para que el mismo pueda usarse para desbloquear la bici.
        enviarBloqueo(); //Envia al transceptor la orden para bloquear el candado en DB
        lcd.clear();
        lcd.setCursor(4, 0);  // Establece el cursor en la primera línea
        lcd.print("Correcto");
        lcd.display();	
        digitalWrite(ledPin, HIGH); // Enciende el LED
        //myservo.write(0);   // Abro el candado. Mueve el servo a la posición abierto (0 grados)
        delay(2000); // Espera 2 segundos
        //myservo.write(180); // Cierro el candado. Mueve el servo a la posición cerrado (180 grados)
        digitalWrite(ledPin, LOW); // Apaga el LED
        lcd.clear();
        lcd.noBacklight();   		//Apagar la luz de fondo.
        CandadoEnUso=true;
      } else {
        lcd.clear();
        lcd.setCursor(3, 0);  // Establece el cursor en la primera línea
        lcd.print("Incorrecto");
        lcd.display();	
        delay(2000); // Espera dos segundos antes de borrar el mensaje en la pantalla
        lcd.clear();
        lcd.setCursor(3, 0);  // Establece el cursor en la primera línea
        lcd.print("Ingrese un ");
        lcd.setCursor(5,1);
        lcd.print("codigo");
        lcd.display();
      }
      // Reinicia el código ingresado después de la comparación
      codigoIngresado = "";
    }
  }
}

void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr )
{
    rssi=rssi;
    rxSize=size;
    memset(rxpacket, 0, BUFFER_SIZE);   // Limpiar el buffer antes de copiar el nuevo paquete
    memcpy(rxpacket, payload, size );
    //Radio.Sleep( );
    Serial.println("\nEntre en modo recepción... y recibi: ");
    Serial.println(rxpacket);
    // Verifica si el paquete contiene el texto "Liberada"
    if (strstr(rxpacket, "Liberada") != NULL) {
      // Enciende el LED en respuesta al mensaje "Liberada"
      liberarBici();
      lcd.setCursor(3, 0);  // Establece el cursor en la primera línea
      lcd.print("Ingrese un ");
      lcd.setCursor(5,1);
      lcd.print("codigo");
      lcd.display();
    }
    codigorecibido = rxpacket; // Código predefinido de 6 dígitos
    lora_idle = true;
}

void liberarBici() {
    Serial.printf("\nBici liberada");
    lora_idle = true;
    CandadoEnUso=false;
    //myservo.write(0);   // Abro el candado
    digitalWrite(ledPin, HIGH); // Enciende el LED
    delay(3000); // Espera 5 segundos
    digitalWrite(ledPin, LOW); // Apaga el LED
    //myservo.write(180); // Cierro el candado

}

void OnTxDone( void )
{
	//Serial.println("\nBloqueo enviado correctamente...\n");
	lora_idle = true; 
  Radio.Rx(0);
}

void enviarBloqueo() {
	// Configura LoRa para poner el candado en locked=true
    sprintf(txpacket, "%s", bloqueo.c_str());
    Radio.Send((uint8_t *)txpacket, strlen(txpacket));
    Serial.printf("\nEnviando mensaje: %s", txpacket);     
}

// DESCOMENTAR void verificarAlarma (){
	// DESCOMENTAR int distancia=sensarDistancia(); //Almaceno en una variable
	// DESCOMENTAR if (distancia < 10) { // Verificar si la distancia es menor a 10 cm
	  // DESCOMENTAR enviarAlarma();
	  // DESCOMENTAR Serial.println("\nAlarma enviada a APP"); // Imprimir mensaje en el monitor serial
    // Activar el buzzer
    // DESCOMENTAR digitalWrite(BUZZER_PIN, HIGH);
	  // DESCOMENTAR delay (2000);
	  // DESCOMENTAR digitalWrite(BUZZER_PIN, LOW);
	// DESCOMENTAR }
// DESCOMENTAR }

// DESCOMENTAR void enviarAlarma() {
	// Configura LoRa para enviar la alarma de que se esta vulnerando el candado
    // DESCOMENTAR sprintf(txpacket, "%s", alarma.c_str());
    // DESCOMENTAR Radio.Send((uint8_t *)txpacket, strlen(txpacket));
    // DESCOMENTAR Serial.printf("\nEnviando mensaje: %s", txpacket);
// DESCOMENTAR }



// DESCOMENTAR int sensarDistancia(){
  //Se envia un pulso LOW para distinguir del HIGH próximo	
  // DESCOMENTAR digitalWrite(trigPin, LOW); 
  // DESCOMENTAR delayMicroseconds(2); //
  //Estas lineas hacen un pulso de 10 uS (emite ultrasonido)
  // DESCOMENTAR digitalWrite(trigPin, HIGH);
  // DESCOMENTAR delayMicroseconds(10);
  // DESCOMENTAR digitalWrite(trigPin, LOW);
  
  // DESCOMENTAR duration = pulseIn(echoPin, HIGH);   // Lee el pin de Echo. Devuelve el tiempo de viaje de la onda sonora
  // DESCOMENTAR distanceCm = duration * 0.034/2;  // Calculamos la distancia (usando la vel del sonido en cm/us)
  //Serial.print("Distancia (cm): ");
  //Serial.println(distanceCm);
  //delay(1000);
  // DESCOMENTAR return distanceCm; 
// DESCOMENTAR }