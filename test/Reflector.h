
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
    std::map<std::string, FuncType> objectMap;
    public:
    std::vector<std::string> getAllRegisterFun(){
        std::vector<std::string> re;
        for(auto& x : objectMap){
            re.push_back(x.first);
        }
        return re;
    }

    bool registerFun(const char* name, FuncType && generator){
        for(auto& x : objectMap){
            if(x.first == name){
                return false;
            }
        }
        objectMap[name] = generator;
        return true;
    }

    bool runFun(const char* name, int argc, char const *argv[]){
        auto ptr = objectMap.find(name);
        if(ptr == objectMap.end()){
            std::cout << "err! not find "<< name << std::endl;
            return false;
        }
        ptr->second(argc, argv);
        return true;
    }

    static std::shared_ptr<Reflector> Instance(){
        static std::shared_ptr<Reflector> ptr;
        if(ptr == nullptr){
            ptr.reset(new Reflector());
        }
        return ptr;
    }
};

#define RE_REGISTER(name) Reflector::Instance()->registerFun(#name, &name)

#define RE_REGISTER_SETNAME(name, func) Reflector::Instance()->registerFun(#name, &func)

#define RE_RUN_REG(name,argc,argv) Reflector::Instance()->runFun(name, argc, argv)


#define RE_GET_REG Reflector::Instance()->getAllRegisterFun

#endif