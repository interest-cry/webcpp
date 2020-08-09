#include <stdio.h>
#include <unistd.h>

#include <iostream>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#define NUM 320
typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

// pull out the type of messages sent by our config
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

bool bIsConnectedServer = false;
int closeConnect();
client::connection_ptr conn;
int end = 0;
// This message handler will be invoked once for each incoming message. It
// prints the message and then sends a copy of the message back to the
// server.
void on_message(websocketpp::connection_hdl hdl, message_ptr msg) {
    std::cout << "hdl.lock():" << hdl.lock().get() << std::endl;
    //             << " and message: " << msg->get_payload() << std::endl;
    // msg->get_payload();
    printf("%s\n", msg->get_payload().c_str());
}
//连接上服务器，回调此函数
void on_open(client* c, websocketpp::connection_hdl hdl) {
    std::cout << "open handler.............." << std::endl;
    client::connection_ptr con = c->get_con_from_hdl(hdl);
    // websocketpp::config::core_client::request_type requestClient =
    // con->get_request();
    if (con->get_ec().value() != 0) {
        bIsConnectedServer = false;
        printf("on open error....\n");
    } else {
        bIsConnectedServer = true;
        printf("on open ok....\n");
    }
}

//服务器远程断开连接，回调此函数
void on_close(client* c, websocketpp::connection_hdl hdl) {
    bIsConnectedServer = false;
    printf("on_close....\n");
    end = 1;
    closeConnect();
}

//无法连接上服务器时，回调此函数，socket在内部已关闭上层无需再关闭
void on_termination_handler(client* c, websocketpp::connection_hdl hdl) {
    bIsConnectedServer = false;
    end = 1;
    printf("on_termination_handler....\n");
    closeConnect();
}
struct Param {
    char uri[60];
    char audio[60];
};

int parse_param(struct Param* param, int argc, char** argv) {
    if (argc < 3) {
        printf("./a.out -url=ws://ip:port/ws/asr/pcm -audio=qq.pcm\n");
        return -1;
    }

    char* index = NULL;
    for (int i = 1; i < argc; i++) {
        printf("index:%d,%s\n", i, *(argv + i));
        switch (i) {
            case 1:
                index = strstr(argv[i], "ws");
                if (index == NULL) {
                    return -1;
                }
                // printf("===index:%ld\n", index - argv[i]);
                strcpy(param->uri, index);
                break;

            case 2:
                index = strstr(argv[i], "=");
                if (index == NULL) {
                    return -1;
                }
                // printf("===index:%ld\n", index + 1 - argv[i]);
                strcpy(param->audio, index + 1);
                break;
        }
    }
    return 0;
}
int main(int argc, char* argv[]) {
    printf("./a.out -url=ws://ip:port/ws/asr/pcm -audio=qq.pcm\n");
    // Create a client endpoint
    client c;
    FILE* fd;
    end = 0;
    struct Param param;
    int fg = parse_param(&param, argc, argv);
    if (fg) {
        printf("param errror\n");
        return -1;
    }
    printf("url:%s, audio:%s\n", param.uri, param.audio);
    std::string uri(param.uri);  //"ws://192.168.5.25:8101/ws/asr/pcm";
    printf("uri:%s\n", uri.c_str());
    if (argc < 2) {
        return -1;
    }
    try {
        // Set logging to be pretty verbose (everything except message payloads)
        c.set_access_channels(websocketpp::log::alevel::all);
        c.clear_access_channels(websocketpp::log::alevel::frame_payload);

        // Initialize ASIO
        c.init_asio();
        c.set_reuse_addr(true);

        // Register our message handler
        c.set_message_handler(bind(&on_message, ::_1, ::_2));
        c.set_open_handler(bind(&on_open, &c, ::_1));
        c.set_close_handler(bind(&on_close, &c, ::_1));

        websocketpp::lib::error_code ec;
        conn = c.get_connection(uri, ec);
        if (ec) {
            std::cout << "could not create connection because: " << ec.message()
                      << std::endl;
            return -1;
        }
        // set http header
        conn->append_header("rate", "16000");
        conn->append_header("appkey",
                            "xuqo7pqagqx5gvdbqyfybrusfosbbkjjtfvsr5qx");
        conn->append_header("closeVad", "true");
        conn->append_header("session-id",
                            "fe945929-dfa4-4960-8e31-f8c057ed2b99");
        conn->append_header("eof", "eof");
        // conn->append_header("vocab-enhance", "5");
        // conn->append_header("group-id", "5e58e7ec39de65172c496471");
        // conn->append_header("vocab-id", "5e707b7639de65172c496476");
        // conn->append_header();
        // Note that connect here only requests a connection. No network
        // messages are exchanged until the event loop starts running in the
        // next line.
        c.connect(conn);
        //设置连接断开通知handler
        conn->set_termination_handler(bind(on_termination_handler, &c, ::_1));
        // Start the ASIO io_service run loop
        // this will cause a single connection to be made to the server. c.run()
        // will exit when this connection is closed.
        // c.run();
        //线程模式中使用run_one，模拟线程代码如下：
        int n;
        char pdata[NUM] = {0};
        int eof = 0;
        // open audio file
        fd = fopen(param.audio, "rb");
        if (fd == NULL) {
            printf("===fopen error\n");
            return -1;
        }
        while (true) {
            if (bIsConnectedServer) {
                if (eof == 0 && !feof(fd)) {
                    n = fread(pdata, 1, NUM, fd);
                    if (n == 0) {
                        continue;
                    }
                    try {
                        ec = conn->send(pdata, n,
                                        websocketpp::frame::opcode::binary);
                        if (ec) {
                            std::cout << "Echo failed because: " << ec.message()
                                      << std::endl;
                            break;
                        }
                    } catch (websocketpp::exception const& e) {
                        std::cout << e.what() << std::endl;
                        break;
                    }

                } else {
                    eof = eof + 1;
                    if (eof == 1) {
                        try {
                            ec = conn->send("eof");
                            if (ec) {
                                std::cout << "...Echo failed because: "
                                          << ec.message() << std::endl;
                                break;
                            }
                        } catch (websocketpp::exception const& e) {
                            std::cout << e.what() << std::endl;
                            break;
                        }
                    }
                    printf("eof:%d.............\n", eof);
                }
            }
            // after sever close,exit while.
            if (end) {
                printf("end end.........\n");
                break;
            }
            // printf("before run_one...\n");
            c.run_one();
            // printf("after run_one...\n");
            usleep(1000 * 10);
        }

    } catch (websocketpp::exception const& e) {
        std::cout << e.what() << std::endl;
    }
    //   sleep(1);
    fclose(fd);
    printf("stop........file close...\n");
    c.stop();
}
//主动关闭连接
int closeConnect() {
    int nRet = 0;
    try {
        if (conn != NULL &&
            conn->get_state() == websocketpp::session::state::open) {
            printf("============== closeConnect\n");
            websocketpp::close::status::value cvValue = 0;
            std::string strReason = "";
            conn->close(cvValue, strReason);
        } else {
            printf("==============no closeConnect\n");
        }

    } catch (websocketpp::exception const& e) {
        nRet = -1;
    }
    return nRet;
}

/********************************
int sendTextData(char* pText) {
  int nRet = 0;
  try {
    websocketpp::lib::error_code ec;
    ec = conn->send(pText);
    if (ec) {
      std::cout << "Echo failed because: " << ec.message() << std::endl;
      nRet = -1;
    }
  } catch (websocketpp::exception const& e) {
    std::cout << e.what() << std::endl;
    return -2;
  }

  return nRet;
}

int sendBinaryData(unsigned char* pData, int nSize) {
  int nRet = 0;
  try {
    websocketpp::lib::error_code ec;
    ec = conn->send(pData, nSize, websocketpp::frame::opcode::binary);
    if (ec) {
      std::cout << "Echo failed because: " << ec.message() << std::endl;
      nRet = -1;
    }
  } catch (websocketpp::exception const& e) {
    std::cout << e.what() << std::endl;
    return -2;
  }

  return nRet;
}
*****************************************/