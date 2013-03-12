#ifndef _PARSE_JSON_H
#define _PARSE_JSON_H

#include "obse/PluginAPI.h"
#include "obse/CommandTable.h"

#if OBLIVION
#include "obse/GameAPI.h"
#endif

#include "obse/ParamInfos.h"
#include "obse/Script.h"
#include "obse/GameObjects.h"

#include "../json/libjson.h"

#include "unicode_conv.h"

#include <boost/lexical_cast.hpp>

extern OBSEArrayVarInterface		* g_arrayIntfc;
extern OBSEScriptInterface			* g_scriptIntfc;
extern OBSESerializationInterface	* g_serialization;

// the default is string if no tags are provided
#define SIG_REFERENCE_VAR "ref:"

typedef OBSEArrayVarInterface::Array	OBSEArray;
typedef OBSEArrayVarInterface::Element	OBSEElement;

void ParseJSON(const JSONNode & n, OBSEArray* jsonMap, bool isStringMap, Script *callingScript, bool CreateForms);

void BuildJSON(JSONNode & n, OBSEArray* jsonMap, OBSEElement& root);

#endif // _PARSE_JSON_H