#include "obse/PluginAPI.h"

#include "NetworkPipe.h"

// helper function for creating an OBSE Map from a std::map<double, OBSEElement>
OBSEArray* MapFromStdMap(const std::map<double, OBSEElement>& data, Script* callingScript)
{
	OBSEArray* arr = g_arrayIntfc->CreateMap(NULL, NULL, 0, callingScript);
	for (std::map<double, OBSEElement>::const_iterator iter = data.begin(); iter != data.end(); ++iter) {
		g_arrayIntfc->SetElement(arr, iter->first, iter->second);
	}

	return arr;
}

// helper function for creating OBSE Array from std::vector<OBSEElement>
OBSEArray* ArrayFromStdVector(const std::vector<OBSEElement>& data, Script* callingScript)
{
	OBSEArray* arr = g_arrayIntfc->CreateArray(&data[0], data.size(), callingScript);
	return arr;
}

// helper function for creating an OBSE StringMap from a std::map<std::string, OBSEElement>
OBSEArray* StringMapFromStdMap(const std::map<std::string, OBSEElement>& data, Script* callingScript)
{
	// create empty string map
	OBSEArray* arr = g_arrayIntfc->CreateStringMap(NULL, NULL, 0, callingScript);

	// add each key-value pair
	for (std::map<std::string, OBSEElement>::const_iterator iter = data.begin(); iter != data.end(); ++iter) {
		g_arrayIntfc->SetElement(arr, iter->first.c_str(), iter->second);
	}

	return arr;
}

// example commands
extern std::string	g_strData;

#if OBLIVION
bool Cmd_TestExtractArgsEx_Execute(COMMAND_ARGS)
{
	UInt32 i = 0;
	char str[0x200] = { 0 };
	*result = 0.0;

	if (ExtractArgsEx(paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, &i, str)) {
		Console_Print("TestExtractArgsEx >> int: %d str: %s", i, str);
		*result = 1.0;
	}
	else {
		Console_Print("TestExtractArgsEx >> couldn't extract arguments");
	}

	return true;
}

bool Cmd_TestExtractFormatString_Execute(COMMAND_ARGS)
{
	char str[0x200] = { 0 };
	int i = 0;
	TESForm* form = NULL;
	*result = 0.0;

	if (ExtractFormatStringArgs(0, str, paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, 
		SIZEOF_FMT_STRING_PARAMS + 2, &i, &form)) {
			Console_Print("TestExtractFormatString >> str: %s int: %d form: %08X", str, i, form ? form->refID : 0);
			*result = 1.0;
	}
	else {
		Console_Print("TestExtractFormatString >> couldn't extract arguments.");
	}

	return true;
}

bool Cmd_ExamplePlugin_0019Additions_Execute(COMMAND_ARGS)
{
	// tests and demonstrates 0019 additions to plugin API
	// args:
	//	an array ID as an integer
	//	a function script with the signature {int, string, refr} returning a string
	// return:
	//	an array containing the keys and values of the original array

	UInt32 arrID = 0;
	TESForm* funcForm = NULL;

	if (ExtractArgsEx(paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, &arrID, &funcForm)) {

		// look up the array
		 OBSEArray* arr = g_arrayIntfc->LookupArrayByID(arrID);
		 if (arr) {
			 // get contents of array
			 UInt32 size = g_arrayIntfc->GetArraySize(arr);
			 if (size != -1) {
				 OBSEElement* elems = new OBSEElement[size];
				 OBSEElement* keys = new OBSEElement[size];

				 if (g_arrayIntfc->GetElements(arr, elems, keys)) {
					 OBSEArray* newArr = g_arrayIntfc->CreateArray(NULL, 0, scriptObj);
					 for (UInt32 i = 0; i < size; i++) {
						 g_arrayIntfc->SetElement(newArr, i*2, elems[i]);
						 g_arrayIntfc->SetElement(newArr, i*2+1, keys[i]);
					 }

					 // return the new array
					 g_arrayIntfc->AssignCommandResult(newArr, result);
				 }

				 delete[] elems;
				 delete[] keys;

			 }
		 }

		 if (funcForm) {
			 Script* func = OBLIVION_CAST(funcForm, TESForm, Script);
			 if (func) {
				 // call the function
				 OBSEElement funcResult;
				 if (g_scriptIntfc->CallFunction(func, thisObj, NULL, &funcResult, 3, 123456, "a string", *g_thePlayer)) {
					 if (funcResult.GetType() == funcResult.kType_String) {
						 Console_Print("Function script returned string %s", funcResult.String());
					 }
					 else {
						 Console_Print("Function did not return a string");
					 }
				 }
				 else {
					 Console_Print("Could not call function script");
				 }
			 }
			 else {
				 Console_Print("Could not extract function script argument");
			 }
		 }
	}

	return true;
}

bool Cmd_ExamplePlugin_MakeArray_Execute(COMMAND_ARGS)
{
	// Create an array of the format
	// { 
	//	 0:"Zero"
	//	 1:1.0
	//	 2:PlayerRef
	//	 3:StringMap { "A":"a", "B":123.456, "C":"manually set" }
	//	 4:"Appended"
	//	}

	// create the inner StringMap array
	std::map<std::string, OBSEElement> stringMap;
	stringMap["A"] = "a";
	stringMap["B"] = 123.456;

	// create the outer array
	std::vector<OBSEElement> vec;
	vec.push_back("Zero");
	vec.push_back(1.0);
	vec.push_back(*g_thePlayer);
	
	// convert our map to an OBSE StringMap and store in outer array
	OBSEArray* stringMapArr = StringMapFromStdMap(stringMap, scriptObj);
	vec.push_back(stringMapArr);

	// manually set another element in stringmap
	g_arrayIntfc->SetElement(stringMapArr, "C", "manually set");

	// convert outer array
	OBSEArray* arr = ArrayFromStdVector(vec, scriptObj);

	// append another element to array
	g_arrayIntfc->AppendElement(arr, "appended");

	if (!arr)
		Console_Print("Couldn't create array");

	// return the array
	if (!g_arrayIntfc->AssignCommandResult(arr, result))
		Console_Print("Couldn't assign array to command result.");

	// result contains the new ArrayID; print it
	Console_Print("Returned array ID %.0f", *result);

	return true;
}

bool Cmd_PluginTest_Execute(COMMAND_ARGS)
{
	_MESSAGE("plugintest");

	*result = 42;

	Console_Print("plugintest running");

	return true;
}

bool Cmd_ExamplePlugin_PrintString_Execute(COMMAND_ARGS)
{
	Console_Print("PrintString: %s", g_strData.c_str());

	return true;
}

bool Cmd_ExamplePlugin_SetString_Execute(COMMAND_ARGS)
{
	char	data[512];

	if(ExtractArgs(PASS_EXTRACT_ARGS, &data))
	{
		g_strData = data;
		Console_Print("Set string %s in script %08x", data, scriptObj->refID);
	}

	ExtractFormattedString(ScriptFormatStringArgs(0, 0, 0, 0), data);
	return true;
}
#endif // OBLIVION