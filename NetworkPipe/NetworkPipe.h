#include "obse/PluginAPI.h"
#include "obse/CommandTable.h"

#if OBLIVION
#include "obse/GameAPI.h"
#define ENABLE_EXTRACT_ARGS_MACROS 1	// #define this as 0 if you prefer not to use this
#else
#include "obse_editor/EditorAPI.h"
#endif

#include "obse/ParamInfos.h"
#include "obse/Script.h"
#include "obse/GameObjects.h"

typedef OBSEArrayVarInterface::Array	OBSEArray;
typedef OBSEArrayVarInterface::Element	OBSEElement;

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/thread/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <vector>

#include <string>
#include <memory>

#include "concurrent_queue.h"

#include "../json/libjson.h"
#include "parse_json.h"

#ifndef _NETWORKPIPE_H
#define _NETWORKPIPE_H

// set buffers to max data that can be transferred using IPV4 UDP
#define DEFAULT_UDP_ADDRESS "127.0.0.1"
#define DEFAULT_UDP_PORT 12345
// add 256 characters for nulls and client address and port
#define MAX_UDP_PACKET_SIZE (65507+256)

// udp buffer type
typedef boost::array<char, MAX_UDP_PACKET_SIZE> udp_buffer;
typedef std::map<std::string,udp_buffer> udp_packet;

#if ENABLE_EXTRACT_ARGS_MACROS

extern const OBSEInterface * g_Interface;
extern OBSEScriptInterface * g_scriptInterface;	// make sure you assign to this
#define ExtractArgsEx(...) g_scriptInterface->ExtractArgsEx(__VA_ARGS__)
#define ExtractFormatStringArgs(...) g_scriptInterface->ExtractFormatStringArgs(__VA_ARGS__)

#endif

extern IDebugLog		gLog;

extern PluginHandle				g_pluginHandle;
extern OBSESerializationInterface	* g_serialization;
extern OBSEArrayVarInterface		* g_arrayIntfc;
extern OBSEScriptInterface			* g_scriptIntfc;

// helper function for creating an OBSE Map from a std::map<double, OBSEElement>
OBSEArray* MapFromStdMap(const std::map<double, OBSEElement>& data, Script* callingScript);
// helper function for creating OBSE Array from std::vector<OBSEElement>
OBSEArray* ArrayFromStdVector(const std::vector<OBSEElement>& data, Script* callingScript);
// helper function for creating an OBSE StringMap from a std::map<std::string, OBSEElement>
OBSEArray* StringMapFromStdMap(const std::map<std::string, OBSEElement>& data, Script* callingScript);

extern bool NetworkPipeEnable;
extern char * current_address;
extern unsigned long current_port;

// udp buffer queues
extern concurrent_queue<udp_packet> udp_input_queue; // input from external processes
extern concurrent_queue<udp_packet> udp_output_queue; // output to external processes

using boost::asio::ip::udp;
class udp_server
{
public:
	udp_server(boost::asio::io_service& io_service, short port)
		: io_service_(io_service),
		  socket_(io_service_, udp::endpoint(boost::asio::ip::address_v4::from_string(current_address), port))//, // udp::v4()  
	{
        // start udp receive
		socket_.async_receive_from(
			boost::asio::buffer(recv_buf), sender_endpoint_,
			boost::bind(&udp_server::handle_receive_from, this,
			  boost::asio::placeholders::error,
			  boost::asio::placeholders::bytes_transferred));  

        send_timer_ = NULL;            
	}

	~udp_server(){
		io_service_.stop();
        if(send_timer_){
            send_timer_->cancel();
            delete send_timer_;
        }
	}

	void start(){        
	}

    void startSendTimer();

	void handle_send_to(const boost::system::error_code& error, size_t bytes_recvd);
	void handle_receive_from(const boost::system::error_code& error, size_t bytes_recvd);

    //void handle_send_timer(const boost::system::error_code& error);
    void handle_send_timer();
    void send_timer_restart();

	void stop()
	{
		io_service_.stop();
	}

	private:
		boost::asio::io_service& io_service_;
		udp::socket socket_;
		udp::endpoint sender_endpoint_;	
        std::vector<udp::endpoint> clientList;
		//std::auto_ptr<boost::asio::io_service::work> busy_work;
		udp_buffer recv_buf;
        boost::asio::deadline_timer* send_timer_;
};


// example commands
#if OBLIVION
bool Cmd_TestExtractArgsEx_Execute(COMMAND_ARGS);
bool Cmd_TestExtractFormatString_Execute(COMMAND_ARGS);
bool Cmd_ExamplePlugin_0019Additions_Execute(COMMAND_ARGS);
bool Cmd_ExamplePlugin_MakeArray_Execute(COMMAND_ARGS);
bool Cmd_PluginTest_Execute(COMMAND_ARGS);
bool Cmd_ExamplePlugin_PrintString_Execute(COMMAND_ARGS);
bool Cmd_ExamplePlugin_SetString_Execute(COMMAND_ARGS);
#endif

#endif //_NETWORKPIPE_H