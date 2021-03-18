#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <RF24.h>
#include <RF24Network.h>
#include "UnoStructs.h"

byte addressMac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0xF5 };  //adresu mac Uno
unsigned int localPort = 5240;        //numer portu 
const uint16_t miniNode = 00;         //adres Pro Mini
const uint16_t unoNode = 01;          //adres Uno

//zarzadzanie CoAPem
class OurCoap {
private:
    EthernetUDP udp;                    //ethernet
    CoapMessage msg;                    //obiekt wiadomosci
    CoapMessageOptions cmOpt;           //struktura zawierajaca opcje
    byte observeToken[8];               //token obserwowanego obiektu
    bool observeFlag = false;           //flaga, która sprawdza czy obserwujemy przycisk
    int observeTokenLength;             //dlugosc tokenu obserwowanego obiektu obserwowalnego

    //parsowanie wiadomosc CoAP i zwracanie jej rodzaju, XOR ma niższy priorytet niż przesunięcie bitowe
    CoapReqType getCoapReqType(int pSize, byte pBuffer[])
    {
        for (int i = 0; i < 255; ++i)
        {
            cmOpt.uriPath[i] = 0; //czyszczenie uriPath
        }
        for (int i = 0; i < 5; ++i)
        {
            msg.payload[i] = 0;
        }
        //operacje bitowe na argumencie funkcji, zczytujemy wartości, zapisujemy elementy do struktury
        CoapReqType reqType = UNKNOWN;                        //początkowa domyślna wartość rodzaju zadania
        byte versionBits = pBuffer[0] >> 6;                   //wyłuskanie wersji CoAP
        msg.coapVersion = versionBits;                        //zapisanie wersji CoAP w strukturze
        byte typeBits = pBuffer[0] >> 4 ^ versionBits << 2;   //wyłuskanie pola typ
        msg.type = typeBits;                                  //zapisanie pola typ w strukturze
        byte tokenLengthBits = pBuffer[0] ^ (versionBits << 6 | typeBits << 4); //wyłuskanie dlugosci tokena
        msg.tokenLength = tokenLengthBits;                    //zapisanie długosci tokena w strukturze
        byte classBits = pBuffer[1] >> 5;                     //wyłuskanie klasy wiadomości pola kod
        msg.c = classBits;                                    //zapisanie klasy wiadomości pola kod w strukturze 
        msg.dd = pBuffer[1] ^ (classBits << 5);               //wyłuskanie i zapisanie szczegółów pola kod
        msg.mid[0] = pBuffer[2];                              //wyłuskanie i zapisanie pierwszego bajtu Message ID
        msg.mid[1] = pBuffer[3];                              //wyłuskanie i zapisanie drugiego bajtu Message ID

        for (int i = 0; i < (int)tokenLengthBits; i++)
        {
            msg.token[i] = pBuffer[i + 4];                      //zapisanie tokena w strukturze
        }
        unsigned int actualOption = 0;                      //aktualna wartosc opcji
        unsigned int optionBytesLength = 0;                 //przechowuje długosci obecnej opcji
        unsigned int optIndex = (int)tokenLengthBits + 4;   //indeks opcji - 4 bajty już wczesniej sparsowane

         //wyłuskanie opcji
        while (pBuffer[optIndex])
        {
            int start = 0;                                   //zmienna uzywana do dostepu do indeksu aktualnej opcji
            byte optionBytes = pBuffer[optIndex] >> 4;       //wyłuskanie opcji delta

            actualOption += (unsigned int)optionBytes;       //zwiekszenie obecnej opcji o opcje delta
            if ((unsigned int)optionBytes == 0)
            {
                start = optionBytesLength;                     //ustawienie indeksu w taki sposób aby w wypadku opcji delta = 0 była ona kontynuowana
            }

            optionBytesLength = (unsigned int)(pBuffer[optIndex++] ^ (optionBytes << 4)); //wyłuskanie długosci obecnej opcji

            //switch po opcjach w zależności od option ID, w case'ach interujemy po kolejnych bajtach opcji... 
            //... i zapisujemy do struktury opt
            switch (actualOption) {
            case 6:             //observe
                cmOpt.observe = true;                   //otrzymywany observe zawsze jest równy 0
                observeTokenLength = pBuffer[0] ^ (versionBits << 6 | typeBits << 4); //wyłuskanie tokena
                for (int i = 0; i < observeTokenLength; i++)
                {
                    observeToken[i] = pBuffer[i + 4];   //zapamiętywanie tokenu
                }
                break;
            case 11:            //uri-path
                for (int k = 0; k < optionBytesLength; k++)
                {
                    cmOpt.uriPath[k + start] = (char)pBuffer[k + optIndex]; //zapis opcji w strukturze
                }
                break;
            case 12:            //content-format
                for (int k = 0; k < optionBytesLength; k++)
                {
                    cmOpt.contentFormat[k + start] = pBuffer[k + optIndex]; //zapis opcji w strukturze
                }
                break;
            case 17:            //accept
                for (int k = 0; k < optionBytesLength; k++)
                {
                    cmOpt.acceptOption[k + start] = pBuffer[k + optIndex]; //zapis opcji w strukturze
                }
                break;
            case 23:            //block2
                for (int k = 0; k < optionBytesLength; k++)
                {
                    cmOpt.block2[k + start] = pBuffer[k + optIndex]; //zapis opcji w strukturze
                }
                break;
            case 28:            //size2
                for (int k = 0; k < optionBytesLength; k++)
                {
                    cmOpt.size2[k + start] = pBuffer[k + optIndex]; //zapis opcji w strukturze
                }
                break;
            }
            optIndex += optionBytesLength; //ustawienie optIndex na indeks nastepnej opcji
        }

        int i = 0;
        //wyłuskiwanie payloadu
        while (i < pSize)
        {
            //początkowy bajt payloadu to ff (255)
            if (pBuffer[i] == 255)
            {
                int j = 0;
                while (pBuffer[i])
                {
                    i++;
                    msg.payload[j] = pBuffer[i]; //zapisanie bajtu payloadu w strukturze
                    j++;
                    //jeśli payload ma mniej niż 5 cyfr lub nie jest liczbą
                    if ((msg.payload[j] != '\0') and (msg.payload[j] > '9' or msg.payload[j] < '0' or j > 5))
                    {
                        return CLIENT_ERROR; //nieprawidłowy payload
                    }
                }
            }
            i++;
        }

        //GET, czyli szczegół pola kod = 1, a w środku porównanie napisów...
        //... i ustawienie odpowiedniego typu zapytania
        if (msg.dd == 1)
        {
            if (!strcmp(cmOpt.uriPath, ".well-knowncore"))
            {
                reqType = DISCOVER;
            }
            else if (!strcmp(cmOpt.uriPath, "light"))
            {
                reqType = LIGHT_GET;
            }
            //dodatkowo sprawdzenie czy obserwowanie musi byc przed samym button
            else if (cmOpt.observe && !strcmp(cmOpt.uriPath, "button"))
            {
                reqType = BUTTON_GET_OBSERVE;
            }
            else if (!strcmp(cmOpt.uriPath, "button"))
            {
                reqType = BUTTON_GET;
            }
            else if (!strcmp(cmOpt.uriPath, "stats"))
            {
                reqType = OUR_STATS;
            }
            else
            {
                reqType = NOT_FOUND;
            }
        }
        //PUT, czyli szczegół pola kod = 3
        else if (msg.dd == 3 && !strcmp(cmOpt.uriPath, "light") && !cmOpt.observe)
        {
            reqType = LIGHT_PUT;
        }
        //wiadomość typu RST
        else if (msg.type == 3)
        {
            reqType = RST;
            return reqType;
        }
        //pozwalamy tylko na zdefiniowane uripath
        else if (!strcmp(cmOpt.uriPath, ".well-knowncore") || !strcmp(cmOpt.uriPath, "light") ||
            !strcmp(cmOpt.uriPath, "button") || !strcmp(cmOpt.uriPath, "stats"))
        {
            reqType = METHOD_NOT_ALLOWED;     //obsługa niedozowlonych zapytań
        }
        else
        {
            reqType = NOT_FOUND;              //obsługa innych przypadków (nie znaleziono)
        }
        return reqType;                       //zwrot typu zapytania
    }

public:
    //otrzymanie zapytania
    CoapReqType receiveRequest()
    {
        CoapReqType type;
        int packetSize = udp.parsePacket();                     //wyłuskanie długości otrzymanego pakietu
        byte packetBuffer[UDP_TX_PACKET_MAX_SIZE] = { NULL };   //czyszczenie bufora
        udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);         //zapis wiadomości w buforze
        if(packetSize)
        {
          type = getCoapReqType(packetSize,packetBuffer);
        }else
        {
          type= UNKNOWN;
        }
        return type; //zwróć rodzaj wiadomosci
    }

    uint16_t tmpMid = 0;
    //wysłanie odpowiedzi do Copper
    void sendResponse(byte options[] = NULL, int optionsSize = 0, char payload[] = NULL, int payloadSize = 0, byte c = 0, byte dd = 0, bool obsFla = false)
    {
        byte rspMessage[256 + 12];                     //tablica wiadomości odpowiedzi
        rspMessage[0] = msg.coapVersion << 6 | 0b1 << 4 | msg.tokenLength;     //przypisanie wartości wersji, typu wiad. i długości tokena
        rspMessage[1] = c << 5 | dd;                   //przypisanie pola kod
        rspMessage[2] = tmpMid >> 8;                   //przypisanie pierwszego bajtu mid
        rspMessage[3] = tmpMid;                        //przypisanie drugiego bajtu mid
        tmpMid += 1;                                   //inkrementacja mid wiadomości

        //sprawdza czy jest observe
        if (!obsFla)
        {
            for (int i = 0; i < (int)msg.tokenLength; i++)
            {
                rspMessage[i + 4] = msg.token[i];
            }
        }
        else
        {
            for (int i = 0; i < observeTokenLength; i++)
            {
                rspMessage[i + 4] = observeToken[i];
            }
        }

        int index = (int)msg.tokenLength + 4;               //indeks na bajt po tokenie
        for (int i = 0; i < optionsSize; i++)
        {
            rspMessage[index++] = (byte)options[i];    //przypisanie opcji
        }
        rspMessage[index++] = 0b11111111;              //bajt rozpoczęcia payloadu
        for (int i = 0; i < payloadSize; i++)
        {
            rspMessage[index++] = (byte)payload[i];    //przypisanie payloadu
        }
        udp.beginPacket(udp.remoteIP(), udp.remotePort());  //ustalenie gdzie wysłać pakiet (adres i port)
        udp.write(rspMessage, index - 1);              //wysłanie pakietu
        udp.endPacket();                                    //koniec wysyłania
    }

    //konwersja char na int do puta
    int convertPayloadToInt()
    {
        int i;
        sscanf(msg.payload, "%d", &i);                  //konwerwsja tablicy char na int
        return i;                                       //zwraca int
    }

    //zainicjalizowanie połączenia internetowego
    void setup(byte addressMac[], unsigned int localPort)
    {
        Ethernet.begin(addressMac);            //przypisanie mac
        Serial.println("Adres IP przydzielony przez DHCP");
        Serial.println(Ethernet.localIP());    //wypisanie adresu ip
        udp.begin(localPort);                  //start połączenia
    }
};

//zarządzanie radiem
class OurRadio {
private:
    RF24 radio = RF24(7, 8);
    RF24Network network = RF24Network(radio);
public:
    //inicjalizacja radia
    void setup(int channel, uint16_t nodeId)
    {
        SPI.begin();
        radio.begin();
        network.begin(channel, nodeId);
    }

    //odbiór wiadomosci od ProMini    
    bool receiveFromMini(our_payload& payload)
    {
        network.update();
        while (network.available()) //sprawdzamy, czy jest dostepna wiadomosc w tym wezle
        {
            RF24NetworkHeader header;
            network.read(header, &payload, sizeof(payload));
            return true;
        }
        return false; //false, gdy juz nie ma wiadomosci
    }

    //wysyłanie wiadomosci do Promini (wartosc, typ, nr węzła Uno)
    bool sendToMini(unsigned long value, unsigned short messageType, int nodeId)
    {
        RF24NetworkHeader header(nodeId); //nagłówek
        our_payload payload = { value, messageType };
        return network.write(header, &payload, sizeof(payload)); //wiadomosc jest wysyłana do Mini
    }
};

//metoda zliczająca cyfry 
int getDigitsNumber(long number)
{
    int r = 0;
    if (!number) 
      return 1;
    while (number > 0)
    {
        ++r;
        number /= 10;
    }
    return r;
}

OurCoap coapServer;
OurRadio radio;

//inicjalizacja radia i połączenia internetowego
void setup()
{
    Serial.begin(115200);
    radio.setup(50, unoNode);
    coapServer.setup(addressMac, localPort);
}

//wysłanie wiadomości o kodzie 2.04 (CHANGED PUT and POST) z payloadem
void messagePut(long number)
{
    char buffer[getDigitsNumber(number) + 1];
    int ret = snprintf(buffer, sizeof(buffer) + 1, "%ld", number);
    byte passOptions[] = { 0b1100 << 4 | 0b1, 0b0,  //12
                           0b0101 << 4 | 0b1, (byte)sizeof(buffer) };  //17
    coapServer.sendResponse(passOptions, sizeof(passOptions), buffer, getDigitsNumber(number) + 1, 2, 4);
}

//wysłanie wiadomości o kodzie 2.05 (GET Content) na pobranie statystyk
void statsGet(long number)
{
    char buffer[getDigitsNumber(number) + 1];
    int ret = snprintf(buffer, sizeof(buffer) + 1, "%ld", number);
    byte passOptions[] = { 0b1100 << 4 | 0b1, 0b0,  //12
                           0b1011 << 4 | 0b1, 0b10, //23
    };
    coapServer.sendResponse(passOptions, sizeof(passOptions), buffer, (byte)(sizeof(buffer) + 1), 2, 5);
}

//wysłanie wiadomości o kodzie 2.05 (GET Content) z payloadem
void messageGet(long number)
{
    char buffer[getDigitsNumber(number) + 1];
    int ret = snprintf(buffer, sizeof(buffer) + 1, "%ld", number);
    byte passOptions[] = { 0b1100 << 4 | 0b1, 0b0,  //12
                           0b1011 << 4 | 0b1, 0b10, //23
                           0b0101 << 4 | 0b1, (byte)(getDigitsNumber(number) + 1)  //28
    };
    coapServer.sendResponse(passOptions, sizeof(passOptions), buffer, getDigitsNumber(number) + 1, 2, 5);
}

uint8_t counterObs = 1; //domyślna wartość observe w odpowiedzi

//wysłanie wiadomości o kodzie 2.05 (GET Content) z payloadem dla observe
void messageGetObserve(long number)
{
    char buffer[getDigitsNumber(number) + 1] = { NULL };
    int ret = snprintf(buffer, sizeof(buffer) + 1, "%ld", number);
    byte passOptions[] = { 0b0110 << 4 | 0b1, counterObs++,  //6
                           0b0110 << 4 | 0b1, 0b0,  //12
                           0b1011 << 4 | 0b1, 0b010,  //23
                           0b0101 << 4 | 0b1, (byte)(getDigitsNumber(number) + 1) };  //28
    coapServer.sendResponse(passOptions, sizeof(passOptions), buffer, getDigitsNumber(number) + 1, 2, 5);
}

//wysłanie wiadomości o kodzie 4.00 (Bad Request)
void messageBadClientRequest(char* ch, int chSize)
{
    byte by[1];
    coapServer.sendResponse(by, 0, ch, chSize + 1, 4, 0);
}

//wysłanie wiadomości o kodzie 4.00 (Bad Request)
void messageMethodNotAllowed()
{
    byte by[1];
    char ch[1];
    coapServer.sendResponse(by, 0, ch, 0, 4, 5);
}

//wysłanie wiadomości o kodzie 4.04 (Not found)
void messageNotFound()
{
    byte by[1];
    char ch[1];
    coapServer.sendResponse(by, 0, ch, 0, 4, 4);
}

unsigned long startTestTime = 0;
int nrOfMsg = 20; //liczba wiadomosci testowych

void loop() {
    int reqType = coapServer.receiveRequest();    //pobranie typu zapytania

    switch (reqType) {
    //zapytanie get light
    case LIGHT_GET:
    {
        Serial.println("Light Get");
        radio.sendToMini(0, GetLightReqR, miniNode);
        break;
    }
    //zapytanie put light
    case LIGHT_PUT:
    {
        Serial.println("Light Put");
        int value = coapServer.convertPayloadToInt();   //wyłuskanie wartości z payloadu
        Serial.println(value);
        //sprawdza czy poprawna wartość
        if (!(value >= 0 and value <= 1000))
        {
            Serial.println(value);
            char buffer[] = "Niepoprawna wartosc";
            messageBadClientRequest(buffer, sizeof(buffer) / sizeof(char));
        }
        else
        {
            radio.sendToMini(value, PutLightReqR, miniNode);
        }
        break;
    }
    //zapytanie get button
    case BUTTON_GET:
    {
        Serial.println("Button Get");
        radio.sendToMini(0, GetButtonReqR, miniNode);
        break;
    }
    //zapytanie get button observe
    case BUTTON_GET_OBSERVE:
    {
        Serial.println("Button Observe");
        radio.sendToMini(1, GetButtonObserveReqR, miniNode);
        break;
    }
    //zapytanie discover
    case DISCOVER:
    {
        Serial.println("Discover");
        char discPayloadFirst[] = "</button>;obs;rt=\"observe\",</light>;title=\"Light which can be s";
        char discPayloadSecond[] = "et to 0-1000\";</stats>;";
        Serial.println(discPayloadFirst);
        Serial.println(discPayloadSecond);
        byte discOptionsFirst[] = { 0b100 << 4 | 0b1, 0b1111111,  //4
                              0b1000 << 4 | 0b1, 0b101000, //12
                              0b1011 << 4 | 0b1, 0b1010, //23
                              0b0101 << 4 | 0b1, (byte)sizeof(discPayloadFirst) + (byte)sizeof(discPayloadSecond) }; //28
        byte discOptionsSecond[] = { 0b100 << 4 | 0b1, 0b1111111, //4
                              0b1000 << 4 | 0b1, 0b101000, //12
                              0b1011 << 4 | 0b1, 0b100010, //23
                              0b0101 << 4 | 0b1, (byte)sizeof(discPayloadSecond) + (byte)sizeof(discPayloadFirst) }; //28
        coapServer.sendResponse(discOptionsFirst, sizeof(discOptionsFirst), discPayloadFirst, (byte)sizeof(discPayloadFirst), 2, 5); //odsyła 64 bajty wiadomości
        while (coapServer.receiveRequest() != DISCOVER)
        {

        }
        coapServer.sendResponse(discOptionsSecond, sizeof(discOptionsSecond), discPayloadSecond, (byte)sizeof(discPayloadSecond), 2, 5); //odsyła pozostałą część wiadomości
        break;
    }
    //obsługa client error
    case CLIENT_ERROR:
    {
        char buffer[] = "Niepoprawna wartosc";
        messageBadClientRequest(buffer, sizeof(buffer) / sizeof(char));
        break;
    }
    //obsługa method not allowed
    case METHOD_NOT_ALLOWED:
    {
        messageMethodNotAllowed();
        break;
    }
    //obsługa statystyk
    case OUR_STATS:
    {
        startTestTime = millis();
        radio.sendToMini(1, IfTestReqR, miniNode);
        delay(100);
        
        while (nrOfMsg--)
        {
            radio.sendToMini(1, GetRadioStatsReqR, miniNode);  //wysylanie kolejnych testow         
            delay(100);
        }
        radio.sendToMini(0, IfTestReqR, miniNode);
        break;
    }
    //obsługa rst 
    case RST:
    {
        counterObs = 9;
        radio.sendToMini(0, GetButtonObserveReqR, miniNode);
        break;
    }
    //obsługa not found
    case NOT_FOUND:
    {
        messageNotFound();
        break;
    }
    default:
    {
        break;
    }
    }

    our_payload payload;
    //odbiór wiadomości radiowych
    if (radio.receiveFromMini(payload))
    {
        switch (payload.type) {
        case PutLightRespR:
            messagePut(payload.value);
            break;
        case GetButtonObserveRespR:
            messageGetObserve(payload.value);
            break;
        case GetButtonRespR:
            messageGet(payload.value);
            break;
        case GetLightRespR:
            messageGet(payload.value);
            break;
        case IfTestRespR:
        {
        Serial.print("Mini dostal: ");
        Serial.println(payload.value);
        Serial.print("Czas wysylania pakietow: ");
        Serial.println(millis() - startTestTime);
        statsGet(millis() - startTestTime);
        }

        }
    }
}
