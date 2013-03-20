#include "NetworkPipe.h"

const OBSEInterface * g_Interface = NULL;
OBSEScriptInterface * g_scriptInterface = NULL;	// make sure you assign to this

IDebugLog		gLog("obse_network_pipe.log");

PluginHandle				g_pluginHandle = kPluginHandle_Invalid;
OBSESerializationInterface	* g_serialization = NULL;
OBSEArrayVarInterface		* g_arrayIntfc = NULL;
OBSEScriptInterface			* g_scriptIntfc = NULL;

#define SEND_TIMER_PERIOD 100

void udp_server::start(){
    // startup other stuff
    startSendTimer();

    // run ioservice
    io_service_.run();
}

void udp_server::handle_send_to(const boost::system::error_code& error,
		size_t bytes_recvd)
{		
    // this was so it would be a command response
    /*
	socket_.async_receive_from(
		boost::asio::buffer(recv_buf), sender_endpoint_,
		boost::bind(&udp_server::handle_receive_from, this,
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred));
    */
}

void udp_server::handle_receive_from(const boost::system::error_code& error,
	size_t bytes_recvd)
{    
	if (!error && bytes_recvd > 0)
	{
        recv_buf.c_array()[bytes_recvd] = '\0';
        if(NetworkPipeEnable){
            std::string clientdata = "";

            // add client to list if new client
            if(std::find(clientList.begin(), clientList.end(), sender_endpoint_) == clientList.end()){
                clientList.push_back(sender_endpoint_);
            }
            
            std::stringstream convert;
            convert << sender_endpoint_.port();                      

            // get sender info
            queue_data ptmp;            
            ptmp["data"] = std::string(recv_buf.data(),bytes_recvd);
            ptmp["host"] = sender_endpoint_.address().to_string();
            ptmp["port"] = convert.str();

            // don't fill up the buffer if not receiving messages
			udp_input_queue.push(ptmp);
        }                    
	}
    // always reset receive trigger
    {		
		// restart check for new data
		socket_.async_receive_from(
			boost::asio::buffer(recv_buf), sender_endpoint_,
			boost::bind(&udp_server::handle_receive_from, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));		
	}    
}

void udp_server::send_timer_restart(){    
    if(send_timer_){
        // restart send timer
        send_timer_->expires_from_now(boost::posix_time::milliseconds(SEND_TIMER_PERIOD));
        send_timer_->async_wait(boost::bind(&udp_server::handle_send_timer, this));
    }
}

void udp_server::handle_send_timer(){    
    // cycle through clients and send message   
    /*
    for(std::vector<udp::endpoint>::iterator itr = clientList.begin(); itr != clientList.end(); ++itr){
        socket_.async_send_to(
			boost::asio::buffer("heart beat", strlen("heart beat")), *itr,			    
			boost::bind(&udp_server::handle_send_to, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
    } 
    */

    // get messages from queue
    queue_data ptmp;
    if(udp_output_queue.try_try_pop(ptmp)){
        std::string temp_data = ptmp["data"];
        std::string temp_host = ptmp["host"]; 
        std::string temp_port = ptmp["port"];     
                        
        std::stringstream convert;
        convert << temp_port;
        unsigned short port;        
        convert >> port;

        Console_Print("host:%s, port:%s, data:%s, portN:%d, len:%d",
            temp_host.c_str(),
            temp_port.c_str(),
            temp_data.c_str(),
            port, temp_data.size());

        udp_buffer temp_buffer;  
        // getting this warning even though I know this is safe enough
        #pragma warning( push )
        #pragma warning( disable : 4996 )
        temp_data.copy(temp_buffer.c_array(), temp_data.size(), 0);
        #pragma warning( pop )    
        
        socket_.async_send_to(
			boost::asio::buffer(temp_buffer, temp_data.size()), 
            udp::endpoint(boost::asio::ip::address_v4::from_string(temp_host), port),			    
			boost::bind(&udp_server::handle_send_to, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));                        
    }

    // restart the timer
    send_timer_restart(); 
}

void udp_server::startSendTimer(){
    // create send timer 
    if(!send_timer_)
        send_timer_ = new boost::asio::deadline_timer(io_service_, boost::posix_time::milliseconds(SEND_TIMER_PERIOD));
    // startup timer
    send_timer_restart();
}