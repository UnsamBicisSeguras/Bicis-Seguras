#include <heltec.h>
#include "LoRaWan_APP.h"
#include <Wire.h>
#include <Keypad.h>

#define RF_FREQUENCY                                915000000 // Hz
#define TX_OUTPUT_POWER                             14        // dBm
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

const int ledPin = 45; // Pin al que está conectado el LED
String codigorecibido; 

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

char txpacket[6];
char rxpacket[6];

static RadioEvents_t RadioEvents;

int16_t txNumber;

int16_t rssi,rxSize;

bool lora_idle = true;

void setup() {
    Heltec.begin(true, false, true);
    Heltec.display->setFont(ArialMT_Plain_16);
    pinMode(ledPin, OUTPUT); // Configura el pin del LED como salida
    Serial.begin(115200); // Inicia la comunicación serial
    Serial.println("Iniciado");
    Heltec.display->clear();
    Heltec.display->drawString(0, 0, "Iniciado");
    Heltec.display->display();
    Mcu.begin();
    
    txNumber=0;
    rssi=0;
  
    RadioEvents.RxDone = OnRxDone;
    Radio.Init( &RadioEvents );
    Radio.SetChannel( RF_FREQUENCY );
    Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                               LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                               LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                               0, true, 0, 0, LORA_IQ_INVERSION_ON, true );
}

void loop()
{
  if(lora_idle)
  {
    lora_idle = false;
    Serial.println("into RX mode");
    Radio.Rx(0);
  }
  Radio.IrqProcess( );

  static String codigoIngresado = ""; // Almacena el código ingresado por el teclado
  char key = keypad.getKey();

  if (key) {
    // Agrega el dígito al código ingresado
    codigoIngresado += key;

    Heltec.display->clear();
    Heltec.display->drawString(0, 0, "Código ingresado:");
    Heltec.display->drawString(0, 30, codigoIngresado);
    Serial.println("codigo ingresado:");
    Serial.println(codigoIngresado);
    Heltec.display->display();
    Serial.println("codigo recibido:");
    Serial.println(codigorecibido);
    // Si se ingresaron 6 dígitos, realiza la comparación automáticamente
    if (codigoIngresado.length() == 6) {
      if (codigoIngresado == codigorecibido.substring(0, 6)) {
        Heltec.display->clear();
        Heltec.display->drawString(0, 0, "Código correcto");
        Heltec.display->display();
        digitalWrite(ledPin, HIGH); // Enciende el LED
        delay(5000); // Espera 5 segundos
        digitalWrite(ledPin, LOW); // Apaga el LED
        Heltec.display->clear();
      } else {
        Heltec.display->clear();
        Heltec.display->drawString(0, 0, "Código incorrecto");
        Heltec.display->display();
        delay(2000); // Espera dos segundos antes de borrar el mensaje en la pantalla
        Heltec.display->clear();
      }
      // Reinicia el código ingresado después de la comparación
      codigoIngresado = "";
      Heltec.display->drawString(0, 0, "Ingrese un código");
      Heltec.display->display();
    }
  }

}

void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr )
{
    rssi=rssi;
    rxSize=size;
    memcpy(rxpacket, payload, size );
    Radio.Sleep( );
    Serial.printf("\r\nreceived packet \"%s\" with rssi %d , length %d\r\n",rxpacket,rssi,rxSize);
    codigorecibido = rxpacket; // Código predefinido de 6 dígitos
    lora_idle = true;
}