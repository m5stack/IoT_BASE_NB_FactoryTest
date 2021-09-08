#ifndef _STRINGLIST_H_
#define _STRINGLIST_H_

#include <Arduino.h>
#include <initializer_list>

#include <vector>
using namespace std;

class StringList
{
public:
    StringList(){};
    StringList(std::initializer_list<String> args ){ for( const String &item : args ){ _list.push_back(item);}};
    StringList(String str,char c){ this->split(str,c);};
    ~StringList(){};

    String at(int i){ return _list.at(i);}
    size_t size(){ return _list.size();}

    int split(String str,char c)
    {
        if( str.isEmpty()){ return 0;}
        _list.clear();
        int index = -1;
        do{
            index = str.indexOf(',');
            if( index > 0 )
            {
                String _str = str.substring(0,index);
                if( !_str.isEmpty()){ _list.push_back(_str); }
                str = str.substring(index + 1);
            }
            else if( index < 0 ){ if( !str.isEmpty()){ _list.push_back(str); break;}}
            else{ str = str.substring(0,1);}
        }while(!str.isEmpty());

        return _list.size();
    }

    void showList(Print *p)
    {
        p->print("[LIST]");
        for (int i = 0; i < _list.size(); i++)
        {
            p->printf("%s ",_list.at(i).c_str());
        }
        p->println(" ");
    }

private:
    vector<String>_list;
};


#endif