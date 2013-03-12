#include "parse_json.h"
#include "unicode_conv.h"

void ParseJSON(const JSONNode & n, OBSEArray *jsonMap, bool isStringMap, Script *callingScript, bool CreateForms){
    using boost::lexical_cast;
    using boost::bad_lexical_cast;

	static unsigned int level = 0;
    JSONNode::const_iterator i = n.begin();
   
    while (i != n.end()){
        // show types
        std::string node_name;
        std::string node_data;
        std::string temp;
        if(isStringMap)
            node_name = UniConv::get_string(i->name());
        node_data = UniConv::get_string(i->as_string());
        switch(i->type()){
            case JSON_NULL:
                //Console_Print("JSON_NULL");
                if(isStringMap)
                    g_arrayIntfc->SetElement(jsonMap,node_name.data(),node_data.data());
                else
                    g_arrayIntfc->AppendElement(jsonMap,node_data.data());
                break;
            case JSON_STRING:
                //Console_Print("JSON_STRING");
                if((node_data.compare(0,strlen(SIG_REFERENCE_VAR),SIG_REFERENCE_VAR) == 0) && CreateForms){
                    temp = node_data.substr(strlen(SIG_REFERENCE_VAR),node_data.length()-strlen(SIG_REFERENCE_VAR));
                    std::stringstream tmpStream;
                    UInt32 tint;
                    tmpStream << std::setbase(16);
                    tmpStream << temp;
                    tmpStream >> tint;
                    UInt32 tref;
                    TESForm *formref = NULL;
                    if(g_serialization->ResolveRefID(tint,&tref)){                                        
                        formref = LookupFormByID((UInt32)tref);
                        if(formref){
                            if(isStringMap)
                                g_arrayIntfc->SetElement(jsonMap,node_name.data(),formref);
                            else
                                g_arrayIntfc->AppendElement(jsonMap,formref);
                            break;  
                        }
                    }                           
                }
                // add more game specific type parsing here

                // default
                if(isStringMap)
                    g_arrayIntfc->SetElement(jsonMap,node_name.data(),node_data.data());
                else
                    g_arrayIntfc->AppendElement(jsonMap,node_data.data());
                break;
            case JSON_NUMBER:        
                //Console_Print("JSON_NUMBER");            

                // Elements only have 1 numeric type and that is double
                /*
                try{
                    json_int_t jint = lexical_cast<json_int_t>(node_data.data());
                    if(isStringMap)
                        g_arrayIntfc->SetElement(jsonMap,node_name.data(),jint);
                    else
                        g_arrayIntfc->AppendElement(jsonMap,jint);            
                    break;
                }catch(bad_lexical_cast &){
                    Console_Print("JSON_NUMBER: not an int: %s",node_data.data()); 
                }
                */
                try{
                    json_number jnum = lexical_cast<json_number>(node_data.data());
                    if(isStringMap)
                        g_arrayIntfc->SetElement(jsonMap,node_name.data(),jnum);
                    else
                        g_arrayIntfc->AppendElement(jsonMap,jnum);
                    break;
                }catch(bad_lexical_cast &){                
                    //Console_Print("JSON_NUMBER: not an int: %s",node_data.data()); 
                }

                Console_Print("JSON_NUMBER: failed to convert number: %s",node_data.data()); 
                if(isStringMap)
                    g_arrayIntfc->SetElement(jsonMap,node_name.data(),node_data.data());
                else
                    g_arrayIntfc->AppendElement(jsonMap,node_data.data());
                break;
            case JSON_BOOL:
                //Console_Print("JSON_BOOL");
                //Console_Print("JSON_BOOL: %d",i->as_bool());
                if(isStringMap)
                    g_arrayIntfc->SetElement(jsonMap,node_name.data(),i->as_bool());
                else
                    g_arrayIntfc->AppendElement(jsonMap,i->as_bool());            
                break;
            case JSON_ARRAY:
                //Console_Print("JSON_ARRAY");
                {
                    OBSEArray *subMap = g_arrayIntfc->CreateArray(NULL, 0, callingScript);
                    if(isStringMap){
                        g_arrayIntfc->SetElement(jsonMap,"array",subMap);
                    }else{
                        g_arrayIntfc->AppendElement(jsonMap,subMap);
                    }
                    level++;
                    ParseJSON(*i,jsonMap,false,callingScript,CreateForms);			
                    level--;
                }
                break;
            case JSON_NODE:
                //Console_Print("JSON_NODE");            
                {
			        OBSEArray *subMap = g_arrayIntfc->CreateStringMap(NULL, NULL, 0, callingScript);
                    if(isStringMap){
                        g_arrayIntfc->SetElement(jsonMap,"object",subMap);
                    }else{
                        g_arrayIntfc->AppendElement(jsonMap,subMap);
                    }
                    level++;
			        ParseJSON(*i,subMap,true,callingScript,CreateForms);
                    level--;
                }
                break;
            default:
                Console_Print("JSON_UNKNOWN");
        };        

        //increment the iterator
        ++i;
    }
}

// this will recurse over arrays
void BuildJSON(JSONNode & n, OBSEArray *jsonMap, OBSEElement& root){    
    // get the array size
    SInt32 size = g_arrayIntfc->GetArraySize(jsonMap);
    if (size != -1) {
        // create space for array keys and elements
	    OBSEElement* elems = new OBSEElement[size];
	    OBSEElement* keys = new OBSEElement[size];        
        UInt32 keyType = OBSEElement::kType_Invalid;

        // set the array key and elements
        if (g_arrayIntfc->GetElements(jsonMap, elems, keys)) {
            // create interator for json nodes
            JSONNode::iterator itr = n.end();
            // if there is data proceed
            if(size){
                // get the key type: Array or StringMap
                //   Map is silently converted to Array, warning Map structure is not maintained
                keyType = keys[0].GetType();
                // if this is a StringMap we will have a string key
                // add array var to previous array
                if(keyType == OBSEElement::kType_String){
                    JSONNode tmpNode(JSON_NODE);
                    if(root.String())
                        tmpNode.set_name(UniConv::get_wstring(std::string(root.String())));
                    n.push_back(JSONNode(tmpNode));
                }else{
                    JSONNode tmpNode(JSON_ARRAY);
                    if(root.String())
                        tmpNode.set_name(UniConv::get_wstring(std::string(root.String())));                    
                    n.push_back(JSONNode(tmpNode));
                }
                /*
                // debug to show type of array_var
                if(keyType == OBSEElement::kType_Numeric){
                    Console_Print("Array");
                }
                else if(keyType == OBSEElement::kType_String){
                    Console_Print("String Map");
                }
                */
            }
            // is there at least one element in the array
            if(n.size()){
                // grab last entry in json nodes
                itr = n.end()-1;                
            
                // temp conversion variables
                std::string tmpStr;
                std::stringstream tmpStream;

                // cycle through all data and add to node
                for (UInt32 i = 0; i < size; i++) {
                    // create empty json string
                    json_string keyString;                    

                    if(keyType == OBSEElement::kType_Numeric){
                        // do nothing, the empty string is what we want for arrays
                    }
                    else if(keyType == OBSEElement::kType_String){
                        // set json string to key
                        keyString = UniConv::get_wstring(std::string(keys[i].String()));
                    }
                    // set based upon type
                    //Console_Print("%s %s,%d",keys[i].String(),elems[i].String(),elems[i].Number());
                    switch(elems[i].GetType()){
                    // set numeric values
                    case OBSEElement::kType_Numeric:
                        itr->push_back(JSONNode(keyString,elems[i].Number()));                        
                        break;
                    // set refs to string with "ref:" tag
                    case OBSEElement::kType_Form:                        
                        tmpStream << "ref:" << std::hex << std::setw(8) << std::setfill('0');
                        tmpStream << elems[i].Form()->refID;
                        tmpStream >> tmpStr;
                        itr->push_back(JSONNode(keyString,UniConv::get_wstring(tmpStr)));                        
                        break;
                    // set string values
                    case OBSEElement::kType_String:
                        itr->push_back(JSONNode(keyString,UniConv::get_wstring(elems[i].String())));                        
                        break;
                    // recurse for array_var types
                    case OBSEElement::kType_Array:
                        BuildJSON(*itr, elems[i].Array(), keys[i]);                        
                        break;
                    }                                
                }
            }
        }      
        // dynamic array alloc needs array delete call
        delete[] elems;        
        delete[] keys;        
    }
}