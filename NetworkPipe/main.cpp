/*
Todo:
1. Function to launch external app for connecting to the engine.  Done!
2. Will use a Python app with examples. Instructions for wrapping with Py2exe for easy packaging.  
3. Come up with a messaging scheme as an example of how to use the plugin. 
   Basic functions in script will be created for simple operations such as storing and retrieving of data.
   Can be used as a base for other mods, or the user can redefine how the messaging
   is done to suit thier needs.  Do through example mods.
4. Function to change the port.  Done!
5. Error return messages and acks for bad/good data.  Most likely in examples of how to use
   as there is really nothing hard coding this right now.  Do through example mods.
6. Function to send data to a client or clients.  This will need a list of active clients that
   gets updated by the UDP thread.  Each message should have a tag showing the client which 
   produced the message.  This will allow the script code to create responses for individual
   clients as it parses the message list.  Done!
*/

#include "NetworkPipe.h"
//#include "static_callbacks.h"
#include "obse/PluginAPI.h"
//#include "obse/ScriptUtils.h"  // errors when included
//ExpressionEvaluator eval(PASS_COMMAND_ARGS);

// command range 0x2790-0x279F assigned to NetworkPipe plugin
#define NP_OPCODEBASE 0x2790

#define SAFE_DELETE(ptr) delete ptr; ptr = NULL;

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
// server pointer
udp_server *udp_server_ptr=NULL;
// port settings
char * current_address = DEFAULT_UDP_ADDRESS;
unsigned long current_port = DEFAULT_UDP_PORT;

// udp buffer queues
concurrent_queue<queue_data> udp_input_queue; // input from external processes
concurrent_queue<queue_data> udp_output_queue; // output to external processes

// process handles
//std::vector<LPPROCESS_INFORMATION> processHandles;
std::map<DWORD,LPPROCESS_INFORMATION> processHandles;

// standard stop service call
void stopService()
{     
    // stop server and thread
    if(udp_server_ptr){
        udp_server_ptr->stop();
        SAFE_DELETE(udp_server_ptr);        
    }
    if(udp_thread)
        SAFE_DELETE(udp_thread);
}
// standard kill process call
void stopProcess(DWORD pid)
{
    STARTUPINFO startupInfo;
    LPPROCESS_INFORMATION processInfo = new PROCESS_INFORMATION;

    // clear the memory to prevent garbage
    ZeroMemory(&startupInfo, sizeof(startupInfo));

    // set size of structure (not using Ex version)
    startupInfo.cb = sizeof(STARTUPINFO);
    // tell the application that we are setting the window display 
    // information within this structure
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;    
    // hide process
    startupInfo.wShowWindow = SW_HIDE;

    //TerminateProcess(itr->second->hProcess, 0);  // not friendly to process, and does not kill child processes
    std::stringstream comStream;       
    comStream << "taskkill /pid ";
    comStream << pid;
    //comStream << " /t /f";  // to be more like TerminateProcess
    _MESSAGE("%s", comStream.str().c_str());             
    //system(comStream.str().c_str()); // works, but pops up a window momentarilly when called        
    
    //LPSTR s = const_cast<char *>(comStream.str().c_str());  
    LPSTR cString = _strdup( comStream.str().c_str() );
    if(!CreateProcess(NULL,cString,NULL,NULL,false,NORMAL_PRIORITY_CLASS,NULL,NULL,&startupInfo,processInfo)){
        _MESSAGE("Could not launch '%s'",cString);
        SAFE_DELETE(processInfo);
    }else{
        // clean up
        CloseHandle(processInfo);
        SAFE_DELETE(processInfo);
    }
    // clean up 
    free(cString);

    // call pskill too
    comStream.str("");
    comStream.clear();  // clears any eof states

    comStream << "pskill ";
    comStream << pid;
    _MESSAGE("%s", comStream.str().c_str());

    cString = _strdup( comStream.str().c_str() );
    if(!CreateProcess(NULL,cString,NULL,NULL,false,NORMAL_PRIORITY_CLASS,NULL,NULL,&startupInfo,processInfo)){
        _MESSAGE("Could not launch '%s'",cString);
        SAFE_DELETE(processInfo);
    }else{
        // clean up
        CloseHandle(processInfo);
        SAFE_DELETE(processInfo);
    }
    // clean up 
    free(cString);
}

/*
Init routine
*/
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

    // stop external processes        
    for(std::map<DWORD,LPPROCESS_INFORMATION>::iterator itr=processHandles.begin(); itr!=processHandles.end(); ++itr){    
        //TerminateProcess(itr->second->hProcess, 0);  // not friendly to process, and does not kill child processes
        //std::stringstream comStream;       
        //comStream << "taskkill /pid ";
        //comStream << itr->second->dwProcessId;
        //comStream << " /t /f";  // to be more like TerminateProcess
        //_MESSAGE("%s", comStream.str().c_str());             
        //system(comStream.str().c_str());
        stopProcess(itr->second->dwProcessId);

        CloseHandle(itr->second);
        delete (itr->second);
    }    

    // stop server and thread
    stopService();            
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

    // resest data as needed
}

static void NetworkPipe_SaveCallback(void * reserved)
{
    return;
}

static void NetworkPipe_LoadCallback(void * reserved)
{
    return;
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
   
    // stop server and thread
    stopService();           

    // controlled by events
    //IsGameLoaded = false;

    // bogus return
    *result = 0;

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

// returns status of the game being a new game
// toggles off once checked
bool Cmd_NetworkPipe_IsNewGame_Execute(COMMAND_ARGS){
    *result = NewGameLoaded;

    NewGameLoaded = false;

    return true;
}

// creates a client
// max 0x200 (512) characters for command path including the parameters
// returns index of client or 0 to indicate client failed to start
#define NP_MAX_COMMAND_SIZE 0x200
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

    unsigned long retVal = 0;    

    char execStr[NP_MAX_COMMAND_SIZE] = { 0 };
    int showFlag = -1;
    if(ExtractArgsEx(paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, execStr, &showFlag)){
        // check string not NULL and string length
        if(execStr && strlen(execStr) < NP_MAX_COMMAND_SIZE){                       
            // set the window display mode of external app
            switch(showFlag){
            case 0:
                startupInfo.wShowWindow = SW_HIDE;
                break;
            case 1:
                startupInfo.wShowWindow = SW_SHOWNORMAL;
                break;
            case 2:
                startupInfo.wShowWindow = SW_SHOWMINIMIZED;
                break;
            default:
                startupInfo.wShowWindow = SW_HIDE;
            }            

            if(!CreateProcess(NULL,execStr,NULL,NULL,false,NORMAL_PRIORITY_CLASS,NULL,NULL,&startupInfo,processInfo)){
                _MESSAGE("Could not launch '%s'",execStr);
                SAFE_DELETE(processInfo);
            }else{
                _MESSAGE("'%s' was launched with show mode: %d",execStr,startupInfo.wShowWindow);
                // store process handle location for new process
                long tmpRet = processHandles.size();  
                // store process handle
                processHandles[processInfo->dwProcessId] = processInfo;

                _MESSAGE("Process: '%s', PID: %d",execStr, processInfo->dwProcessId);

                // return process index
                retVal = processInfo->dwProcessId;                           
            }
        }
    }    

    // returns the process index in the processHandles structure
    *result = retVal;

    return true;
}
// kills a client started by CreateClient
// parameter is the index into the process list
bool Cmd_NetworkPipe_KillClient_Execute(COMMAND_ARGS){
    unsigned long pid = 0;
    if(ExtractArgsEx(paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, &pid)){
        _MESSAGE("delete PID: %d", pid);

        // make sure the pid is in the list
        if(processHandles.count(pid)){
            _MESSAGE("Found PID", pid);

            // grab pointer to process info
            LPPROCESS_INFORMATION processInfo = processHandles[pid];
            processHandles.erase(pid);                

            // kill the process, close the handle, and delete the memory holding the process info            
            //TerminateProcess(processInfo->hProcess, 0);  // not friendly to process, and does not kill child processes            
            //std::stringstream comStream;       
            //comStream << "taskkill /pid ";
            //comStream << processInfo->dwProcessId;
            //comStream << " /t /f";  // to be more like TerminateProcess
            //_MESSAGE("%s", comStream.str().c_str());             
            //system(comStream.str().c_str());

            stopProcess(processInfo->dwProcessId);

            CloseHandle(processInfo);
            SAFE_DELETE(processInfo);
        }
    }   

    return true;
}

// data for get/set data
std::map<std::string, OBSEElement> persistentData;

// get and set persistent data
// overcomes limitations of storing data is quest variables
// designed to store data that will survive game loads
// takes a map of data 
bool Cmd_NetworkPipe_SetData_Execute(COMMAND_ARGS)
{
    *result = 0;

    UInt32 arrID = 0;
    if(ExtractArgsEx(paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, &arrID)){
        OBSEArray* arr = g_arrayIntfc->LookupArrayByID(arrID);
	    if (arr) {
            // get contents of array
			UInt32 size = g_arrayIntfc->GetArraySize(arr);
			if (size != -1) {
                // get the elems and keys
			    OBSEElement* elems = new OBSEElement[size];
			    OBSEElement* keys = new OBSEElement[size];

                // cycle through and assign data to map
                if (g_arrayIntfc->GetElements(arr, elems, keys)) {
                    for (UInt32 i = 0; i < size; i++) {
                        if(keys[i].String() != NULL)
                            persistentData[keys[i].String()] = elems[i];					   
				    }

                    *result = 1;  // result success
                }                

                // delete data when done
                delete[] elems;
			    delete[] keys;
            }
        }
    }

    return true;
}
// takes an array of strings
// returns a map of data
bool Cmd_NetworkPipe_GetData_Execute(COMMAND_ARGS)
{
    // create output array
    OBSEArray* newArr = g_arrayIntfc->CreateStringMap(NULL, NULL, 0, scriptObj);
    // set empty array as output, empty if failed to lookup any values
    g_arrayIntfc->AssignCommandResult(newArr, result);    

    UInt32 arrID = 0;
    if(ExtractArgsEx(paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, &arrID)){
        OBSEArray* arr = g_arrayIntfc->LookupArrayByID(arrID);
        if (arr) {
            // get contents of array
			UInt32 size = g_arrayIntfc->GetArraySize(arr);
			if (size != -1) {
                // get the elems and keys
			    OBSEElement* elems = new OBSEElement[size];
			    OBSEElement* keys = new OBSEElement[size];                

                // cycle through and assign data to map
                if (g_arrayIntfc->GetElements(arr, elems, keys)) {
                    for (UInt32 i = 0; i < size; i++){
                        if(elems[i].String() != NULL){
                            if(persistentData.count(elems[i].String())){                                
                                g_arrayIntfc->SetElement(newArr, elems[i].String(), persistentData[elems[i].String()]);
                            }                            
                        }
				    }                    
                }  

                // delete data when done
                delete[] elems;
			    delete[] keys;
            }
        }
    }

    return true;
}

#endif // Oblivion

static ParamInfo kParams_NetworkPipe_Start[1] =
{
	{ "network port", kParamType_Integer, 0 },
};

static ParamInfo kParams_NetworkPipe_Send[1] =
{
	{ "array var", kParamType_Integer, 0 },
};

static ParamInfo kParams_NetworkPipe_CreateClient[2] =
{
	{ "exec path", kParamType_String, 0 },
    { "show flag", kParamType_Integer, 1 }, // is optional so set last value to 1
};

static ParamInfo kParams_NetworkPipe_KillClient[1] =
{
	{ "client pid", kParamType_Integer, 0 },
};

static ParamInfo kParams_NetworkPipe_SetData[1] =
{
	{ "array var", kParamType_Integer, 0 },
};

static ParamInfo kParams_NetworkPipe_GetData[1] =
{
	{ "array var", kParamType_Integer, 0 },
};

/**************************
* Command definitions
**************************/

DEFINE_COMMAND_PLUGIN(NetworkPipe_StartService, "starts the active acceptance of messages in game", 0, 1, kParams_NetworkPipe_Start);
DEFINE_COMMAND_PLUGIN(NetworkPipe_StopService, "stops the active acceptance of messages in game", 0, 0, NULL);
DEFINE_COMMAND_PLUGIN(NetworkPipe_Receive, "reads data from udp io", 0, 0, NULL);
DEFINE_COMMAND_PLUGIN(NetworkPipe_Send, "sends data to udp io", 0, 1, kParams_NetworkPipe_Send);

DEFINE_COMMAND_PLUGIN(NetworkPipe_IsNewGame, "checks status of a new game being started", 0, 0, NULL);

DEFINE_COMMAND_PLUGIN(NetworkPipe_CreateClient, "creates client program", 0, 2, kParams_NetworkPipe_CreateClient);
DEFINE_COMMAND_PLUGIN(NetworkPipe_KillClient, "kills client program", 0, 1, kParams_NetworkPipe_KillClient);

DEFINE_COMMAND_PLUGIN(NetworkPipe_SetData, "stores data", 0, 1, kParams_NetworkPipe_SetData);
DEFINE_COMMAND_PLUGIN(NetworkPipe_GetData, "retrieves data", 0, 1, kParams_NetworkPipe_GetData);

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

	// register commands
    // 0x2000 is for testing only
    // command range 0x2790-0x279F assigned to NetworkPipe plugin
	obse->SetOpcodeBase(NP_OPCODEBASE);

    // NetworkPipe Commands	    
    obse->RegisterCommand(&kCommandInfo_NetworkPipe_StartService);
    obse->RegisterCommand(&kCommandInfo_NetworkPipe_StopService);
    obse->RegisterTypedCommand(&kCommandInfo_NetworkPipe_Receive,kRetnType_Array);
    obse->RegisterTypedCommand(&kCommandInfo_NetworkPipe_Send,kRetnType_Array);

    obse->RegisterCommand(&kCommandInfo_NetworkPipe_IsNewGame);
    obse->RegisterCommand(&kCommandInfo_NetworkPipe_CreateClient);
    obse->RegisterCommand(&kCommandInfo_NetworkPipe_KillClient);
    
    obse->RegisterCommand(&kCommandInfo_NetworkPipe_SetData);
    obse->RegisterTypedCommand(&kCommandInfo_NetworkPipe_GetData,kRetnType_Array);

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
		g_serialization->SetSaveCallback(g_pluginHandle, NetworkPipe_SaveCallback);
		g_serialization->SetLoadCallback(g_pluginHandle, NetworkPipe_LoadCallback);
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
