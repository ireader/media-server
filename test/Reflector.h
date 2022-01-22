
#ifndef _REFLECTOR_H
#define _REFLECTOR_H

#include <map>
#include <string>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

class Reflector{
public:
    typedef std::function<int(int argc, char const *argv[])> FuncType;

private:
    std::map<std::string, std::pair<std::string, FuncType>> objectMap;

public:
    std::vector<std::string> getAllRegisterFun(){
        std::vector<std::string> re;
        for(auto& x : objectMap){
            re.push_back(x.second.first);
        }
        return re;
    }

    bool registerFun(const char* name, const std::string& proto, FuncType && generator){
        assert(objectMap.find(name) == objectMap.end());
        objectMap[name] = std::make_pair(proto, generator);
        return true;
    }

    bool runFun(const char* name, int argc, char const *argv[]){
        auto ptr = objectMap.find(name);
        if(ptr == objectMap.end()){
            std::cout << "err! not find "<< name << std::endl;
            return false;
        }
        ptr->second.second(argc, argv);
        return true;
    }

    static Reflector* Instance(){
        static Reflector ptr;
        return &ptr;
    }
};

#endif