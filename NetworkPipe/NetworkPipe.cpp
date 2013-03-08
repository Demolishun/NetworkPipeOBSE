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
            
        // temporary code to test packets
        /*
		socket_.async_send_to(
			boost::asio::buffer(recv_buf.c_array(), bytes_recvd), sender_endpoint_,
			//boost::asio::buffer(recv_buf), sender_endpoint_,
			boost::bind(&udp_server::handle_send_to, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));        
		*/
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