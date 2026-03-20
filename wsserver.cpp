#include <iostream>
#include <string>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>


typedef websocketpp:: server<websocketpp:: config:: asio_no_tls> wsserver_t;

void http_callback(wsserver_t* srv, websocketpp:: connection_hdl hdl) {

}

void open_callback(wsserver_t* srv,websocketpp:: coonnection_hdl hdl) {
    
}

void close_callback(wsserver_t* srv,websocketpp:: coonnection_hdl hdl) {
    
}

void message_callback(wsserver_t* srv,websocketpp:: coonnection_hdl hdl, wsserver_t::message_ptr msg) {
    
}

int main()
{
    //1.实例化server对象
    wsserver_t wssrv;
    //2.设置日志等级
    wssrv.set_access_channels(websocketpp::log::alevel::none);
    //3.初始化asio调度器
    wssrv.init_asio();
    wssrv.set_reuse_addr(true);
    //4.设置回调函数
    wssrv.set_http_handler(std::bind(http_callback,&wssrv,std::placeholders::_1));
    wssrv.set_open_handler(std::bind(open_callback,&wssrv,std::placeholders::_1));
    wssrv.set_close_handler(std::bind(close_callback,&wssrv,std::placeholders::_1));
    wssrv.set_message_handler(std::bind(message_callback,&wssrv,std::placeholders::_1,std::placeholders::_2));
    //5.设置监听端口
    wssrv.listen(8085);
    //6.开始获取新连接
    wssrv.start_accept();
    //7.启动服务器
    wssrv.run();

    return 0;
}