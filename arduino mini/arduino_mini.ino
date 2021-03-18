#include <Bounce2.h> //eliminujemy drgania stykow
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include "MiniStructs.h"

class OurRadio {
private:
    RF24 radio = RF24(7, 8);
    RF24Network network = RF24Network(radio);
public:
    unsigned long numberOfReceivedReq = 0;          //licznik wykorzystywany w statystykach
    unsigned long numberOfSentResp = 0;             //licznik wykorzystywany w statystykach 
    
    //inicjalizacja radia
    void setup(int channel, uint16_t nodeId) 
    {
      SPI.begin();
      radio.begin();
      network.begin(channel, nodeId);
    }

    bool receiveFromUno(our_payload& payload) 
    {
      network.update();
      while (network.available()) 
      {
        RF24NetworkHeader header;
        network.read(header, &payload, sizeof(payload));
        return true;
      }
      return false;
    }

    bool sendToUno(unsigned long value, unsigned short typeOfMessage, int nodeId) 
    {
      our_payload payload = { value, typeOfMessage };
      RF24NetworkHeader header(nodeId);
      return network.write(header, &payload, sizeof(payload));
    }
};

const uint16_t miniNode = 00;                   //adres Pro Mini
const uint16_t unoNode = 01;                    //adres Uno

OurRadio radio;                                 //radio

int buttonPin = 2;                              //pin przycisku
bool observeFlag = false;                       //flaga do sprawdzania czy przycisk jest obserwowany
unsigned long timestampButton = 0;              //wartość przycisku
unsigned short buttonState = 1;                 //debounce
Bounce debouncer = Bounce();

int lightPin = 3;                               //pin lampki
double lightScale = 1000.0;
unsigned short lightValue = 255 * lightScale / 255.0;     //zmienna określająca wartość lampki
//statystyki
unsigned long numberOfReceivedReq = 0;
bool ifTest = false;

void processRadioMassage() 
{
  our_payload payloadR;

  //sprawdzamy czy coś odebrał
  if (radio.receiveFromUno(payloadR))
  {
    switch (payloadR.type)
    {
    case GetLightReqR:
        radio.sendToUno(lightValue, GetLightRespR, unoNode);  //wyślij wartość lampki            
        break;
    case PutLightReqR:
        analogWrite(lightPin, 255 - (255.0 / lightScale) * payloadR.value);   //ustawienie wartość lampki 
        lightValue = payloadR.value;  //uaktualnienie wartości zmiennej lightValue
        radio.sendToUno(lightValue, PutLightRespR, unoNode);  //wysłanie ustawionej wartości
        break;
    case GetButtonReqR:
        radio.sendToUno(buttonState, GetButtonRespR, unoNode);  //wyślij wartość przycisku
        break;
    case GetButtonObserveReqR:
        observeFlag = payloadR.value;  //ustawienie flagi obserwowania przycisku
        if (payloadR.value != 0)
        {
          radio.sendToUno(timestampButton, GetButtonObserveRespR, unoNode);
        }
        break;
    case GetRadioStatsReqR:   //wysyłanie statystyk
            
                numberOfReceivedReq++;          
            break;
    case IfTestReqR:
        {
            if (payloadR.value == 1) {
                
                numberOfReceivedReq = 0;
                ifTest = true;
            }
            else 
            { 
                ifTest = false;
                radio.sendToUno(numberOfReceivedReq, IfTestRespR, unoNode);
                Serial.println("Send stats");
            }
            break;
          }
    default:
        //do nothing
        break;
    }
  }
}

void setup(void)
{
    Serial.begin(9600);
    radio.setup(50, miniNode);
    pinMode(buttonPin, INPUT);
    pinMode(lightPin, OUTPUT);
    analogWrite(lightPin, 0);         //na starcie lampka świeci
    digitalWrite(buttonPin, HIGH);
    debouncer.attach(buttonPin);
    debouncer.interval(50);           //czas w ms, pozwala eliminowac drgania stykow
}

void loop(void) 
{
    processRadioMassage();   //odebranie wiadomości od Uno
    boolean changed = debouncer.update();
    if (changed)
    {
        int value = debouncer.read();
        if (value == HIGH)
        {
          buttonState = 1;
        }
        else 
        {
          buttonState = 0;
        }
        timestampButton = millis();
        if (observeFlag)
        {
            radio.sendToUno(timestampButton, GetButtonObserveRespR, unoNode);
        }
        else
        {
           
        }
    }
    else
    {
        //nth
    }
}
