#include <M5Stack.h>

#include "esp_heap_caps.h"

#include "Fonts/EVA_11px.h"
#include "Fonts/EVA_20px.h"

#include <WiFi.h>
#include <HTTPClient.h>

#include "resource.h"

#include "TFTTerminal.h"

TFT_eSprite t_sprite(&M5.Lcd);
TFTTerminal nb_iot_terminal(&t_sprite);

TFT_eSprite porta_sprite(&M5.Lcd);
TFT_eSprite portb_sprite(&M5.Lcd);
TFT_eSprite portc_sprite(&M5.Lcd);

TFT_eSprite state_title_sprite(&M5.Lcd);

#include "simcom.h"

#include "StringList.h"

#include "Adafruit_SGP30.h"

Adafruit_SGP30 sgp;

simcom sim7020;

#define SIM_STATE_SIMCARD   0x01
#define SIM_STATE_SIGNAL    0x02
#define SIM_STATE_NETWORK   0x04
#define SIM_STATE_TCP       0x08
#define SIM_STATE_MQTT      0x10

#define PORT_SWITCH_PORTC   0x01
#define PORT_SWITCH_RS485   0x02

struct _sys
{
    uint8_t sim7020state;
    int rssi;
    int portswitch;
    bool qrcodeisopen;
    int mqttid;
    int cnt;
    bool pinout_state;
    uint16_t rx_cnt;
    uint16_t tx_cnt;

    int iot_cnt;
    QueueHandle_t mqttmsgQueue;
    uint8_t mac[6];
    String url;

    bool test_flag;

}sys = {.sim7020state = 0,.rssi = 0,.portswitch = PORT_SWITCH_PORTC,.qrcodeisopen = false,.mqttid = -1,.cnt = 0,.pinout_state = false,.rx_cnt = 0,.tx_cnt = 0};

bool isInRange(int i,int mim,int max) { return (( i >= mim )&&( i <= max )) ? true : false; }

int16_t rssimap(int rssiraw){ if(!isInRange(rssiraw,3,30))return 0; return map(rssiraw,3,30,-105,-48);}

void flushIcons(uint8_t state)
{
    if( sys.test_flag )
    {
        //Serial.printf("rssi:%d\r\n",rssimap(sys.rssi));
        String str(rssimap(sys.rssi),10);
        str = str + "dbm";
        M5.Lcd.fillRect(140,135,90,32,TFT_BLACK);
        M5.Lcd.drawString(str,140,135,4);

        M5.Lcd.fillRect(10,95 ,10,20,(state & SIM_STATE_NETWORK) ? TFT_GREEN :TFT_RED);
        M5.Lcd.fillRect(10,135,10,20,(state & SIM_STATE_NETWORK) ? TFT_GREEN :TFT_RED);
        M5.Lcd.fillRect(10,175,10,20,(state & SIM_STATE_NETWORK) ? TFT_GREEN :TFT_RED);
    }
    else
    {
        state_title_sprite.fillSprite(state_title_sprite.color565(0x37,0x37,0x37));
        state_title_sprite.drawColorBitmap(6,5,18,15, image_NetWork,(state & SIM_STATE_NETWORK)?0xffffff:0x454545,0x373737);
        state_title_sprite.drawColorBitmap(28,5,18,15,image_Signal, (state & SIM_STATE_SIGNAL )?0xffffff:0x454545,0x373737);
        state_title_sprite.drawColorBitmap(50,5,18,15,image_SIMCard,(state & SIM_STATE_SIMCARD)?0xffffff:0x454545,0x373737);
        state_title_sprite.pushSprite(246,0);
    }
}

void checkTCPConnect(String str)
{
    StringList csq_parm(str,',');
    if( csq_parm.size() == 3 )
    {
        if( csq_parm.at(1).toInt() == 1 )
        {
            sys.sim7020state |= SIM_STATE_TCP;
            sys.mqttid = csq_parm.at(0).toInt();
            Serial.printf("[MQTT] MQTT ID%d\r\n", sys.mqttid);
        }
        else
        {
            sys.sim7020state &= (~SIM_STATE_TCP);
            sys.mqttid = -1;
        }
    }
}

void nbiotTask(void* arg)
{
    sys.iot_cnt = 100;
    while(1)
    {
        char *msgbuff = nullptr;
        if (xQueueReceive(sys.mqttmsgQueue, &msgbuff, (TickType_t)10) == pdTRUE)
        {
            if(( sys.sim7020state & SIM_STATE_NETWORK )&&( sys.sim7020state & SIM_STATE_MQTT )&&( msgbuff != nullptr))
            {
                char mqtt_send_buff[1024];
                sprintf(mqtt_send_buff,"AT+CMQPUB=%d,\"$m5/nb_iot/base15\",1,0,0,%d,\"%s\"\r\n",sys.mqttid,strlen(msgbuff),msgbuff);
                sim7020.sendCMDAndFindUntil(mqtt_send_buff,{"OK","ERROR"});
            }
        }
        sys.iot_cnt ++;
        if( sys.iot_cnt == 100 )
        {
            String str;
            if( sim7020.sendCMDAndWaitRevice("AT+CSQ\r\n",{"OK","ERROR"},"+CSQ: ",&str) == 0 )
            {
                StringList csq_parm(str,',');
                if( csq_parm.size() >= 2 )
                {
                    Serial.printf("[RSSI] %ld,%ld\r\n",csq_parm.at(0).toInt(),csq_parm.at(1).toInt());
                    sys.rssi = csq_parm.at(0).toInt();
                    if( isInRange(csq_parm.at(0).toInt(),3,31)){ sys.sim7020state |= SIM_STATE_SIGNAL; };
                }
            }
            if( sim7020.sendCMDAndWaitRevice("AT+CGREG?\r\n",{"OK","ERROR"},"+CGREG: ",&str) == 0 )
            {
                StringList csq_parm(str,',');
                if( csq_parm.size() >= 2 )
                {
                    if(( csq_parm.at(0).toInt() == 0 )&&( csq_parm.at(1).toInt() == 1 ))
                    { 
                        sys.sim7020state |= SIM_STATE_NETWORK; 
                    }
                }
            }
            //sys.iot_cnt = ( sys.sim7020state & SIM_STATE_NETWORK ) ? 0 : 80;
        }

        if(( sys.sim7020state & SIM_STATE_NETWORK ) && (!( sys.sim7020state & SIM_STATE_TCP ))&&( !sys.test_flag ))
        {
            String str;
            if( sim7020.sendCMDAndWaitRevice("AT+CMQNEW?\r\n",{"OK","ERROR"},"+CMQNEW: ",&str) == 0 )
            {
                checkTCPConnect(str);
            }
            if(!( sys.sim7020state & SIM_STATE_TCP ))
            {
                if( sim7020.sendCMDAndWaitRevice("AT+CMQNEW=\"120.24.58.30\",\"1883\",12000,1024\r\n",{"OK","ERROR"},"+CMQNEW: ",&str) == 0 )
                {
                    StringList csq_parm(str,',');
                    if( csq_parm.size() == 1 )
                    {
                        sys.sim7020state |= SIM_STATE_TCP;
                        sys.mqttid = csq_parm.at(0).toInt();
                        Serial.printf("[MQTT] MQTT ID%d\r\n", sys.mqttid);
                    }
                }
            }
        }

        if(( sys.sim7020state & SIM_STATE_TCP ) && (!( sys.sim7020state & SIM_STATE_MQTT )))
        {
            String str;
            if( sim7020.sendCMDAndWaitRevice("AT+CMQCON?\r\n",{"OK","ERROR"},"+CMQCON: ",&str) == 0 )
            {
                StringList csq_parm(str,',');
                if( csq_parm.size() == 3 )
                {
                    sys.sim7020state = ( csq_parm.at(1).toInt() == 1 ) ? ( sys.sim7020state | SIM_STATE_MQTT ) : ( sys.sim7020state & (~SIM_STATE_MQTT));
                }
            }
            if((!( sys.sim7020state & SIM_STATE_MQTT ))&&( sys.mqttid != -1))
            {
                char mqttConnectBuff[1024];
                sprintf(mqttConnectBuff,"AT+CMQCON=%d,3,\"nb-iot-base15\",60,1,0\r\n",sys.mqttid);

                if( sim7020.sendCMDAndFindUntil(mqttConnectBuff,{"OK","ERROR"}) == 0 )
                {
                    sys.sim7020state |= SIM_STATE_MQTT;              
                }
                else
                {
                    sys.sim7020state &= (~SIM_STATE_MQTT);  
                }
            }
        }
        
        if( sys.iot_cnt == 110 )
        {
            sys.iot_cnt = ( sys.sim7020state & SIM_STATE_NETWORK ) ? 0 : 80;
        }
        
        vTaskDelay(50);
    }
}

int findStr(String str,String rxp)
{
    if( str.isEmpty()) return -1;

    int start_pos = str.indexOf("$SE");
    int end_pos = str.indexOf("......#");

    if(( start_pos < 0 )||( end_pos < 0 )||( start_pos > end_pos)) return -1;
    String datastr = str.substring(start_pos,end_pos);
    
    Serial.printf("data:%s\r\n",datastr.c_str());

    StringList list(datastr,',');
    Serial.printf("List size %d\r\n",list.size());

    if( list.size() != 5 ) return -1;

    if(( list.at(1) == "AA" )&&( list.at(2) == "55" )&&( list.at(3) == "10" )&&( list.at(4) == rxp ))
    {
        return 0;
    }
    return -1;
}

bool isRS485Fixture(HardwareSerial &uart)
{
    int rs485_state = -1,cnt = 0;

    Serial1.setTimeout(100);

    do{
        uart.print("$RE,AA,55,01,AAA,......#\r\n");
        String revice_str = uart.readStringUntil('\n');
        if( !revice_str.isEmpty())
        {
            rs485_state = findStr(revice_str,"AAA");
        }
        cnt ++;
    }while(( rs485_state == -1 )&&( cnt < 5 ));

    if( rs485_state == -1 ) return false;

    return true;
}

int findI2C( TwoWire &pWire )
{
    for (int i = 0; i < 127; i++)
    {
        pWire.beginTransmission(i);
        if( pWire.endTransmission() == I2C_ERROR_OK )
        {
            Serial.printf("Find %02x \r\n",i);
            if( i == 0x76 ) return 0;
        }
    }
    return -1;
}

uint8_t ioTest(uint8_t pinin,uint8_t pinout)
{
    pinMode(pinin, INPUT);
    pinMode(pinout, OUTPUT);

    uint8_t mark = 0x00;

    for (int i = 0; i < 8; i++)
    {
        mark <<= 1;
        digitalWrite(pinout,( i % 2 == 0 ) ? HIGH : LOW );
        delay(1);
        mark |= digitalRead(pinin);
        delay(1);
    }
    return mark;
}

void hardwareError()
{
    while(1)
    {
        M5.update();
        delay(100);
    }
}


void setup()
{
	M5.begin(true,false,true,true);
    M5.Lcd.drawJpg(nb_iot_bk_jpeg,50695,0,0,320,240);

    t_sprite.createSprite(167,116);
    nb_iot_terminal.setGeometry(122,100,167,116);
    nb_iot_terminal.setcolor(M5.Lcd.color565(0x00,0xff,0x00),M5.Lcd.color565(0x1c,0x1c,0x1c));

    porta_sprite.createSprite(93,42);
    porta_sprite.setFreeFont(&EVA_11px);
    porta_sprite.setTextColor(TFT_BLACK,porta_sprite.color565(0xc3,0xc3,0xc3));

    portb_sprite.createSprite(93,42);
    portb_sprite.setFreeFont(&EVA_11px);
    portb_sprite.setTextColor(TFT_BLACK,porta_sprite.color565(0xc3,0xc3,0xc3));

    portc_sprite.createSprite(93,42);
    portc_sprite.setFreeFont(&EVA_11px);
    portc_sprite.setTextColor(TFT_BLACK,porta_sprite.color565(0xc3,0xc3,0xc3));

    state_title_sprite.createSprite(74,24);
    
    flushIcons(sys.sim7020state);

    Serial1.begin(9600, SERIAL_8N1, 13,15);
    Serial2.begin(115200, SERIAL_8N1, 35, 0);
    sim7020.begin(Serial2,&nb_iot_terminal);

    sys.test_flag = isRS485Fixture(Serial1);

	pinMode(12, OUTPUT);

    //----------PORT B -INITIALIZE-----------
    pinMode(36, INPUT);
    pinMode(26, OUTPUT);

	digitalWrite(12, 0);
    delay(500);
	digitalWrite(12, 1);
    delay(500);

    if( sys.test_flag )
    {
        Serial.printf("Test Mode\r\n");

        M5.Lcd.fillRect(0, 0, 320, 240, TFT_BLACK);
        M5.Lcd.fillRect(0, 0, 320, 40, TFT_WHITE);
        M5.Lcd.setTextColor(TFT_BLACK);
        M5.Lcd.setTextDatum(TC_DATUM);
        M5.Lcd.drawString("BASE15 NB-IOT TEST", 160, 10, 4);

        M5.Lcd.setTextDatum(TL_DATUM);
        
        if( findI2C(Wire) == -1 )
        {
            M5.Lcd.setTextColor(TFT_RED);
            M5.Lcd.drawString("PORTA hardware error", 10, 55, 4);
            hardwareError();
        }
        if( ioTest(36,26) != 0xaa )
        {
            M5.Lcd.setTextColor(TFT_RED);
            M5.Lcd.drawString("PORTB hardware error", 10, 55, 4);
            hardwareError();
        }
        if( ioTest(16,17) != 0xaa )
        {
            M5.Lcd.setTextColor(TFT_RED);
            M5.Lcd.drawString("PORTC hardware error", 10, 55, 4);
            hardwareError();
        }
        M5.Lcd.setTextColor(TFT_GREEN);
        M5.Lcd.drawString("PORTA,B,C Test passed", 10, 55, 4);

        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.drawString("SIM CARD",   30, 95 , 4);
        M5.Lcd.drawString("SIGNAL",     30, 135, 4);
        M5.Lcd.drawString("NETWORK",    30, 175, 4);

        M5.Lcd.fillRect(10,95 ,10,20,TFT_RED);
        M5.Lcd.fillRect(10,135,10,20,TFT_RED);
        M5.Lcd.fillRect(10,175,10,20,TFT_RED);
    }
    else
    {
        if (!sgp.begin())
        {
            Serial.println("Sensor not found :(");
        }
    }

    if( sim7020.sendCMDAndFindUntil("AT\r\n",{"OK","ERROR"}) != 0 ){};
    if( sim7020.sendCMDAndFindUntil("ATE0\r\n",{"OK","ERROR"}) != 0 ){};

    String str;

    for( int i = 0 ; i < 10; i++ )
    {
        if( sim7020.sendCMDAndWaitRevice("AT+CPIN?\r\n",{"OK","ERROR"},"+CPIN: ",&str) == 0 )
        {
            if( str.indexOf("READY") >= 0 ){ sys.sim7020state |= SIM_STATE_SIMCARD; break;}
        }
        delay(100);
    }
    flushIcons(sys.sim7020state);
    
    if( sim7020.sendCMDAndFindUntil("AT+CMQTSYNC=1\r\n",{"OK","ERROR"}) != 0 ){};
    if( sim7020.sendCMDAndFindUntil("AT+CREVHEX=0\r\n",{"OK","ERROR"}) != 0 ){};

    esp_read_mac(sys.mac,ESP_MAC_WIFI_STA);
    char urlstrbuff[128];
    sprintf(urlstrbuff,"http://120.24.58.30/m5/basenbiot?id=%02x%02x%02x%02x%02x%02x",MAC2STR(sys.mac));
    sys.url = String(urlstrbuff);

    sys.mqttmsgQueue = xQueueCreate(32, sizeof(char*));
    xTaskCreatePinnedToCore(nbiotTask, "nbiotTask", 4096, nullptr, 10, nullptr, 0);
}

void nbiotFlush()
{
    sys.cnt ++;
    if( sys.cnt == 100 )
    {
        String str;
        if( sim7020.sendCMDAndWaitRevice("AT+CSQ\r\n",{"OK","ERROR"},"+CSQ: ",&str) == 0 )
        {
            StringList csq_parm(str,',');
            if( csq_parm.size() >= 2 )
            {
                Serial.printf("[RSSI] %ld,%ld\r\n",csq_parm.at(0).toInt(),csq_parm.at(1).toInt());
                sys.rssi = csq_parm.at(0).toInt();
                if( isInRange(csq_parm.at(0).toInt(),3,31)){ sys.sim7020state |= SIM_STATE_SIGNAL; };
            }
        }
        flushIcons(sys.sim7020state);
    }

    if( sys.cnt == 150 )
    {
        String str;
        if( sim7020.sendCMDAndWaitRevice("AT+CGREG?\r\n",{"OK","ERROR"},"+CGREG: ",&str) == 0 )
        {
            StringList csq_parm(str,',');
            if( csq_parm.size() >= 2 )
            {
                if(( csq_parm.at(0).toInt() == 0 )&&( csq_parm.at(1).toInt() == 1 ))
                { 
                    sys.sim7020state |= SIM_STATE_NETWORK; 
                }
            }
        }
        flushIcons(sys.sim7020state);
    }

    if(( sys.sim7020state & SIM_STATE_NETWORK ) && (!( sys.sim7020state & SIM_STATE_TCP )))
    {
        String str;
        if( sim7020.sendCMDAndWaitRevice("AT+CMQNEW?\r\n",{"OK","ERROR"},"+CMQNEW: ",&str) == 0 )
        {
            checkTCPConnect(str);
        }
        if(!( sys.sim7020state & SIM_STATE_TCP ))
        {
            if( sim7020.sendCMDAndWaitRevice("AT+CMQNEW=\"120.24.58.30\",\"1883\",12000,1024\r\n",{"OK","ERROR"},"+CMQNEW: ",&str) == 0 )
            {
                StringList csq_parm(str,',');
                if( csq_parm.size() == 1 )
                {
                    sys.sim7020state |= SIM_STATE_TCP;
                    sys.mqttid = csq_parm.at(0).toInt();
                    Serial.printf("[MQTT] MQTT ID%d\r\n", sys.mqttid);
                }
            }
        }
    }

    if(( sys.sim7020state & SIM_STATE_TCP ) && (!( sys.sim7020state & SIM_STATE_MQTT )))
    {
        String str;
        if( sim7020.sendCMDAndWaitRevice("AT+CMQCON?\r\n",{"OK","ERROR"},"+CMQCON: ",&str) == 0 )
        {
            StringList csq_parm(str,',');
            if( csq_parm.size() == 3 )
            {
                /*
                if(csq_parm.at(1).toInt() == 1 ) sys.sim7020state |= SIM_STATE_MQTT;
                else sys.sim7020state &= (~SIM_STATE_MQTT);
                */
                sys.sim7020state = ( csq_parm.at(1).toInt() == 1 ) ? ( sys.sim7020state | SIM_STATE_MQTT ) : ( sys.sim7020state & (~SIM_STATE_MQTT));
            }
        }
        if((!( sys.sim7020state & SIM_STATE_MQTT ))&&( sys.mqttid != -1))
        {
            char mqttConnectBuff[1024];
            sprintf(mqttConnectBuff,"AT+CMQCON=%d,3,\"nb-iot-base15\",60,1,0\r\n",sys.mqttid);

            if( sim7020.sendCMDAndFindUntil(mqttConnectBuff,{"OK","ERROR"}) == 0 )
            {
                sys.sim7020state |= SIM_STATE_MQTT;              
            }
            else
            {
                sys.sim7020state &= (~SIM_STATE_MQTT);  
            }
        }
    }

    if( sys.cnt == 200 )
    {
        if(( sys.sim7020state & SIM_STATE_NETWORK )&&( sys.sim7020state & SIM_STATE_MQTT ))
        {
            String mqtt_send_str("{ 'id': '123456','msg': 'Hello M5Stack'}");
            char mqtt_send_buff[1024];
            sprintf(mqtt_send_buff,"AT+CMQPUB=%d,\"$m5/nb_iot/base15\",1,0,0,%d,\"%s\"\r\n",sys.mqttid,mqtt_send_str.length(),mqtt_send_str.c_str());
            sim7020.sendCMDAndFindUntil(mqtt_send_buff,{"OK","ERROR"});
            
        }
        sys.cnt = 0;
    }

    if( sys.cnt % 10 == 0 )
    {
        char strbuff[128];
        sgp.IAQmeasure();
        Serial.printf("[TVOC] %d\r\n", sgp.TVOC);
        Serial.printf("[eCO2] %d\r\n", sgp.eCO2);
        porta_sprite.fillSprite(porta_sprite.color565(0xc5,0xc5,0xc5));

        sprintf(strbuff,"[TVOC]%dppb/t",sgp.TVOC);  porta_sprite.drawString(strbuff,2,5);
        sprintf(strbuff,"[eCO2]%dppm",sgp.eCO2);  porta_sprite.drawString(strbuff,2,20);

        porta_sprite.pushSprite(8,26);

        nb_iot_terminal.printf("[TVOC]%dppb/t [eCO2]%dppm \r\n",sgp.TVOC,sgp.eCO2);
    }
}

void loop()
{

    if(( sys.cnt == 10 )&&( !sys.test_flag ))
    {
        sys.cnt = 0;
        sys.pinout_state = ( sys.pinout_state ) ? false : true;
        digitalWrite(26, sys.pinout_state ? HIGH : LOW );
        sgp.IAQmeasure();
        if( !sys.qrcodeisopen )
        {
            char strbuff[128];
            porta_sprite.fillSprite(porta_sprite.color565(0xc5,0xc5,0xc5));
            sprintf(strbuff,"[TVOC]%dppb/t",sgp.TVOC);  porta_sprite.drawString(strbuff,2,5);
            sprintf(strbuff,"[eCO2]%dppm",sgp.eCO2);  porta_sprite.drawString(strbuff,2,20);
            porta_sprite.pushSprite(8,26);

            nb_iot_terminal.printf("[TVOC]%dppb/t [eCO2]%dppm \r\n",sgp.TVOC,sgp.eCO2);
            flushIcons(sys.sim7020state);

            portb_sprite.fillSprite(porta_sprite.color565(0xc5,0xc5,0xc5));
            portb_sprite.drawString((digitalRead(36) == HIGH ) ? "IN[36]:HIGH" : "IN[36]:LOW",2,5);
            portb_sprite.drawString((sys.pinout_state) ? "OUT[26]:HIGH" : "OUT[26]:LOW",2,20);
            portb_sprite.pushSprite(8,100);

            portc_sprite.fillSprite(porta_sprite.color565(0xc5,0xc5,0xc5));
            sprintf(strbuff,"[RX]:%d",sys.rx_cnt);  portc_sprite.drawString(strbuff,2,5);
            sprintf(strbuff,"[TX]:%d",sys.tx_cnt);  portc_sprite.drawString(strbuff,2,20);
            portc_sprite.pushSprite(8,174);
        }
    }

    sys.cnt ++;

    if( !sys.test_flag )
    {
        if( M5.BtnA.wasPressed()&&( !sys.qrcodeisopen )&&(sys.portswitch != PORT_SWITCH_PORTC))
        {
            sys.portswitch = PORT_SWITCH_PORTC;
            M5.Lcd.drawJpg(part1_portc_jpeg_img,14547,0,151,103,69);
            Serial1.begin(9600, SERIAL_8N1, 16,17);
            sys.tx_cnt = 0;
            sys.rx_cnt = 0;
        }
        if( M5.BtnB.wasPressed())
        {
            sys.qrcodeisopen = ( sys.qrcodeisopen ) ? false : true;
            if( sys.qrcodeisopen )
            {
                M5.Lcd.drawJpg(qrcode_img,19554,60,20,200,200);
                M5.Lcd.qrcode(sys.url,80,30,160,5);
            }
            else
            {
                M5.Lcd.drawJpg(nb_iot_bk_jpeg,50695,0,0,320,240);
                flushIcons(sys.sim7020state);
            }
        }
        if( M5.BtnC.wasPressed()&&( !sys.qrcodeisopen )&&(sys.portswitch != PORT_SWITCH_RS485))
        {
            sys.portswitch = PORT_SWITCH_RS485;
            M5.Lcd.drawJpg(part1_rs485_jpeg_img,14470,0,151,103,69);
            Serial1.begin(9600, SERIAL_8N1, 13,15);
            sys.tx_cnt = 0;
            sys.rx_cnt = 0;
        }
        if( Serial1.available() != 0 )
        {
            sys.rx_cnt += Serial1.available();
            Serial1.flush();
        }
    }
    else
    {
        if( sys.cnt >= 20 )
        {
            sys.cnt = 0;
            flushIcons(sys.sim7020state);
        }
    }

    char *logbuff = nullptr;
    if (xQueueReceive(sim7020.logQueue, &logbuff, (TickType_t)10) == pdTRUE)
    {
        if( logbuff != nullptr )
        {
            if(( !sys.qrcodeisopen )&&( !sys.test_flag ))
            {
                nb_iot_terminal.printf("%s",logbuff);
            }
            free(logbuff);
        }
    }

	M5.update();
    delay(10);

    //Serial.printf("%d\r\n",heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
}