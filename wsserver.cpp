#include <websocketpp/config/asio_no_tls.hpp>
#include <iostream>
#include <string>
#include <websocketpp/server.hpp>


typedef websocketpp:: server<websocketpp:: config:: asio> wsserver_t; //这里必须用asio，而不是asio_no_tls，命名问题。

void http_callback(wsserver_t* srv, websocketpp:: connection_hdl hdl) {
    wsserver_t:: connection_ptr conn = srv->get_con_from_hdl(hdl);
    std::cout<< "body:" << conn->get_request_body() << std::endl;
    websocketpp::http::parser::request rep = conn->get_request();
    std::cout<< "uri:" << rep.get_uri() << std::endl;
    std::cout<< "method:" << rep.get_method() << std::endl;

    std:: string body = "<html><body><h1>Hello, World!</h1></body></html>";
    conn->set_body(body);
    conn->set_status(websocketpp::http::status_code::ok);
    conn->append_header("Content-Type","text/html");
}

void open_callback(wsserver_t* srv,websocketpp:: connection_hdl hdl) {
    std:: cout <<"握手成功，连接已打开" << std::endl;
}

void close_callback(wsserver_t* srv,websocketpp:: connection_hdl hdl) {
    std:: cout <<"连接已关闭" << std::endl;
}

void message_callback(wsserver_t* srv,websocketpp:: connection_hdl hdl, wsserver_t::message_ptr msg) {
    wsserver_t:: connection_ptr conn = srv->get_con_from_hdl(hdl);
    std:: cout <<"收到消息:" << msg->get_payload() << std::endl;
    std:: string response = "服务器已收到消息: " + msg->get_payload();
    conn->send(response,websocketpp::frame::opcode::text);
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