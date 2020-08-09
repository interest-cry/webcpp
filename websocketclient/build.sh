#git clone https://github.com/zaphoyd/websocketpp.git
g++ -g -o ppcli4 asrcli.cpp -I./ -I/root/libpath/boost/include -L /root/libpath/boost/lib \
-lboost_system -lboost_chrono -lboost_random -lpthread
