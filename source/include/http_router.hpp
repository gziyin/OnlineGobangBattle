#pragma once

#include <string>

#include <json/json.h>

#include "connection_manager.hpp"
#include "online.hpp"
#include "user_table.hpp"
#include "auth_handler.hpp"
#include "util.hpp"

namespace gobang {

class HttpRouter {
public:
    void init(UserTable* user_table, OnlineManager* online_mgr) {
        _user_table = user_table;
        _online_mgr = online_mgr;
    }

    void handle_request(WebsocketServer& server, WebsocketConnectionHdl hdl) {
        auto con = server.get_con_from_hdl(hdl);
        std::string method = con->get_request().get_method();
        std::string resource = trim_path(con->get_resource());

        if (method == "OPTIONS") {
            con->append_header("Access-Control-Allow-Origin", "*");
            con->append_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
            con->append_header("Access-Control-Allow-Headers",
                               "Content-Type, Authorization");
            con->set_status(websocketpp::http::status_code::ok);
            return;
        }

        std::string body = con->get_request_body();
        Json::Value req_json;
        Json::Value resp_json;
        std::string errmsg;

        if (method == "POST" && resource == "/api/v1/auth/register") {
            if (!util::str_to_json(body, req_json, errmsg)) {
                resp_json["success"] = false;
                resp_json["message"] = "Invalid JSON: " + errmsg;
            } else {
                resp_json = auth::handle_register(
                    *_user_table,
                    req_json["username"].asString(),
                    req_json["password"].asString());
            }
        } else if (method == "POST" && resource == "/api/v1/auth/login") {
            if (!util::str_to_json(body, req_json, errmsg)) {
                resp_json["success"] = false;
                resp_json["message"] = "Invalid JSON: " + errmsg;
            } else {
                resp_json = auth::handle_login(
                    *_user_table,
                    *_online_mgr,
                    req_json["username"].asString(),
                    req_json["password"].asString());
            }
        } else {
            con->append_header("Access-Control-Allow-Origin", "*");
            con->set_status(websocketpp::http::status_code::not_found);
            con->set_body("{\"success\":false,\"message\":\"Not Found\"}");
            return;
        }

        con->append_header("Access-Control-Allow-Origin", "*");
        con->append_header("Content-Type", "application/json; charset=utf-8");
        con->set_status(websocketpp::http::status_code::ok);
        con->set_body(util::json_to_str(resp_json));
    }

private:
    static std::string trim_path(const std::string& resource) {
        size_t end = resource.find_first_of("?#");
        return resource.substr(0, end);
    }

    UserTable* _user_table = nullptr;
    OnlineManager* _online_mgr = nullptr;
};

} // namespace gobang
