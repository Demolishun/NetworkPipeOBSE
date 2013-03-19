#include "NetworkPipe.h"

const OBSEInterface * g_Interface = NULL;
OBSEScriptInterface * g_scriptInterface = NULL;	// make sure you assign to this

IDebugLog		gLog("obse_network_pipe.log");

PluginHandle				g_pluginHandle = kPluginHandle_Invalid;
OBSESerializationInterface	* g_serialization = NULL;
OBSEArrayVarInterface		* g_arrayIntfc = NULL;
OBSEScriptInterface			* g_scriptIntfc = NULL;


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

            clientdata += sender_endpoint_.address().to_string().c_str();
            clientdata += ":";
            std::stringstream convert;
            convert << sender_endpoint_.port();
            clientdata +=  convert.str();

            // get sender info
            udp_packet ptmp;
            ptmp[clientdata] = recv_buf;

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

    {
        // send test
        /*
        for(std::vector<udp::endpoint>::iterator itr = clientList.begin(); itr != clientList.end(); ++itr){
            socket_.async_send_to(
			    boost::asio::buffer("heart beat", strlen("heart beat")), *itr,			    
			    boost::bind(&udp_server::handle_send_to, this,
			    boost::asio::placeholders::error,
			    boost::asio::placeholders::bytes_transferred));
        }
        */
    }

    // hack to get delayed timer working
    if(!send_timer_){
        startSendTimer();
    }
}

void udp_server::send_timer_restart(){    
    if(send_timer_){
        // restart send timer
        send_timer_->expires_from_now(boost::posix_time::milliseconds(500));
        send_timer_->async_wait(boost::bind(&udp_server::handle_send_timer, this));
    }
}

//void udp_server::handle_send_timer(const boost::system::error_code& error){
void udp_server::handle_send_timer(){
    //if (error != boost::asio::error::operation_aborted)
    {        
        // ignore restarts
    }

    for(std::vector<udp::endpoint>::iterator itr = clientList.begin(); itr != clientList.end(); ++itr){
        socket_.async_send_to(
			boost::asio::buffer("heart beat", strlen("heart beat")), *itr,			    
			boost::bind(&udp_server::handle_send_to, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
    }

    send_timer_restart(); 
}

void udp_server::startSendTimer(){
    // start send timer                
    send_timer_ = new boost::asio::deadline_timer(io_service_, boost::posix_time::milliseconds(500));
    send_timer_restart();
}