#include "simcom.h"

void simcom::begin(HardwareSerial & uart,Print *stream)
{
    _serialPtr = &uart;
    _serialPtr->setTimeout(1000);
    _log = stream;

#ifdef LOG_ASYNC
    logQueue = xQueueCreate(32, sizeof(char*));
#endif
}

void simcom::sendCMD(String str)
{
    _serialPtr->write(str.c_str());
}

int simcom::sendCMDAndFindUntil(String str,std::initializer_list<String> args,int retry,int max_line,int timeout)
{
    int strlist_size = args.size();
    if( strlist_size <= 0 ) return -1;

    String *strbuff = new String[strlist_size];
    
    int cnt = 0;

    for( const String &item : args )
    {
        strbuff[cnt] = item;
        cnt++;
    }

    _serialPtr->setTimeout(timeout);

    while (retry >= 0 )
    {
        _serialPtr->write(str.c_str());

        SIMCOM_LOG("[SEND]%s\r\n",str.c_str());
    
        Serial.println(str.c_str());

        int line_cnt = 0;

        while( line_cnt < max_line )
        {
            String reviceStr = _serialPtr->readStringUntil('\n');
            
            if( !reviceStr.isEmpty())
            {
                reviceStr.replace("\r",""); reviceStr.replace("\n",""); reviceStr.trim();
                if( reviceStr.length() >= 1 )
                {
                    SIMCOM_LOG("[REVC]%s\r\n",reviceStr.c_str());

                    for (int i = 0; i < strlist_size; i++)
                    {
                        if( reviceStr.indexOf(strbuff[i]) >= 0 ){ delete[] strbuff; return i; }
                    }
                }
            }
            line_cnt++;
        }
        retry --;
    }
    delete[] strbuff;
    return -1;
}

int simcom::sendCMDAndWaitRevice(String str,std::initializer_list<String> args,String reviceMark,String *revidePtr,int retry,int max_line,int timeout)
{
    int strlist_size = args.size();
    if( strlist_size <= 0 ) return -1;

    String *strbuff = new String[strlist_size];
    
    int cnt = 0;

    for( const String &item : args )
    {
        strbuff[cnt] = item;
        cnt++;
    }

    _serialPtr->setTimeout(timeout);

    while (retry >= 0 )
    {
        _serialPtr->write(str.c_str());
        SIMCOM_LOG("[SEND]%s\r\n",str.c_str());

        int line_cnt = 0;

        while( line_cnt < max_line )
        {
            String reviceStr = _serialPtr->readStringUntil('\n');
            if( !reviceStr.isEmpty())
            {
                reviceStr.replace("\r",""); reviceStr.replace("\n",""); reviceStr.trim();
                if( reviceStr.length() >= 1 )
                {
                    SIMCOM_LOG("[REVC]%s\r\n",reviceStr.c_str());

                    if(( reviceStr.indexOf(reviceMark) >= 0 ) && ( revidePtr != nullptr ))
                    {
                        int pos = reviceStr.indexOf(reviceMark) + reviceMark.length();
                        *revidePtr = reviceStr.substring(pos);
                    }
                    else
                    {
                        for (int i = 0; i < strlist_size; i++)
                        {
                            if( reviceStr.indexOf(strbuff[i]) >= 0 ){ delete[] strbuff; return i; }
                        }
                    }
                }
            }
            line_cnt++;
        }
        retry --;
    }
    delete[] strbuff;
    return -1;
}

int simcom::waitUntill(String reviceMark,String *revidePtr,int timeout)
{
    String reviceStr = _serialPtr->readStringUntil('\n');
    if( !reviceStr.isEmpty())
    {
        reviceStr.replace("\r",""); reviceStr.replace("\n",""); reviceStr.trim();
        if( reviceStr.length() >= 1 )
        {
            SIMCOM_LOG("[REVC]%s\r\n",reviceStr.c_str());

            if(( reviceStr.indexOf(reviceMark) >= 0 ) && ( revidePtr != nullptr ))
            {
                int pos = reviceStr.indexOf(reviceMark) + reviceMark.length();
                *revidePtr = reviceStr.substring(pos);
            }
        }
    }
    return 0;
}


String simcom::sendCMDAndRevice(String str)
{
    return String();
}
