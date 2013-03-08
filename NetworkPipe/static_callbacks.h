#include "NetworkPipe.h"

// example callbacks
std::string	g_strData;

static void ResetData(void)
{
	g_strData.clear();
}

static void ExamplePlugin_SaveCallback(void * reserved)
{
	// write out the string
	//g_serialization->OpenRecord('STR ', 0);
	//g_serialization->WriteRecordData(g_strData.c_str(), g_strData.length());

	// write out some other data
	//g_serialization->WriteRecord('ASDF', 1234, "hello world", 11);
    return;
}

static void ExamplePlugin_LoadCallback(void * reserved)
{
	//UInt32	type, version, length;

	ResetData();

    /*
	char	buf[512];

	while(g_serialization->GetNextRecordInfo(&type, &version, &length))
	{
		_MESSAGE("record %08X (%.4s) %08X %08X", type, &type, version, length);

		switch(type)
		{
			case 'STR ':
				g_serialization->ReadRecordData(buf, length);
				buf[length] = 0;

				_MESSAGE("got string %s", buf);

				g_strData = buf;
				break;

			case 'ASDF':
				g_serialization->ReadRecordData(buf, length);
				buf[length] = 0;

				_MESSAGE("ASDF chunk = %s", buf);
				break;
			default:
				_MESSAGE("Unknown chunk type $08X", type);
		}
	}
    */
}

static void ExamplePlugin_PreloadCallback(void * reserved)
{
	ExamplePlugin_LoadCallback(reserved);
}

static void ExamplePlugin_NewGameCallback(void * reserved)
{
	ResetData();
}

static CommandInfo kPluginTestCommand =
{
	"plugintest",
	"",
	0,
	"test command for obse plugin",
	0,		// requires parent obj
	0,		// doesn't have params
	NULL,	// no param table

	HANDLER(Cmd_PluginTest_Execute)
};

static ParamInfo kParams_ExamplePlugin_0019Additions[2] =
{
	{ "array var", kParamType_Integer, 0 },
	{ "function script", kParamType_InventoryObject, 0 },
};

//DEFINE_COMMAND_PLUGIN(ExamplePlugin_SetString, "sets a string", 0, 1, kParams_OneString)
//DEFINE_COMMAND_PLUGIN(ExamplePlugin_PrintString, "prints a string", 0, 0, NULL)
//DEFINE_COMMAND_PLUGIN(ExamplePlugin_MakeArray, test, 0, 0, NULL);
//DEFINE_COMMAND_PLUGIN(ExamplePlugin_0019Additions, "tests 0019 API", 0, 2, kParams_ExamplePlugin_0019Additions);

static ParamInfo kParams_TestExtractArgsEx[2] =
{
	{	"int",		kParamType_Integer,	0	},
	{	"string",	kParamType_String,	0	},
};

static ParamInfo kParams_TestExtractFormatString[SIZEOF_FMT_STRING_PARAMS + 2] =
{
	FORMAT_STRING_PARAMS,
	{	"int",		kParamType_Integer,	0	},
	{	"object",	kParamType_InventoryObject,	0	},
};

//DEFINE_COMMAND_PLUGIN(TestExtractArgsEx, "tests 0020 changes to arg extraction", 0, 2, kParams_TestExtractArgsEx);
//DEFINE_COMMAND_PLUGIN(TestExtractFormatString, "tests 0020 changes to format string extraction", 0, 
					  //SIZEOF_FMT_STRING_PARAMS+2, kParams_TestExtractFormatString);