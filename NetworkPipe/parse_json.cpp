#include "parse_json.h"

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