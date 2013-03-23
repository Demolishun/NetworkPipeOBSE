/*
Todo:
1. Function to launch external app for connecting to the engine.  
   Will use a Python app with examples. Instructions for wrapping with Py2exe for easy packaging.
2. Function to monitor external app to determine the health of the app/apps.
3. Come up with a messaging scheme as an example of how to use the plugin.
   Basic functions in script will be created for simple operations such as storing and retrieving of data.
   Can be used as a base for other mods, or the user can redefine how the messaging
   is done to suit thier needs.
4. Function to change the port.  
5. Error return messages and acks for bad/good data.  Most likely in examples of how to use
   as there is really nothing hard coding this right now.
6. Function to send data to a client or clients.  This will need a list of active clients that
   gets updated by the UDP thread.  Each message should have a tag showing the client which 
   produced the message.  This will allow the script code to create responses for individual
   clients as it parses the message list.
*/

#include "NetworkPipe.h"
#include "static_callbacks.h"
#include "obse/PluginAPI.h"

/*
Control plugin.  Controls conversion of data received.
*/
bool NetworkPipeEnable = false;
/* 
Control when streams are converted to data
We need to keep objects from being created that don't have a game world to be in.
*/
bool IsGameLoaded = false;
bool NewGameLoaded = false;

/*
Networking
*/
// io service for communicating to OS layer
boost::asio::io_service io_service;
boost::system::error_code ec;
// thread pointer
boost::thread* udp_thread=NULL;
// timer
//boost::asio::deadline_timer send_timer(io_service);
// port settings
char * current_address = DEFAULT_UDP_ADDRESS;
unsigned long current_port = DEFAULT_UDP_PORT;

// udp buffer queues
concurrent_queue<queue_data> udp_input_queue; // input from external processes
concurrent_queue<queue_data> udp_output_queue; // output to external processes

// process handles
std::vector<LPPROCESS_INFORMATION> processHandles;

/*
Init routine
*/
udp_server *udp_server_ptr=NULL;
static void PluginInit_PostLoadCallback()
{	
	_MESSAGE("NetworkPipe: PluginInit_PostLoadCallback called");
    
	if(!g_Interface->isEditor)
	{
		//_MESSAGE("NetworkPipe: Starting UDP");
		//udp_server_ptr = new udp_server(io_service, current_port);                 
        //udp_thread = new boost::thread(boost::bind(&udp_server::start, udp_server_ptr));         
		//_MESSAGE("NetworkPipe: UDP Started");
        //NetworkPipeEnable = true;
	}
    else
	{
		_MESSAGE("NetworkPipe: Running in editor, not starting UDP");
	}    
}

static void NetworkPipe_Exit()
{
    _MESSAGE("NetworkPipe_Exit called");

    // stop if we quit to desktop
	NetworkPipeEnable = false;
    IsGameLoaded = false;

    if(udp_server_ptr){
        udp_server_ptr->stop();
        delete udp_server_ptr;
    }
    if(udp_thread)
        delete udp_thread;                
}

static void NetworkPipe_LoadGameCallback(void * reserved)
{		
    // stop if loading a game
    NetworkPipeEnable = false;

    IsGameLoaded = false;
}

static void NetworkPipe_PostLoadGameCallback(void * reserved)
{
    IsGameLoaded = true;
}

static void NetworkPipe_ExitToMainMenu(void * reserved)
{
    // stop if we quit to desktop
	NetworkPipeEnable = false;

    IsGameLoaded = false;
}

static void NetworkPipe_IsNewGameCallback(void * reserved)
{
    _MESSAGE("NetworkPipe: New game callback");

    // stop if we create a new game
	NetworkPipeEnable = false;

    IsGameLoaded = false; // bad, do not set true

    NewGameLoaded = true;

	ResetData();
}

/**********************
* Command handlers
**********************/

#if OBLIVION

Script *inputCallbackScript = NULL; // data being received by the engine

// create the service
/*
bool Cmd_NetworkPipe_CreateService_Execute(COMMAND_ARGS)
{    
    if(!g_Interface->isEditor)
	{
        UInt32 tmpport = 0;
        if(ExtractArgsEx(paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, &tmpport)){
            current_port = tmpport;
        }

        if(udp_server_ptr){
            *result = -2;
            _MESSAGE("UDP Already Started");
            return true;
        }

		_MESSAGE("Starting UDP");
		udp_server_ptr = new udp_server(io_service, send_timer, current_port);	
        if(udp_server_ptr){
		    udp_thread = new boost::thread(boost::bind(&boost::asio::io_service::run, &io_service));        
        }
        if(!udp_server_ptr || !udp_thread){
            *result = -1;
            _MESSAGE("UDP Failed to Start");
        }else{
            *result = 0;
            _MESSAGE("UDP Started");
        }
		
	}

    return true;
}
*/

// start the service
bool Cmd_NetworkPipe_StartService_Execute(COMMAND_ARGS)
{   
    // Was this loaded already?
    bool wasStarted = NetworkPipeEnable;

    // add command line args to choose port
    UInt32 tmpport = 0;
    if(ExtractArgsEx(paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, &tmpport)){
        current_port = tmpport;
        //Console_Print("Port: %d", tmpport);
    }

    // startup server
    if(!NetworkPipeEnable){
        _MESSAGE("NetworkPipe: Starting UDP");    
	    udp_server_ptr = new udp_server(io_service, current_port);                   
        udp_thread = new boost::thread(boost::bind(&udp_server::start, udp_server_ptr));         
        _MESSAGE("NetworkPipe: UDP Started");
    }    	    
    
    if(udp_server_ptr && udp_thread){
        NetworkPipeEnable = true;   
    }else{
        _MESSAGE("NetworkPipe: Could not allocate memory for UDP objects.");
    }

    // return if the service was started previously
    *result = wasStarted;

    return true;
}
// port will still show and respond to external apps
// messages will not be sent to script functions
bool Cmd_NetworkPipe_StopService_Execute(COMMAND_ARGS)
{
    // disable service
    NetworkPipeEnable = false;

    // controlled by events
    //IsGameLoaded = false;

    // return IsGameLoaded state
    *result = IsGameLoaded;

    return true;
}
// needed for asynchronous response to sending a messages to an external service?
// maybe not, maybe the main input queue message handler can handle this
// leave for reference, not needed and would require to be called from game thread anyway
/*
bool Cmd_NetworkPipe_SetCallbackScript_Execute(COMMAND_ARGS)
{
    TESForm* funcForm = NULL;

    *result = 0.0;

    if (ExtractArgsEx(paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, &funcForm)) {
        Script* func = OBLIVION_CAST(funcForm, TESForm, Script);
	    if (func) {
            inputCallbackScript = func;
            return true;        
        }
    }
    
	Console_Print("Could not extract function script argument");
	
    return true;
}
*/
// gets data placed in the message queue by an external service
// need to get IP and PORT info from message source to pass to script
// returns a stringmap
bool Cmd_NetworkPipe_Receive_Execute(COMMAND_ARGS)
{
	if(!g_Interface->isEditor)
	{
        // grab data off the queue
        queue_data ptmp;
		if(udp_input_queue.try_try_pop(ptmp))
		{
            // do not use data until we enable it
            if(NetworkPipeEnable)
            {
                std::string temp_data = ptmp["data"];
                std::string temp_host = ptmp["host"]; 
                std::string temp_port = ptmp["port"];                                               

                // convert to unicode string
                std::wstring ws = UniConv::get_wstring(temp_data);
                
			    // turn into JSON node
                JSONNode n;
                try{
			         n = libjson::parse(ws);
                }
                catch(std::invalid_argument e){
                    std::string msg;
                    msg += "libjson::parse invalid_argument : ";
                    msg += UniConv::get_string(ws);
                    Console_Print(msg.data());
                }
                catch(std::exception e){
                    Console_Print("libjson::parse : %s",e.what());
                }
                          
                if(!n.empty()){
                    // parse into data a string map as a root container even if it is one value
                    // create empty string map
	                OBSEArray* arr = g_arrayIntfc->CreateStringMap(NULL, NULL, 0, scriptObj);	

                    try{
                        ParseJSON(n, arr, 1, scriptObj, IsGameLoaded);
                    }
                    catch(std::invalid_argument e){
                        std::string msg;
                        msg += "libjson::parse invalid_argument : ";
                        msg += UniConv::get_string(ws);
                        Console_Print(msg.data());
                    }
                    catch(std::exception e){
                        Console_Print("ParseJSON : %s",e.what());
                    }

                    // add port and address data
                    g_arrayIntfc->SetElement(arr,"host",temp_host.data());
                    g_arrayIntfc->SetElement(arr,"port",temp_port.data());                    

                    // return the new array
			        g_arrayIntfc->AssignCommandResult(arr, result);
                    return true;
                }else{
                    Console_Print("libson::parse - JSON Message was parsed, but result is empty.");
                }
            }
		}
        
	}   
    
    OBSEArray* arr = g_arrayIntfc->CreateStringMap(NULL, NULL, 0, scriptObj);
    // return the empty array
    g_arrayIntfc->AssignCommandResult(arr, result);

	return true;
}
// sends data to a client or broadcasts to all known clients
// requires IP and PORT to send to client, if none then it will broadcast
// returns a stringmap for status
bool Cmd_NetworkPipe_Send_Execute(COMMAND_ARGS){
    if(!g_Interface->isEditor){
        // do not use data until we enable it
        if(NetworkPipeEnable){
            // get data from array
            UInt32 arrID = 0;
            if(ExtractArgsEx(paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, &arrID)){
                // get array
                OBSEArray* arr = g_arrayIntfc->LookupArrayByID(arrID);
		        if (arr) {
                    JSONNode n; 
                    OBSEElement root("root");                    
                                        
                    BuildJSON(n, arr, root);

                    JSONNode::iterator itr = n.find(UniConv::get_wstring("root"));                    
                    JSONNode::iterator itr_data = n.end();
                    JSONNode::iterator itr_host = n.end();
                    JSONNode::iterator itr_port = n.end();
                    if(itr != itr->end()){ 
                        itr_data = itr->find(UniConv::get_wstring(std::string("data")));
                        if(itr_data != itr->end()){                          
                            //Console_Print("[%s],%s",UniConv::get_string(itr_data->name()).c_str(),UniConv::get_string(itr_data->as_string()).c_str());                        
                        }
                        itr_host = itr->find(UniConv::get_wstring(std::string("host")));
                        if(itr_host != itr->end()){                          
                            //Console_Print("[%s],%s",UniConv::get_string(itr_host->name()).c_str(),UniConv::get_string(itr_host->as_string()).c_str());                        
                        }
                        itr_port = itr->find(UniConv::get_wstring(std::string("port")));
                        if(itr_port != itr->end()){                          
                            //Console_Print("[%s],%s",UniConv::get_string(itr_port->name()).c_str(),UniConv::get_string(itr_port->as_string()).c_str());                        
                        }
                    }

                    /*
                    if(itr_host != itr->end() && itr_port != itr->end()){
                        Console_Print("host=%s, port=%s, data=%s",
                            UniConv::get_string(itr_host->as_string()).c_str(),
                            UniConv::get_string(itr_port->as_string()).c_str(),
                            UniConv::get_string(itr->write()).c_str());
                    }
                    */
                    
                    queue_data ptmp;                    
                    if(itr_host != itr->end() && itr_port != itr->end()){
                        // copy and remove host and port data
                        JSONNode tHost = itr_host->duplicate();
                        JSONNode tPort = itr_port->duplicate();
                        itr->erase(itr_host);
                        itr->erase(itr_port);                        

                        ptmp["host"] = UniConv::get_string(tHost.as_string());
                        ptmp["port"] = UniConv::get_string(tPort.as_string());
                        ptmp["data"] = UniConv::get_string(itr->write()); 

                        udp_output_queue.push(ptmp);
                    }                                                                                                    
			    }
            }        
        }
    }
    
    OBSEArray* arr = g_arrayIntfc->CreateStringMap(NULL, NULL, 0, scriptObj);
    // return the empty array
    g_arrayIntfc->AssignCommandResult(arr, result);

    return true;
}
// get client list
bool Cmd_NetworkPipe_GetClients_Execute(COMMAND_ARGS)
{
    return true;
}
// start external client
bool Cmd_NetworkPipe_StartClient_Execute(COMMAND_ARGS)
{
    return true;
}
// returns status of the game being a new game
// toggles off once checked
bool Cmd_NetworkPipe_IsNewGame_Execute(COMMAND_ARGS){
    *result = NewGameLoaded;

    NewGameLoaded = false;

    return true;
}

// max 0x200 (512) characters
bool Cmd_NetworkPipe_CreateClient_Execute(COMMAND_ARGS){
    STARTUPINFO startupInfo;
    LPPROCESS_INFORMATION processInfo = new PROCESS_INFORMATION;

    // clear the memory to prevent garbage
    ZeroMemory(&startupInfo, sizeof(startupInfo));

    // set size of structure (not using Ex version)
    startupInfo.cb = sizeof(STARTUPINFO);
    // tell the application that we are setting the window display 
    // information within this structure
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    // set the window display to HIDE
    //startupInfo.wShowWindow = SW_HIDE;
    startupInfo.wShowWindow = SW_SHOWNORMAL; //debug

    long retVal = -1;    

    char execStr[0x200] = { 0 };
    if(ExtractArgsEx(paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, execStr)){
        // check string not NULL and string length
        if(execStr && strlen(execStr) < 0x200){
            long tmpRet = processHandles.size();
            
            if(!CreateProcess(NULL,"python.exe",NULL,NULL,false,NORMAL_PRIORITY_CLASS,NULL,NULL,&startupInfo,processInfo)){
                _MESSAGE("Could not launch python.exe");
            }else{
                _MESSAGE("python.exe was launched");
                //TerminateProcess(processInfo->hProcess, 0);
                CloseHandle(processInfo);
                delete processInfo;
            }
        }
    }

    *result = retVal;

    return true;
}


#endif
static ParamInfo kParams_NetworkPipe_Start[2] =
{
	{ "network port", kParamType_Integer, 0 },
};

static ParamInfo kParams_NetworkPipe_Send[1] =
{
	{ "array var", kParamType_Integer, 0 },
};

static ParamInfo kParams_NetworkPipe_CreateClient[1] =
{
	{ "exec path", kParamType_String, 0 },
};

/**************************
* Command definitions
**************************/

DEFINE_COMMAND_PLUGIN(NetworkPipe_StartService, "starts the active acceptance of messages in game", 0, 1, kParams_NetworkPipe_Start);
DEFINE_COMMAND_PLUGIN(NetworkPipe_StopService, "stops the active acceptance of messages in game", 0, 0, NULL);
DEFINE_COMMAND_PLUGIN(NetworkPipe_Receive, "reads data from udp io", 0, 0, NULL);
DEFINE_COMMAND_PLUGIN(NetworkPipe_Send, "sends data to udp io", 0, 1, kParams_NetworkPipe_Send);

DEFINE_COMMAND_PLUGIN(NetworkPipe_IsNewGame, "checks status of a new game being started", 0, 0, NULL);
DEFINE_COMMAND_PLUGIN(NetworkPipe_CreateClient, "checks status of a new game being started", 0, 1, kParams_NetworkPipe_CreateClient);

/*************************
	Messaging API example
*************************/

OBSEMessagingInterface* g_msg;

void MessageHandler(OBSEMessagingInterface::Message* msg)
{
    void *nothing = NULL;
	switch (msg->type)
	{
	case OBSEMessagingInterface::kMessage_ExitGame:
		_MESSAGE("NetworkPipe received ExitGame message");
        NetworkPipe_Exit();        
		break;
	case OBSEMessagingInterface::kMessage_ExitToMainMenu:
		_MESSAGE("NetworkPipe received ExitToMainMenu message");
        IsGameLoaded = false;
		break;
	case OBSEMessagingInterface::kMessage_PostLoad:
		//_MESSAGE("Plugin Example received PostLoad mesage");
		PluginInit_PostLoadCallback();
		break;
	case OBSEMessagingInterface::kMessage_LoadGame:
        //NetworkPipeEnable = false;
        IsGameLoaded = false;
	case OBSEMessagingInterface::kMessage_SaveGame:
		_MESSAGE("NetworkPipe received save/load message with file path %s", msg->data);
		break;
	case OBSEMessagingInterface::kMessage_Precompile: 
		{
			ScriptBuffer* buffer = (ScriptBuffer*)msg->data;		
			_MESSAGE("NetworkPipe received precompile message. Script Text:\n%s", buffer->scriptText);
			break;
		}
	case OBSEMessagingInterface::kMessage_PreLoadGame:
		_MESSAGE("NetworkPipe received pre-loadgame message with file path %s", msg->data);
		break;
	case OBSEMessagingInterface::kMessage_ExitGame_Console:
		_MESSAGE("NetworkPipe received quit game from console message");
		break;
    case OBSEMessagingInterface::kMessage_PostLoadGame:
        // enable form creation        
        IsGameLoaded = true;
        break;
	default:
		_MESSAGE("NetworkPipe received unknown message");
		break;
	}
}

extern "C" {

bool OBSEPlugin_Query(const OBSEInterface * obse, PluginInfo * info)
{
	_MESSAGE("query");

	// fill out the info structure
	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = "NetworkPipe_plugin";
	info->version = 1;

	// version checks
	if(!obse->isEditor)
	{
		if(obse->obseVersion < OBSE_VERSION_INTEGER)
		{
			_ERROR("OBSE version too old (got %08X expected at least %08X)", obse->obseVersion, OBSE_VERSION_INTEGER);
			return false;
		}

#if OBLIVION
		if(obse->oblivionVersion != OBLIVION_VERSION)
		{
			_ERROR("incorrect Oblivion version (got %08X need %08X)", obse->oblivionVersion, OBLIVION_VERSION);
			return false;
		}
#endif

		g_serialization = (OBSESerializationInterface *)obse->QueryInterface(kInterface_Serialization);
		if(!g_serialization)
		{
			_ERROR("serialization interface not found");
			return false;
		}

		if(g_serialization->version < OBSESerializationInterface::kVersion)
		{
			_ERROR("incorrect serialization version found (got %08X need %08X)", g_serialization->version, OBSESerializationInterface::kVersion);
			return false;
		}

		g_arrayIntfc = (OBSEArrayVarInterface*)obse->QueryInterface(kInterface_ArrayVar);
		if (!g_arrayIntfc)
		{
			_ERROR("Array interface not found");
			return false;
		}

		g_scriptIntfc = (OBSEScriptInterface*)obse->QueryInterface(kInterface_Script);		
	}
	else
	{
		// no version checks needed for editor
	}

	// version checks pass

	return true;
}

bool OBSEPlugin_Load(const OBSEInterface * obse)
{
	_MESSAGE("load");

	g_Interface = obse;

	g_pluginHandle = obse->GetPluginHandle();

	/***************************************************************************
	 *	
	 *	READ THIS!
	 *	
	 *	Before releasing your plugin, you need to request an opcode range from
	 *	the OBSE team and set it in your first SetOpcodeBase call. If you do not
	 *	do this, your plugin will create major compatibility issues with other
	 *	plugins, and may not load in future versions of OBSE. See
	 *	obse_readme.txt for more information.
	 *	
	 **************************************************************************/

	// register commands
    // 0x2000 is for testing only
    // command range 0x2790-0x279F assigned to NetworkPipe plugin
	obse->SetOpcodeBase(0x2790);

    // NetworkPipe Commands	    
    obse->RegisterCommand(&kCommandInfo_NetworkPipe_StartService);
    obse->RegisterCommand(&kCommandInfo_NetworkPipe_StopService);
    obse->RegisterTypedCommand(&kCommandInfo_NetworkPipe_Receive,kRetnType_Array);
    obse->RegisterTypedCommand(&kCommandInfo_NetworkPipe_Send,kRetnType_Array);

    obse->RegisterCommand(&kCommandInfo_NetworkPipe_IsNewGame);
    obse->RegisterCommand(&kCommandInfo_NetworkPipe_CreateClient);

	//obse->RegisterCommand(&kPluginTestCommand);

	//obse->RegisterCommand(&kCommandInfo_ExamplePlugin_SetString);
	//obse->RegisterCommand(&kCommandInfo_ExamplePlugin_PrintString);

	// commands returning array must specify return type; type is optional for other commands
	//obse->RegisterTypedCommand(&kCommandInfo_ExamplePlugin_MakeArray, kRetnType_Array);
	//obse->RegisterTypedCommand(&kCommandInfo_ExamplePlugin_0019Additions, kRetnType_Array);

	//obse->RegisterCommand(&kCommandInfo_TestExtractArgsEx);
	//obse->RegisterCommand(&kCommandInfo_TestExtractFormatString);

	// set up serialization callbacks when running in the runtime
	if(!obse->isEditor)
	{
		// NOTE: SERIALIZATION DOES NOT WORK USING THE DEFAULT OPCODE BASE IN RELEASE BUILDS OF OBSE
		// it works in debug builds
		g_serialization->SetSaveCallback(g_pluginHandle, ExamplePlugin_SaveCallback);
		g_serialization->SetLoadCallback(g_pluginHandle, ExamplePlugin_LoadCallback);
		g_serialization->SetNewGameCallback(g_pluginHandle, NetworkPipe_IsNewGameCallback);
#if 0	// enable below to test Preload callback, don't use unless you actually need it
		g_serialization->SetPreloadCallback(g_pluginHandle, ExamplePlugin_PreloadCallback);
#endif

		// register to use string var interface
		// this allows plugin commands to support '%z' format specifier in format string arguments
		OBSEStringVarInterface* g_Str = (OBSEStringVarInterface*)obse->QueryInterface(kInterface_StringVar);
		g_Str->Register(g_Str);

		// get an OBSEScriptInterface to use for argument extraction
		g_scriptInterface = (OBSEScriptInterface*)obse->QueryInterface(kInterface_Script);
	}

	// register to receive messages from OBSE
	OBSEMessagingInterface* msgIntfc = (OBSEMessagingInterface*)obse->QueryInterface(kInterface_Messaging);
	msgIntfc->RegisterListener(g_pluginHandle, "OBSE", MessageHandler);
	g_msg = msgIntfc;

	// get command table, if needed
	OBSECommandTableInterface* cmdIntfc = (OBSECommandTableInterface*)obse->QueryInterface(kInterface_CommandTable);
	if (cmdIntfc) {
#if 0	// enable the following for loads of log output
		for (const CommandInfo* cur = cmdIntfc->Start(); cur != cmdIntfc->End(); ++cur) {
			_MESSAGE("%s",cur->longName);
		}
#endif
	}
	else {
		_MESSAGE("Couldn't read command table");
	}

	return true;
}

};
