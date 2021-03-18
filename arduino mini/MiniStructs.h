#pragma once

//struktura określająca payload
struct our_payload {
    unsigned long value;      //wartość payloadu
    unsigned short type;    //typ payloadu
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
    GetLightReqR = 0,        //pobranie wartosci lampki
    PutLightReqR,            //ustawienie wartosci lampki
    GetButtonReqR,           //pobranie wartosci przycisku
    GetButtonObserveReqR,    //obserwowanie wartosci przycisku
    GetRadioStatsReqR,           //wiadomosc testowa statystyk
    IfTestReqR
};
