#ifndef _SIMCOM_H_
#define _SIMCOM_H_

#include <Arduino.h>
#include <list>
#include <initializer_list>

#include <Print.h>

#define LOG_ENABLE
#define DEBUG_ENABLE

#define LOG_ASYNC

#ifndef LOG_ASYNC
    #define SIMCOM_LOG( format, ... ) \
            Serial.printf(format, ##__VA_ARGS__);if(_log != nullptr){_log->printf( format, ##__VA_ARGS__);}
#else
    #define SIMCOM_LOG( format, ... )  do{\
                Serial.printf(format, ##__VA_ARGS__);\
                if(_log != nullptr){\
                    char* buffptr = nullptr;\
                    asprintf(&buffptr,format, ##__VA_ARGS__);\
                    if( buffptr != nullptr ){if( xQueueSend(logQueue, &buffptr, (TickType_t)10) == pdFALSE ){free(buffptr);}}}\
                }while(0);
#endif

class simcom
{
public:
    simcom(){}
    ~simcom(){}
    void begin(HardwareSerial & uart,Print *stream = nullptr);

    void setlogStream(Print *stream){_log = stream;};
    
    void sendCMD(String str);
    int sendCMDAndFindUntil(String str,std::initializer_list<String> args,int retry = 5,int max_line = 10,int timeout = 1000);
    int sendCMDAndWaitRevice(String str,std::initializer_list<String> args,String reviceMark,String *revidePtr = nullptr,int retry = 5,int max_line = 10,int timeout = 1000);
    int waitUntill(String reviceMark,String *revidePtr = nullptr,int timeout = 10);
    String sendCMDAndRevice(String str);

#ifdef LOG_ASYNC
    QueueHandle_t logQueue = nullptr;
#endif

private:
    HardwareSerial *_serialPtr = nullptr;
    Print *_log = nullptr;



    
};


#endif