#pragma once
//struktura definiująca kształt payloadu (w komunikacji radiowej)
struct our_payload {
    unsigned long value;      //wartość niesiona przez payload
    unsigned short type;      //typ payloadu, zdefiniowany w typie wyliczeniowym
};

//typy jakie możemy nadać strukturze our_payload (responses)
enum PayloadResp {
    GetLightRespR = 0,        //pobranie wartosci lampki
    PutLightRespR,            //ustawienie wartosci lampki
    GetButtonRespR,           //pobranie wartosci przycisku
    GetButtonObserveRespR,    //obserwowanie wartosci przycisku
    GetRadioStatsRespR,        //wiadomosc testowa statystyk
    IfTestRespR
};

//typy jakie możemy nadać strukturze our_payload (requests)
enum PayloadReq {
    GetLightReqR = 0,         //pobranie wartosci lampki
    PutLightReqR,             //ustawienie wartosci lampki
    GetButtonReqR,            //pobranie wartosci przycisku
    GetButtonObserveReqR,     //obserwowanie wartosci przycisku
    GetRadioStatsReqR,         //wiadomosc testowa statystyk
    IfTestReqR
};

//typy zapytan Coap dla wykorzystywanych przez nas obiektow
enum CoapReqType {
    DISCOVER, 
    LIGHT_GET, 
    LIGHT_PUT, 
    BUTTON_GET, 
    BUTTON_GET_OBSERVE, 
    CLIENT_ERROR, 
    RST, 
    UNKNOWN, 
    METHOD_NOT_ALLOWED, 
    NOT_FOUND,
    OUR_STATS
};

//struktura wiadomości CoAP bez opcji
struct CoapMessage {
    byte coapVersion;         //wersja CoAPa
    byte type;                //CON=0, NON=1, ACK=2, RST=3, ten projekt wspiera NON
    byte tokenLength;         //dlugosc tokena
    byte c;                   //definiuje klasę wiadomości pole kod 
    byte dd;                  //definiuje szczegóły pola kod
    byte mid[2];              //Message ID, czyli detekcja powtórzenia
    byte token[8];            //token, dopasowuje odpowiedź do zapytania
    char payload[5];          //payload
};

//struktura opcje CoAP
struct CoapMessageOptions {
    bool observe;             //6
    char uriPath[255];        //11
    byte contentFormat[2];    //12
    byte acceptOption[2];     //17
    byte block2[3];           //23, bo "zasob DUZY", koło 60 bajtow
    byte size2[2];            //28
};
