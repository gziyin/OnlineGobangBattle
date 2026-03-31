#include<iostream>
#include<sstream>
#include<string>
#include<vector>
#include<json/json.h>

//使用jsoncpp库进行多数据对象的序列化
std::string serializeJson( Json::Value &root)
{
    //1. 将需要序列化的数据，存储在Json：:Value对象中
    root["name"] = "张三";
    root["age"] = 20;
    root["grade"].append("88");
    root["grade"].append("98");
    root["grade"].append("78");
    //2.实例化一个StreamWriterBuilder工厂类对象
    Json::StreamWriterBuilder builder;
    builder["emitUTF8"] = true; //设置输出的字符串为UTF-8编码格式
    //3.通过工厂类对象创建一个StreamWriter对象
    Json::StreamWriter * writer = builder.newStreamWriter();
    //4.调用StreamWriter对象的write函数，将Json：:Value对象序列化
    std:: stringstream ss;
    int ret=writer->write(root,&ss);
    if(ret!=0)
    {
        std:: cout << "序列化失败" << std::endl;
        return "";
    }
    std:: cout << ss.str() << std::endl;
    delete writer;
    return ss.str();
}

void unserializeJson(const std::string &jsonStr)
{
    //1.实例化一个Json：:Value对象
    Json::Value root;
    //2.实例化一个CharReaderBuilder工厂类对象
    Json::CharReaderBuilder builder;
    //3.通过工厂类对象创建一个CharReader对象
    Json::CharReader * reader = builder.newCharReader();
    //4.调用CharReader对象的parse函数，将json字符串反序列化到Json：:Value对象中
    std:: string errs;
    bool ret = reader->parse(jsonStr.c_str(),jsonStr.c_str()+jsonStr.size(),&root,&errs);
    if(!ret)
    {
        std:: cout << "反序列化失败" << std::endl;
        return;
    }
    std:: cout << "name:" << root["name"].asString() << std::endl;
    std:: cout << "age:" << root["age"].asInt() << std::endl;
    std:: cout << "grade:" ;
    for(int i=0;i<(int)root["grade"].size();i++)
    {
        std:: cout << root["grade"][i].asString() << " ";
    }
    std:: cout << std:: endl;
    delete reader;
}

int main()
{
    Json::Value root;
    std:: string jsonStr = serializeJson(root);
    unserializeJson(jsonStr);
    return 0;
}