/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#include "stapin-plugin-symbols-logic.h"

#include <list>
#include <queue>

namespace symbolsandexpressionslogic 
{
    //shared global map with expressions, defined as "extern" in stapin-plugin-symbols-and-expressions-logic.h
    stapinlogic::SymbolsIDToIteratorMap& symbols_id_to_iterator()
    {
        static stapinlogic::SymbolsIDToIteratorMap symbolsIDToIterator;
        return symbolsIDToIterator;
    }
};

namespace{
    static std::mutex symbolIteratorMapMutex;
    //**************** Consts ************************** */
    const std::string SPACE = " ";
    const std::string CONST = "const";
    const std::string POINTER = "*";
    const std::string REFERENCE = "&";
    const std::string R_VAL_REFERENCE = "&&";
    const std::string VOLATILE = "volatile";
    const std::string FUNCTION = "function";
    const std::string COMPLEX = "complex";
    const std::string ENUM = "enum";
    const std::string MEMBER_PTR = "member_pointer"; 
    /**
     * FORWARD DECLERATIONS
     */
    void get_symbol_by_address_async(void* arg);
    void get_symbol_by_id_async(void* arg);
    void get_symbol_by_reg_async(void* arg);

    enum GET_SYMBOL_TYPE
    {
        //used for GET_TYPE_TO_FUNC index. Do not change order!
        ADDRESS = 0,
        REG,
        ID,
        COUNT //not part of the enum, just for counting items the items.
    };

    const std::array<void(*)(void*), GET_SYMBOL_TYPE::COUNT> GET_TYPE_TO_FUNC = {
        get_symbol_by_address_async,
        get_symbol_by_reg_async,
        get_symbol_by_id_async
    };


    LocationExpressionState * evaluate_symbol_location(const Symbol * sym) {
        Trap trap;
        Context * ctx = nullptr;
        int frame = STACK_NO_FRAME;
        StackFrame * frameInfo = nullptr;
        LocationExpressionState * state = nullptr;
        LocationInfo * locInfo = nullptr;
        if (0 > get_symbol_frame(sym, &ctx, &frame))
        {
            return nullptr;
        } 
        if (0 > get_location_info(sym, &locInfo))
        {
            return nullptr;
        } 
        if (0 < locInfo->args_cnt) 
        {
            return nullptr;
        }
        if (STACK_NO_FRAME != frame && 0 > get_frame_info(ctx, frame, &frameInfo))
        {
            return nullptr;
        } 
        if (!set_trap(&trap)) return nullptr;
        state = evaluate_location_expression(ctx, frameInfo,
            locInfo->value_cmds.cmds, locInfo->value_cmds.cnt, nullptr, 0);
        clear_trap(&trap);
        return state;
    }

    /**
     * @brief Appends the symbol name to the type name string.
     * @param symbol A pointer to the Symbol structure.
     * @param typeName A reference to the string where the symbol name will be appended.
     * @return bool to notify if name was found and appended or not.
     */
    bool append_symbol_name(const Symbol* symbol, std::string& typeName) {
        char* typeNameBuf = nullptr;
        get_symbol_name(symbol, &typeNameBuf);
        if (nullptr != typeNameBuf) 
        {
            typeName += typeNameBuf;
            return true;
        }
        //TODO write to log
        return false;
    }

    /**
     * @brief checks if the type of given symbol has flags of given flag.
     * @param flagToCheck flags of the check.
     * @param symbol thre symbol to check it's type's flags.
     * @return boolean that indicates if the check result is true.
     */
    bool type_symbol_flag_check(const SYM_FLAGS flagToCheck, const Symbol* symbol)
    {
        Symbol* typeSymbol;
        SYM_FLAGS flagsForTypeSymbol;
        return (0 == get_symbol_type(symbol, &typeSymbol) &&
                0 == get_symbol_flags(typeSymbol, &flagsForTypeSymbol) && 
                0 != (flagsForTypeSymbol & flagToCheck));
    }

    /**
     * @brief Appends the const or volatile names or both to type name.
     * **Note** - sometimes the volatile indicator is in the type symbol, 
     * therfore we check it in the symbol and in it's type if possible.
     * @param flags flags of the symbol.
     * @param typeName A reference to the string where the symbol type name will be appended.
     * @param baseTypeSymbol The base type of the symbol.
     * @param spaceFirst boolean to notify if the space is before or after the const or volatile names.
     */
    void append_const_and_volatile_names(SYM_FLAGS flags, std::string& typeName, const Symbol* baseTypeSymbol, bool spaceFirst) {
        if(0 != (flags & SYM_FLAG_CONST_TYPE) || type_symbol_flag_check(SYM_FLAG_CONST_TYPE, baseTypeSymbol))
        {
            typeName += spaceFirst? SPACE + CONST: CONST + SPACE;
        }
        if(0 != (flags & SYM_FLAG_VOLATILE_TYPE) || type_symbol_flag_check(SYM_FLAG_VOLATILE_TYPE, baseTypeSymbol))
        {
            typeName += spaceFirst? SPACE + VOLATILE: VOLATILE + SPACE;
        }
    }

    /**
     * @brief Appends the type class name to type name.
     * @param typeName A reference to the string where the symbol type name will be appended.
     * @param symbolClass the type class of the symbol to check.
     */
    void append_type_class_names_to_symbol_type_name(std::string& typeName, int symbolClass)
    {
        switch(symbolClass)
        {
            case TYPE_CLASS_FUNCTION:
                typeName += FUNCTION;
                break;
            case TYPE_CLASS_COMPLEX:
                typeName += COMPLEX;
                break;
            case TYPE_CLASS_ENUMERATION:
                typeName += ENUM;
                break;
            case TYPE_CLASS_MEMBER_PTR:
                typeName += MEMBER_PTR;
                break;
            default:
                break;
        }
    }

    /**
     * @brief get symbol pointer type - pointer, reference or rvalue reference.
     * @param flags flags of the symbol.
     * @return the result matching string
     */
    std::string get_pointer_type(const SYM_FLAGS flags)
    {
        if(0 != (flags & SYM_FLAG_REFERENCE)) //REFERENCE
        {
            if(0 != (flags & SYM_FLAG_RVALUE)) //RVALUE REFERENCE
            {
                return R_VAL_REFERENCE;
            }
            else //REGULAR REFERENCE
            {
                return REFERENCE;
            }
        }
        else
        {
            return POINTER;
        }
    }

    /**
     * @brief Adds the name of the basic type to the final type name.
     * @details Loops over the type symbol until a name is found or stays with the same uniqueID. 
     * @param baseTypeSymbol symbol of the base type.
     * @param finalTypeName final name.
     * @return the result matching string
     */
    void append_base_type_name(Symbol* baseTypeSymbol, std::string& finalTypeName)
    {
        auto typeSymbol = baseTypeSymbol; 
        const char* prevUniqueID = "";
        auto curUniqueID = symbol2id(typeSymbol);
        bool appended = append_symbol_name(typeSymbol, finalTypeName);
        bool gotSymbolType = 0 == get_symbol_type(typeSymbol, &typeSymbol);
        bool differentUniqueIDs =  0 != strcmp(prevUniqueID, curUniqueID);
        while (!appended     &&
               gotSymbolType &&
               differentUniqueIDs)
        {
            prevUniqueID = curUniqueID;
            curUniqueID = symbol2id(typeSymbol);
            gotSymbolType = 0 == get_symbol_type(typeSymbol, &typeSymbol);
            differentUniqueIDs =  0 != strcmp(prevUniqueID, curUniqueID);
            appended = append_symbol_name(typeSymbol, finalTypeName);
        }
    }
        

    /**
     * @brief Function to check if type name handling is needed by the type class.
     * @param typeClass The typeclass to check.
     * @return boolean  to indicate if type name handling is needed.
     */
    bool type_name_needed_by_type_class(int typeClass)
    {
        return  TYPE_CLASS_POINTER == typeClass     ||
                TYPE_CLASS_ARRAY == typeClass       ||
                TYPE_CLASS_FUNCTION == typeClass    ||
                TYPE_CLASS_COMPLEX == typeClass     ||
                TYPE_CLASS_ENUMERATION == typeClass ||
                TYPE_CLASS_MEMBER_PTR == typeClass;
    }
    
    /**
    * @brief function to process each symbol during symbol enumeration. Sometimes called as callback.
    * @details This function is called for each symbol found. It extracts symbol details,
    *          creates a symbolAttr symbol structure, and pushes it into the symbols queue
    *          of the provided Symbols_iterator. It handles symbol properties such as unique ID,
    *          name, address, size, flags, type, and register-related information.
    * @param args A void pointer to Symbols_iterator, used to store and manage symbols.
    * @param symbol A pointer to the Symbol structure containing symbol details.
    */
    void  fill_symbol_info (void * args, Symbol *symbol){
        auto curSymbolIterator = static_cast<stapinlogic::Symbols_iterator*>(args);    
        stapinlogic::Symbol_attributes symbolAttr{0};
        symbolsandexpressionslogic::fill_symbol_attributes(symbol, &symbolAttr);
        curSymbolIterator->symbols.push_back(symbolAttr);
        return;
    }

    /**
     * @brief Asynchronously retrieves a list of symbols without a parent symbol.
     * @details This function checks if a specific name is provided and finds symbols by name.
     *          If no name is provided, it enumerates all symbols. It posts a callback for each
     *          symbol found and notifies once all symbols are processed.
     * @param arg A void pointer to Symbols_iterator used for storing and managing symbols.
     */
    void get_symbol_list_async_no_parent(void* arg){
        auto curSymbolIterator = static_cast<stapinlogic::Symbols_iterator*>(arg);
        auto tcfContext = stapincontext::get_TCF_context(curSymbolIterator->pid, curSymbolIterator->tid );
        if(nullptr == tcfContext)
        {
            //TODO write to log
            notify_conditional_variable(curSymbolIterator);
            return;
        }
        stapincontext::notify_stop_context(tcfContext);;
        if(!curSymbolIterator->name.empty())
        {
            auto regValIp = stapincontext::get_register_value(tcfContext, stapinlogic::IP_NAME.c_str());
            if(nullptr != regValIp)
            {
                Symbol* symbols = nullptr;
                if(0 == find_symbol_by_name(tcfContext, 0,  (uint64_t)regValIp, 
                curSymbolIterator->name.c_str(), &symbols))
                {
                    if(nullptr != symbols)
                    {
                        fill_symbol_info (curSymbolIterator, symbols);
                        Symbol* foundSym = nullptr;
                        while (0 == find_next_symbol(&foundSym))
                        {
                            fill_symbol_info (curSymbolIterator, foundSym);
                        }
                    }
                    else
                    { //global symbol!!!
                        Value v;
                        if (0 <= evaluate_expression(tcfContext, 0, 0, curSymbolIterator->name.data(), 0, &v)) 
                        {
                            if ((nullptr != v.sym))
                            {
                                fill_symbol_info (curSymbolIterator, v.sym);
                            }
                        }
                    }
                }
            }
        }
        else
        {
            enumerate_symbols(tcfContext, 0, fill_symbol_info ,  curSymbolIterator);
        }
        stapincontext::notify_resume_context(tcfContext); 
        //THIS IS HERE AND NOT IN THE CALLBACK BECAUSE IT CALLS THE  CALLBACK FOR EACH SYMBOL
        notify_conditional_variable(curSymbolIterator);
        
    }

    /**
     * @brief This function handles the finding of a child variable memroy by it's parent.
     * @details This function finds the offset of the child inside the parent and adding it to the parent address.
     * @param parentSymbol the parent symbol.
     * @param curSymbolIterator the current symbol iterator.
     * @param childSymbol the child symbol.
     */
    void handle_child_symbol_memory(const Symbol* parentSymbol, stapinlogic::Symbols_iterator* curSymbolIterator, Symbol* childSymbol)
    {
        int parentSymbolTypeClass;
        ContextAddress parentAddress;
        if(0 == get_symbol_type_class(parentSymbol, &parentSymbolTypeClass) && 
           SYM_CLASS_BLOCK == parentSymbolTypeClass                         &&
           0 == get_symbol_address(parentSymbol, &parentAddress))
        {
            ContextAddress offset = 0;
            if(0 == get_symbol_offset(childSymbol, &offset))
            {
                auto& childSymbolAttributes =  curSymbolIterator->symbols.back();
                childSymbolAttributes.addr = parentAddress + offset;
                childSymbolAttributes.rpcFlags = stapinlogic::E_rpc_symbol_flags::VALUE_IN_MEM;
                childSymbolAttributes.regIndex = 0;
            }
        }
    }

    /**
     * @brief Asynchronously retrieves a list of symbols that are children of a specified parent symbol.
     * @details This function finds child symbols of a given parent symbol. If a specific name is provided,
     *          it finds a symbol by name within the scope of the parent. It handles the enumeration of symbols
     *          and posts a callback for each symbol found. It notifies once all symbols are processed.
     * @param arg A void pointer to Symbols_iterator used for storing and managing symbols.
     */
    void get_symbol_list_async_with_parent(void* arg){
        auto curSymbolIterator = static_cast<stapinlogic::Symbols_iterator*>(arg);
        auto tcfContext = stapincontext::get_TCF_context(curSymbolIterator->pid, curSymbolIterator->tid );
        if(nullptr == tcfContext)
        {
            //TODO write to log
            notify_conditional_variable(curSymbolIterator);
            return;
        }
        stapincontext::notify_stop_context(tcfContext);;
        Symbol* parentSymbol = nullptr;
        auto foundParent = id2symbol(curSymbolIterator->parentUinqueID.c_str(), &parentSymbol);
        if(-1 != foundParent || nullptr != parentSymbol )
        {
            //Parent found
            if(curSymbolIterator->name.empty())
            {
                //Name is not provided - find all children symbols of parent (can't find in scope as we need a name for that).
                int count = 0;
                Symbol ** children = nullptr;
                int typeClass;
                get_symbol_type_class(parentSymbol, &typeClass);
                if(TYPE_CLASS_FUNCTION == typeClass) //FIND return type + ARGUMENTS!
                {
                    Symbol* typeSymbol;
                    //return type is the base typr of the symbol
                    if(0 == get_symbol_base_type(parentSymbol, &typeSymbol))
                    {
                        fill_symbol_info(curSymbolIterator, typeSymbol);
                    }
                    //argumetns are children of the type symbol!
                    if(0 == get_symbol_type(parentSymbol, &typeSymbol))
                    {
                        if(0 == get_symbol_children(typeSymbol, &children, &count))
                        {
                            for (size_t i = 0; i < count; ++i)
                            {
                                fill_symbol_info(curSymbolIterator, children[i]);
                                handle_child_symbol_memory(parentSymbol, curSymbolIterator, children[i]);
                            } 
                        }
                    }
                }
                else if(0 == get_symbol_children(parentSymbol, &children, &count))
                {
                    // Parent has children - get the symbols and their memory location 
                    for (size_t i = 0; i < count; ++i)
                    {
                        fill_symbol_info(curSymbolIterator, children[i]);
                        handle_child_symbol_memory(parentSymbol, curSymbolIterator, children[i]);
                    }
                }
            }
            else
            {
                //Find the symbol by a provided name - in parent scope or in parent children.
                auto regValIp = stapincontext::get_register_value(tcfContext, stapinlogic::IP_NAME.c_str()); //To find in scope we need IP register value
                if(nullptr != regValIp)
                {
                    //no error in finding the IP register value
                    Symbol* symbols = nullptr;
                    auto foundSymbolsInScope = find_symbol_in_scope(tcfContext, 0, (uint64_t)regValIp , parentSymbol, curSymbolIterator->name.c_str(),  &symbols);
                    if(0 == foundSymbolsInScope && nullptr != symbols)
                    {
                        //Found symbol in scope of parent (found using the provided name).
                        fill_symbol_info (curSymbolIterator, symbols); //This is the first found symbol (in "symbols").
                        Symbol* foundSym = nullptr;
                        while (0 == find_next_symbol(&foundSym))
                        {
                            //Get all other matching symbols (to the provided name) in parent scope
                            fill_symbol_info (curSymbolIterator, foundSym);
                        }
                    }
                    else
                    {
                        //Try to find in parent's children
                        int count = 0;
                        Symbol ** children = nullptr;
                        if(0 == get_symbol_children(parentSymbol, &children, &count))
                        {
                            // Parent has children - get the symbols and their memory location if name matches to the provided name.
                            for (size_t i = 0; i < count; ++i)
                            {
                                char* tempNameBuf = nullptr;
                                auto foundSymbolName = get_symbol_name( children[i], &tempNameBuf);
                                if(-1 != foundSymbolName && strlen(tempNameBuf) == curSymbolIterator->name.length() && 
                                0 == strncmp(tempNameBuf, curSymbolIterator->name.c_str(), curSymbolIterator->name.length()))
                                {
                                    //Found a symbol with a matching name!
                                    fill_symbol_info(curSymbolIterator, children[i]);
                                    handle_child_symbol_memory(parentSymbol, curSymbolIterator, children[i]);
                                }
                            }
                        }
                    }
                } 
            }
        }
        stapincontext::notify_resume_context(tcfContext);
        //THIS IS HERE AND NOT IN THE CALLBACK BECAUSE IT CALLS THE  CALLBACK FOR EACH SYMBOL
        notify_conditional_variable(curSymbolIterator);
    }

    /**
     * @brief Indicating whether given flags are indicating a type symbol or not.
     * @param flags int which is the TCF flags.
     * @return True if the given flags are indicating a type symbol.
     */
    bool is_type_symbol(int flags){
        return 0 != (flags & stapinlogic::SYMBOL_TYPE_FLAGS_MASK);
    }

    /**
     * @brief Translating TCF typeClass to STAPIN type.
     * @param typeClass int which is the TCF typeClass.
     * @return The result STAPIN type after translation.
     */
    unsigned int translate_to_STAPIN_symbol_type(int typeClass){
        switch (typeClass)
        {
            case TYPE_CLASS_FUNCTION:
                return (int)stapinlogic::E_symbol_type::FUNCTION;
            case TYPE_CLASS_INTEGER:
                return (int)stapinlogic::E_symbol_type::INTEGER;
            case TYPE_CLASS_MEMBER_PTR:
            case TYPE_CLASS_POINTER:
                return(int)stapinlogic::E_symbol_type::POINTER;
            case TYPE_CLASS_REAL:
                return (int)stapinlogic::E_symbol_type::REAL;
            case TYPE_CLASS_ARRAY:
                return (int)stapinlogic::E_symbol_type::ARRAY;
            default:
                //TODO write to log
                return (int)stapinlogic::E_symbol_type::UNKNOWN; // Default case for unmapped or unknown values
        }
    }

    /**
     * @brief Function to fill common attributes in symbols.
     * @param rpcSymbol the rpc symbol to fill.
     * @param symbolAttr The symbol attributes to fill.
     */
    void fill_common_attributes_in_symbol(stapin::Rpc_symbol* rpcSymbol, const stapinlogic::Symbol_attributes* symbolAttr){
          //flags
        if (is_type_symbol(symbolAttr->flags))
        {
            rpcSymbol->flags = (int)stapinlogic::E_rpc_symbol_flags::SYMBOL_IS_TYPE;
        }
        else
        {
            rpcSymbol->flags = (int)symbolAttr->rpcFlags;
        }
        //TO KNOW WEATHER IT IS IN A REGISTERT OR NOT!! 
        if(stapinlogic::E_rpc_symbol_flags::VALUE_IN_MEM == symbolAttr->rpcFlags)
        {
            rpcSymbol->memoryOrReg = symbolAttr->addr;
        }
        else if (stapinlogic::E_rpc_symbol_flags::VALUE_IN_REG == symbolAttr->rpcFlags)
        {
            rpcSymbol->memoryOrReg = symbolAttr->regIndex;
        }
        else
        {
            rpcSymbol->memoryOrReg = 0;
        }
        rpcSymbol->size = symbolAttr->size;
        rpcSymbol->type =  translate_to_STAPIN_symbol_type(symbolAttr->symbolClass);
    }

    /**
     * @brief Converts symbol data to an RPC-compatible format and stores it in a Registry.
     * @details This function checks for new unique names and IDs, copies them into buffers, and updates
     *          indices and offsets. It handles the conversion of symbol flags and registration details.
     *          It ensures that there is enough capacity in the RPC symbols array before adding new entries.
     * @param symbolAttr A pointer to Symbol_attributes containing the symbol's details.
     * @param curSymbolsRegistry A pointer to Registry where the RPC-compatible symbol data is stored.
     * @return A boolean indicating success (true) or failure (false) of the operation.
     */
    bool symbols_to_RPC_symbol_wrapper(const stapinlogic::Symbol_attributes* symbolAttr, stapinlogic::Registry* curSymbolsRegistry ){
        // Ensure there is enough capacity in the RPCexpressions array
        auto rpcSymbolsSize = sizeof(stapin::Rpc_symbol);
        if (curSymbolsRegistry->curIndex + rpcSymbolsSize > curSymbolsRegistry->rpcItemsArraySize) 
        {
            //TODO write to log
            return false;
        }
        // stapin::Rpc_symbol rpcSymbol{0};
        auto rpcSymbol = (stapin::Rpc_symbol*)(curSymbolsRegistry->rpcItemsArray.get() + curSymbolsRegistry->curIndex);
        auto nameSearch = curSymbolsRegistry->name2index.end();
        auto uniqueIDSearch = curSymbolsRegistry->uniqueID2index.end();
        auto typeUniqueIDSearch =  curSymbolsRegistry->uniqueID2index.end();
        auto baseTypeUniqueIDSearch = curSymbolsRegistry->uniqueID2index.end();
        /** filling the arrays if needed */
        {
            std::lock_guard<std::mutex> lock(curSymbolsRegistry->mtx);
            nameSearch = curSymbolsRegistry->name2index.find(symbolAttr->name);
            uniqueIDSearch =  curSymbolsRegistry->uniqueID2index.find(symbolAttr->uniqueID);
            typeUniqueIDSearch =  curSymbolsRegistry->uniqueID2index.find(symbolAttr->typeUniqueID);
            baseTypeUniqueIDSearch = curSymbolsRegistry->uniqueID2index.find(symbolAttr->baseTypeUniqueID);
        }
        //NAMES
        if(curSymbolsRegistry->name2index.end() == nameSearch)
        { //new name! 
            symbolsandexpressionslogic::insert_name_or_ID_to_registry_buffer(symbolAttr->name, curSymbolsRegistry, true, true);
        } 
        //UNIQUE IDS
        if(curSymbolsRegistry->uniqueID2index.end() == typeUniqueIDSearch)
        { // new unique ID! 
            symbolsandexpressionslogic::insert_name_or_ID_to_registry_buffer(symbolAttr->uniqueID, curSymbolsRegistry, false, true);
        }
        //TYPE UNIQUE IDS
        if( curSymbolsRegistry->uniqueID2index.end() == typeUniqueIDSearch)
        { // new unique ID! 
            symbolsandexpressionslogic::insert_name_or_ID_to_registry_buffer(symbolAttr->typeUniqueID, curSymbolsRegistry, false, true);
        }
        //BASE TYPE UNIQUE IDS
        if(  curSymbolsRegistry->uniqueID2index.end() == baseTypeUniqueIDSearch)
        { // new unique ID! 
            symbolsandexpressionslogic::insert_name_or_ID_to_registry_buffer(symbolAttr->baseTypeUniqueID, curSymbolsRegistry, false, true);
        }
        fill_common_attributes_in_symbol(rpcSymbol, symbolAttr);
        {
            std::lock_guard<std::mutex> lock(curSymbolsRegistry->mtx);
            rpcSymbol->nameOffset = curSymbolsRegistry->name2index[symbolAttr->name];     //!< Symbol name offset in a names buffer
            rpcSymbol->uniqueIdOffset =  curSymbolsRegistry->uniqueID2index[symbolAttr->uniqueID]; //!< unique Id offset in a unique Ids buffer
            rpcSymbol->typeIdOffset = curSymbolsRegistry->uniqueID2index[symbolAttr->typeUniqueID];   //!< unique Id of the type of the symbol in a unique Ids buffer
            rpcSymbol->baseTypeIdOffset = curSymbolsRegistry->uniqueID2index[symbolAttr->baseTypeUniqueID];  //!< unique Id of the base type of the symbol in a unique Ids buffer
        }
        // Assign the constructed rpcSymbol to the array and Increment the index for the next symbol
        curSymbolsRegistry->curIndex += rpcSymbolsSize;
        return true;
    }

    //***********************END OF GET SYMBOLS START HELPERS********************** */

    //***********************HELPER FUNCTION OF GET SYMBOLS BY QUERY********************** */

    /**
     * @brief function to handle symbol retrieval by query - address, reg or ID.
     * @details This function processes a symbol found by its unique ID OR by address OR by reg. It retrieves various properties of the symbol
     *          such as address, size, type, and flags, and updates the query with these details.
     * @param curQuery A Symbol_by_query which contains the unique ID OR address OR reg and where results will be stored.
     * @param symbol Pointer to the Symbol structure to be processed.
     */
    void fill_symbol_info_by_query(void* args, Symbol *symbol) {
        auto curQuery = static_cast<stapinlogic::Symbol_by_query*>(args);
        if(curQuery->found) 
        {
            //TODO write to log
            return;
        }
        stapinlogic::Symbol_attributes symbolAttr {0};
        // Address-based query
        symbolsandexpressionslogic::fill_symbol_attributes(symbol, &symbolAttr);
        if (0 != curQuery->symbolSearchAddr) 
        {
            if(stapinlogic::E_rpc_symbol_flags::VALUE_IN_MEM != symbolAttr.rpcFlags || curQuery->symbolSearchAddr != symbolAttr.addr)
            {
                //TODO write to log
                curQuery->found = false;
                return;
            }
        }

        // Register-based query
        if (!curQuery->regName.empty()) 
        {
            if(stapinlogic::E_rpc_symbol_flags::VALUE_IN_REG != symbolAttr.rpcFlags || curQuery->regIndex != symbolAttr.regIndex)
            {
                //TODO write to log
                curQuery->found = false;
                return;
            }
        }
        // Common symbol information retrieval
        fill_common_attributes_in_symbol((stapin::Rpc_symbol*)curQuery->resultSymbol.get(), &symbolAttr);
        curQuery->symbolName = std::move(symbolAttr.name);
        curQuery->typeUniqueID = std::move(symbolAttr.typeUniqueID);
        curQuery->baseTypeUniqueID = std::move(symbolAttr.baseTypeUniqueID);
        curQuery->uniqueID = symbol2id(symbol);
        curQuery->found = true;
    }

    //***********************END HELPER FUNCTION OF GET SYMBOLS BY QUERY********************** */

    //***********************START OF GET SYMBOLS BY ADDRESS HELPER********************** */

    /**
     * @brief Asynchronously retrieves a symbol by its address and updates the query with the symbol's details.
     * @details This function is called to asynchronously find a symbol by its address. It checks if the symbol has already been found,
     *          attempts to find the symbol by address, and if successful, retrieves various properties of the symbol such as size,
     *          type, and flags. It also checks if the symbol is stored in a register and updates the query accordingly.
     * @param arg A void pointer to Symbol_by_query which contains the address to look for and where results will be stored.
     */
    void get_symbol_by_address_async(void* arg){
        auto curQuery = static_cast<stapinlogic::Symbol_by_query*>(arg);
        auto tcfContext = stapincontext::get_TCF_context(curQuery->pid, curQuery->tid );
        if(curQuery->found || nullptr == tcfContext)
        {
            //TODO write to log
            notify_conditional_variable(curQuery);
            return;
        }
        stapincontext::notify_stop_context(tcfContext);;
        Symbol* symbol = nullptr;
        if(-1 == find_symbol_by_addr( tcfContext, 0, curQuery->symbolSearchAddr, &symbol))
        {
            enumerate_symbols(tcfContext, 0, fill_symbol_info_by_query, curQuery);
        } 
        else
        {
            fill_symbol_info_by_query(curQuery, symbol);
        }
        stapincontext::notify_resume_context(tcfContext);
        notify_conditional_variable(curQuery);
    }

    //***********************END OF GET SYMBOLS BY ADDRESS HELPER********************** */

    //***********************START OF GET SYMBOLS BY REG HELPER********************** */

    /**
     * @brief Asynchronously initiates the process to retrieve a symbol by register.
     * @details This function sets the tcf context to stopped, then posts an event to enumerate symbols and find the symbol by register.
     *          It waits for the callback to complete the symbol retrieval.
     * @param arg A void pointer to Symbol_by_query which contains the register name and where results will be stored.
     */
    void get_symbol_by_reg_async(void* arg){
        auto curQuery = static_cast<stapinlogic::Symbol_by_query*>(arg);
        auto tcfContext = stapincontext::get_TCF_context(curQuery->pid, curQuery->tid );
        if(nullptr == tcfContext)
        {
            //TODO write to log
            notify_conditional_variable(curQuery);
            return;
        }
        stapincontext::notify_stop_context(tcfContext);
        Symbol* symbol = nullptr;
        enumerate_symbols(tcfContext, 0, fill_symbol_info_by_query, curQuery);
        stapincontext::notify_resume_context(tcfContext);
        notify_conditional_variable(curQuery);
        
    }

    //***********************END OF GET SYMBOLS BY REG HELPER********************** */

    //***********************START OF GET SYMBOLS BY ID HELPER********************** */

    /**
     * @brief Asynchronously initiates the process to retrieve a symbol by its unique ID.
     * @details This function sets the tcf context to stopped, then attempts to find a symbol by its unique ID. If found, it invokes a callback to process the symbol.
     * @param arg A void pointer to Symbol_by_query which contains the unique ID and where results will be stored.
     */
    void get_symbol_by_id_async(void* arg){
        auto curQuery = static_cast<stapinlogic::Symbol_by_query*>(arg);
        Symbol* symbol = nullptr;
        auto tcfContext = stapincontext::get_TCF_context(curQuery->pid, curQuery->tid );
        if(nullptr == tcfContext)
        {
            //TODO write to log
            notify_conditional_variable(curQuery);
            return;
        }
        stapincontext::notify_stop_context(tcfContext);;
        if(-1 == id2symbol(curQuery->uniqueID.c_str(), &symbol))
        {
            //TODO write to log
            curQuery->found = false;
            notify_conditional_variable(curQuery);
            return;
        }
        fill_symbol_info_by_query(curQuery, symbol);
        stapincontext::notify_resume_context(tcfContext);
        notify_conditional_variable(curQuery);
    }


    void get_symbol_by_query(stapinlogic::Symbol_by_query* curQuery, GET_SYMBOL_TYPE type)
    {
        std::unique_lock<std::mutex> lock(curQuery->mtx);
        curQuery->ready = false;        
        stapinlogic::push_func_to_event_loop(GET_TYPE_TO_FUNC[type], curQuery);
        curQuery->cv.wait(lock, [&curQuery] { return curQuery->ready; });
    }
    

}//anonymus namespace

namespace symbolslogic
{
    bool remove_symbols_iterator(uintptr_t iteratorID){
        std::lock_guard<std::mutex> mapLock(symbolIteratorMapMutex);
        symbolsandexpressionslogic::symbols_id_to_iterator().erase(iteratorID);
        return true;
    }

    uint16_t symbols_to_RPC_symbol(uint16_t count, stapinlogic::Symbols_iterator* curSymbolsIterator , stapinlogic::Registry* curSymbolsRegistry){
        if(nullptr == curSymbolsIterator || nullptr == curSymbolsRegistry)
        {
            //TODO write to log
            return false;
        }
        uint16_t foundCount = 0;
        for (size_t i = 0; i < count; ++i)
        {        
            if(curSymbolsIterator->symbols.empty()) 
            {
                break;
            }
            auto symbol = curSymbolsIterator->symbols.front();
            auto res = symbols_to_RPC_symbol_wrapper(&symbol, curSymbolsRegistry);
            if(!res)
            {
                break;
            }
            curSymbolsIterator->symbols.pop_front();
            foundCount++;
        }   
        return foundCount; 
    }

    stapinlogic::Symbols_iterator* get_symbols_start(const char* parentUinqueID,  NATIVE_TID pid, NATIVE_TID tid, const char* name){
        std::shared_ptr<stapinlogic::Symbols_iterator> curSymbolIterator(new (std::nothrow) stapinlogic::Symbols_iterator());
        if(nullptr == curSymbolIterator)
        {
            //TODO write to log
            return nullptr;
        }
        std::unique_lock<std::mutex> lock(curSymbolIterator->mtx); 
        curSymbolIterator->pid = pid;
        curSymbolIterator->tid = tid;
        curSymbolIterator->id = reinterpret_cast<uintptr_t>(curSymbolIterator.get());
        curSymbolIterator->parentUinqueID = parentUinqueID? parentUinqueID: "";
        curSymbolIterator->name = name? name: "";
        curSymbolIterator->ready = false;
        if(curSymbolIterator->parentUinqueID.empty())
        {
            stapinlogic::push_func_to_event_loop(get_symbol_list_async_no_parent,curSymbolIterator.get() );
        }
        else
        {
            stapinlogic::push_func_to_event_loop(get_symbol_list_async_with_parent,curSymbolIterator.get() );
        }
        curSymbolIterator->cv.wait(lock, [&curSymbolIterator] { return curSymbolIterator->ready; });
        auto rawPtr = curSymbolIterator.get();
        auto gotInserted = false;
        {
            std::lock_guard<std::mutex> mapLock(symbolIteratorMapMutex);
            auto insertRes = symbolsandexpressionslogic::symbols_id_to_iterator().insert(std::make_pair(curSymbolIterator->id, curSymbolIterator));
            gotInserted = insertRes.second;
        }
        return gotInserted? rawPtr: nullptr;
    }


    void get_symbol_by_address(stapinlogic::Symbol_by_query* curQuery){
        get_symbol_by_query(curQuery, GET_SYMBOL_TYPE::ADDRESS);
    }

    //****************** GET SYMBOLS BY REG ****************** */

    void get_symbol_by_reg(stapinlogic::Symbol_by_query* curQuery){
        get_symbol_by_query(curQuery, GET_SYMBOL_TYPE::REG);
    }

    //****************** GET SYMBOLS BY ID ****************** */

    void get_symbol_by_id(stapinlogic::Symbol_by_query* curQuery){
        get_symbol_by_query(curQuery, GET_SYMBOL_TYPE::ID);
    }

    stapinlogic::Symbols_iterator* get_symbol_iterator_by_id(uintptr_t iteratorID) {
        return symbolsandexpressionslogic::get_iterator_by_id<stapinlogic::Symbols_iterator>(iteratorID, symbolsandexpressionslogic::symbols_id_to_iterator(), symbolIteratorMapMutex);
    }

};

namespace symbolsandexpressionslogic
{
    void fill_symbol_attributes(Symbol *symbol, stapinlogic::Symbol_attributes *symbolAttr)
    {
        if(nullptr == symbol || nullptr == symbolAttr)
        {
            //TODO write to log file
            return;
        }
        char* tempNameBuf = nullptr;
        Symbol* typeSymbol = nullptr;
        Symbol* baseTypeSymbol = nullptr;
        Context* ctxForReg = nullptr;
        int frame = 0;
        RegisterDefinition * reg = nullptr;
        symbolAttr->uniqueID = symbol2id(symbol);
        get_symbol_name(symbol, &tempNameBuf);
        symbolAttr->name = tempNameBuf? tempNameBuf: "";
        if(0 == get_symbol_register(symbol, &ctxForReg, &frame, &reg ))
        {
            if(nullptr != reg && nullptr != reg->name)
            {
                symbolAttr->rpcFlags = stapinlogic::E_rpc_symbol_flags::VALUE_IN_REG;
                symbolAttr->regIndex = stapinlogic::get_reg_index(reg->name);
                symbolAttr->addr = 0;
            }
            else
            {
                symbolAttr->rpcFlags = stapinlogic::E_rpc_symbol_flags::LOCATION_FETCH_ERROR;
            }
            
        }
        else if (0 == get_symbol_address(symbol, &symbolAttr->addr))
        {
            symbolAttr->rpcFlags = stapinlogic::E_rpc_symbol_flags::VALUE_IN_MEM;
            symbolAttr->regIndex = 0;
        }
        else 
        {
            auto locExpressionState = evaluate_symbol_location(symbol);
            symbolAttr->regIndex = 0;
            symbolAttr->addr = 0;
            if(nullptr == locExpressionState || nullptr == locExpressionState->pieces)
            {
                symbolAttr->rpcFlags = stapinlogic::E_rpc_symbol_flags::LOCATION_FETCH_ERROR;
            }
            else if(locExpressionState->pieces->optimized_away)
            {
                symbolAttr->rpcFlags = stapinlogic::E_rpc_symbol_flags::OPTIMIZED_AWAY;
            }
            else if(nullptr != locExpressionState->pieces->value)
            {
                symbolAttr->rpcFlags = stapinlogic::E_rpc_symbol_flags::DW_STACK_VALUE;
            }
            else{
                symbolAttr->rpcFlags = stapinlogic::E_rpc_symbol_flags::IS_BIT_FIELD;
            }
        }
        get_symbol_size(symbol, &symbolAttr->size);
        get_symbol_flags(symbol, &symbolAttr->flags);
        get_symbol_type_class(symbol, &symbolAttr->symbolClass);
        get_symbol_type(symbol, &typeSymbol);
        if(nullptr != typeSymbol)
        {
            symbolAttr->typeUniqueID = symbol2id(typeSymbol);
            if(0 == get_symbol_base_type(typeSymbol, &baseTypeSymbol))
            {
                symbolAttr->baseTypeUniqueID = symbol2id(baseTypeSymbol);
            }
            else
            {
                symbolAttr->baseTypeUniqueID = "";
            }
        }
        //handle type name correctly
        if(symbolAttr->name.empty()               && 
            (is_type_symbol(symbolAttr->flags)            || 
            0 != (symbolAttr->flags & SYM_FLAG_CONST_TYPE)||
            type_name_needed_by_type_class(symbolAttr->symbolClass)))
        {
            symbolsandexpressionslogic::handle_type_name(symbol, symbolAttr);
        }
    }

    void handle_type_name(Symbol *symbol, stapinlogic::Symbol_attributes *symbolAttr)
    {
        std::string finalTypeName = "";
        SYM_FLAGS flags;
        if(TYPE_CLASS_POINTER == symbolAttr->symbolClass || TYPE_CLASS_ARRAY == symbolAttr->symbolClass)
        {
            std::list<std::string> pointerLevels;
            auto baseTypeSymbol = symbol;
            auto symbolClass = symbolAttr->symbolClass;
            while(TYPE_CLASS_POINTER == symbolClass || TYPE_CLASS_ARRAY == symbolClass)
            {
                get_symbol_flags(baseTypeSymbol, &flags);
                if(TYPE_CLASS_POINTER == symbolClass) //POINTER
                {
                    auto pointerLevel= get_pointer_type(flags);
                    append_const_and_volatile_names(flags, pointerLevel, baseTypeSymbol, true);
                    pointerLevels.push_back(pointerLevel); //from right to left 
                }
                else //ARRAY
                {
                    ContextAddress length = 0;
                    auto hasLength = get_symbol_length(baseTypeSymbol, &length) == 0;
                    auto pointerLevel = hasLength? stapinlogic::OPEN_ARRAY + std::to_string(length) + stapinlogic::CLOSE_ARRAY: stapinlogic::OPEN_ARRAY+ stapinlogic::CLOSE_ARRAY;
                    pointerLevels.push_front(pointerLevel); //from left to right 
                }
                if(0 > get_symbol_base_type(baseTypeSymbol, &baseTypeSymbol) ||
                    0 > get_symbol_type_class(baseTypeSymbol, &symbolClass))
                {
                    break;
                }
            }
            // Now handle the base type
            append_type_class_names_to_symbol_type_name(finalTypeName , symbolClass);
            get_symbol_flags(baseTypeSymbol, &flags);
            append_const_and_volatile_names(flags, finalTypeName, baseTypeSymbol, false);
            append_base_type_name(baseTypeSymbol, finalTypeName);
            for(auto it = pointerLevels.rbegin(); it != pointerLevels.rend(); ++it)
            {
                finalTypeName += SPACE + *it;
            }
        }
        else
        {
            Symbol* typeNameSymbol;
            int typeClass;
            auto volatileTypeCheckSymbol = symbol;
            if(0 == id2symbol(symbolAttr->typeUniqueID.c_str(), &typeNameSymbol))
            {
                append_base_type_name(typeNameSymbol, finalTypeName);
            }
            std::string const_and_volatile;
            append_const_and_volatile_names(symbolAttr->flags, const_and_volatile, symbol, false);
            if(0 <= get_symbol_type_class(symbol, &typeClass))
            {
                append_type_class_names_to_symbol_type_name(finalTypeName , typeClass);
            }
            finalTypeName = const_and_volatile + finalTypeName;
        }
        symbolAttr->name = std::move(finalTypeName);
    }

};
