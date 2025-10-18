#ifndef ServerLib_h
#define ServerLib_h

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

extern bool newRequest;
extern DNSServer dnsServer;
extern char message;
extern bool isWordMode;
extern String currentWord;
extern int currentLetterIndex;
void StartServer();


#endif