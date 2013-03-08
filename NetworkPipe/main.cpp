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
boost::thread* udp_thread;
// port settings
char * current_address = DEFAULT_UDP_ADDRESS;
unsigned long current_port = DEFAULT_UDP_PORT;

// udp buffer queues
concurrent_queue<udp_packet> udp_input_queue; // input from external processes
concurrent_queue<udp_packet> udp_output_queue; // output to external processes

/*
Init routine
*/
udp_server *udp_server_ptr=NULL;
static void PluginInit_PostLoadCallback()
{	
	_MESSAGE("NetworkPipe: PluginInit_PostLoadCallback called");
    
	if(!g_Interface->isEditor)
	{
		_MESSAGE("NetworkPipe: Starting UDP");
		udp_server_ptr = new udp_server(io_service, current_port);	
		udp_thread = new boost::thread(boost::bind(&boost::asio::io_service::run, &io_service));        
		_MESSAGE("NetworkPipe: UDP Started");
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

    udp_server_ptr->stop();
    delete udp_thread;
    delete udp_server_ptr;
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
		udp_server_ptr = new udp_server(io_service, current_port);	
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

// start the service
bool Cmd_NetworkPipe_StartService_Execute(COMMAND_ARGS)
{    
    // now enable as a script has started the service
    NetworkPipeEnable = true;
    // let the PostLoadGame event handle this
    //if(!NewGameLoaded)
    //    IsGameLoaded = true;

    // return IsGameLoaded state
    *result = IsGameLoaded;

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
        //if (ExtractArgsEx(paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList)){
        //}

		udp_packet temp_packet;
		if(udp_input_queue.try_try_pop(temp_packet))
		{
            // do not use data until we enable it
            if(NetworkPipeEnable)
            {
                std::string temp_source = temp_packet.begin()->first;
                udp_buffer temp_buffer = temp_packet.begin()->second;                 

                // may need to setlocale
                wchar_t *wbuf = new wchar_t[ temp_buffer.size() ];
                size_t num_chars = mbstowcs( wbuf, temp_buffer.c_array(), temp_buffer.size() );
                std::wstring ws( wbuf, num_chars );
                delete wbuf;                                        

                Console_Print(UniConv::get_string(ws).c_str());

			    //Console_Print("NetworkPipe message: int: %d str: %s", strlen(temp_buffer.c_array()), temp_buffer.c_array());
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
                    g_arrayIntfc->SetElement(arr,"address:port",temp_source.data());

                    // return the new array
			        g_arrayIntfc->AssignCommandResult(arr, result);
                    return true;
                }else{
                    Console_Print("libson::parse - JSON Message was parsed, but result is empty.");
                }
            }
		}
        OBSEArray* arr = g_arrayIntfc->CreateStringMap(NULL, NULL, 0, scriptObj);
        // return the empty array
        g_arrayIntfc->AssignCommandResult(arr, result);
	}    

	return true;
}
// returns status of the game being a new game
// toggles off once checked
bool Cmd_NetworkPipe_IsNewGame_Execute(COMMAND_ARGS){
    *result = NewGameLoaded;

    NewGameLoaded = false;

    return true;
}


#endif

/**************************
* Command definitions
**************************/

DEFINE_COMMAND_PLUGIN(NetworkPipe_Receive, "runs poll on the udp io", 0, 0, NULL);
//DEFINE_COMMAND_PLUGIN(NetworkPipe_CreateService, "starts the active acceptance of messages in game", 0, 1, NULL);
DEFINE_COMMAND_PLUGIN(NetworkPipe_StartService, "starts the active acceptance of messages in game", 0, 0, NULL);
DEFINE_COMMAND_PLUGIN(NetworkPipe_StopService, "stops the active acceptance of messages in game", 0, 0, NULL);
DEFINE_COMMAND_PLUGIN(NetworkPipe_IsNewGame, "checks status of a new game being started", 0, 0, NULL);


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
	obse->RegisterTypedCommand(&kCommandInfo_NetworkPipe_Receive,kRetnType_Array);
    //obse->RegisterCommand(&kCommandInfo_NetworkPipe_CreateService);
    obse->RegisterCommand(&kCommandInfo_NetworkPipe_StartService);
    obse->RegisterCommand(&kCommandInfo_NetworkPipe_StopService);
    obse->RegisterCommand(&kCommandInfo_NetworkPipe_IsNewGame);

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
