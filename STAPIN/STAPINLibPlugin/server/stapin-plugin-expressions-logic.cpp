/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#include "stapin-plugin-expressions-logic.h"

namespace{
    static std::mutex expressionIteratorMapMutex;
    /**
     * @brief Retriving the iteratorID to iterator map safely.
     */
    stapinlogic::ExpressionsIDToIteratorMap& expressins_id_to_iterator()
    {
        static stapinlogic::ExpressionsIDToIteratorMap expressionsIDToIterator;
        return expressionsIDToIterator;
    }
    
    //Forward decleration
    void choose_action_by_type_class(Value* v, stapinlogic::Expression_iterator* curExpressionIterator,
                                    const std::string& name, const std::string& parentName, int level);
    

    /**
     * @brief Fiils some common expression fields by the arguments. 
     * @param expressionAtrr A pointer to Expression_attributes to fill.
     * @param v A pointer to the Value.
     * @param name name of the expression
     * @param parentName name of parent of expression.
     * @param level level of depth of expression.
     */
    void fill_common_expression_fields(stapinlogic::Expression_attributes* expressionAtrr, const Value* v,  
                                const std::string& name, const std::string& parentName, int level)
    {
        expressionAtrr->parentName = parentName;
        expressionAtrr->level = level;
        expressionAtrr->typeUniqueID = v->type? symbol2id(v->type): "";
        expressionAtrr->symbolUniqueID = nullptr != v->sym? symbol2id(v->sym) : "";
        expressionAtrr->name = name;
        if(0 == expressionAtrr->dataSize)
        {
            expressionAtrr->dataSize = v->size;
        }
    }

    /**
     * @brief Handles different types of expressions and pushes them into the iterator.
     * @tparam VALUE_TYPE The type of the expression value.
     * @param v A pointer to the Value.
     * @param curExpressionIterator A pointer to Expression_iterator to fill.
     * @param name name of the expression.
     * @param parentName name of parent of expression.
     * @param level level of depth of expression.
     * @param expressionType The type of the expression.
     * @param valueConverter A function to convert the value to the desired type.
     */
    template <typename VALUE_TYPE>
    void handle_expression(Value* v, stapinlogic::Expression_iterator* curExpressionIterator,  
                        const std::string& name, const std::string& parentName, int level,
                        stapinlogic::Expression_type expressionType, int (*valueConverter)(Value*, VALUE_TYPE*))
    {
        stapinlogic::Expression_attributes expressionAttr{};
        expressionAttr.data.resize(v->size);
        VALUE_TYPE resValue;
        valueConverter(v, &resValue);
        memcpy(expressionAttr.data.data(), &resValue, v->size);
        expressionAttr.type = expressionType;
        fill_common_expression_fields(&expressionAttr, v, name, parentName, level);
        curExpressionIterator->expressions.push_back(expressionAttr);
    }
    /**
     * @brief Handles signed type expression and pushes it into the iterator.
     * @param v A pointer to the Value.
     * @param curExpressionIterator A pointer to Expression_iterator to fill.
     * @param name name of the expression
     * @param parentName name of parent of expression.
     * @param level level of depth of expression.
     */
    void handle_singed_expression(Value* v, stapinlogic::Expression_iterator* curExpressionIterator,  
                                const std::string&  name, const std::string& parentName, int level)
    {
        handle_expression<int64_t>(v, curExpressionIterator, name, parentName, level, 
                               stapinlogic::Expression_type::SIGNED, value_to_signed);
    }

    /**
     * @brief Handles unsinged type expression and pushes it into the iterator.
     * @param v A pointer to the Value.
     * @param curExpressionIterator A pointer to Expression_iterator to fill.
     * @param name name of the expression
     * @param parentName name of parent of expression.
     * @param level level of depth of expression.
     */
    void handle_unsinged_expression(Value* v, stapinlogic::Expression_iterator* curExpressionIterator,  
                                const std::string&  name, const std::string& parentName, int level)
    {
        handle_expression<uint64_t>(v, curExpressionIterator, name, parentName, level, 
                                stapinlogic::Expression_type::UNSIGNED, value_to_unsigned);
    }

    /**
     * @brief Handles double type expression and pushes it into the iterator.
     * @param v A pointer to the Value.
     * @param curExpressionIterator A pointer to Expression_iterator to fill.
     * @param name name of the expression
     * @param parentName name of parent of expression.
     * @param level level of depth of expression.
     */
    void handle_double_expression(Value* v, stapinlogic::Expression_iterator* curExpressionIterator,  
                                const std::string&  name, const std::string& parentName, int level)
    {
        handle_expression<double>(v, curExpressionIterator, name, parentName, level, 
                              stapinlogic::Expression_type::DOUBLE, value_to_double);
    }

    /**
     * @brief Handles pointer type expression and pushes it into the iterator.
     * @param v A pointer to the Value.
     * @param curExpressionIterator A pointer to Expression_iterator to fill.
     * @param name name of the expression
     * @param parentName name of parent of expression.
     * @param level level of depth of expression.
     */
    void handle_pointer_expression(Value* v, stapinlogic::Expression_iterator* curExpressionIterator,  
                                const std::string&  name, const std::string& parentName, int level)
    {
        handle_expression<ContextAddress>(v, curExpressionIterator, name, parentName, level, 
                                      stapinlogic::Expression_type::POINTER, value_to_address);
    }

    /**
     * @brief Handles member ptr type expression and pushes it into the iterator.
     * @param v A pointer to the Value.
     * @param curExpressionIterator A pointer to Expression_iterator to fill.
     * @param name name of the expression
     * @param parentName name of parent of expression.
     * @param level level of depth of expression.
     */
    void handle_member_ptr_expression(Value* v, stapinlogic::Expression_iterator* curExpressionIterator,  
                                const std::string&  name, const std::string& parentName, int level)
    {
        handle_expression<ContextAddress>(v, curExpressionIterator, name, parentName, level, 
                                      stapinlogic::Expression_type::MEMBER_PTR, value_to_address);
    }

    /**
     * @brief Handles enum type expression and pushes it into the iterator.
     * @param v A pointer to the Value.
     * @param curExpressionIterator A pointer to Expression_iterator to fill.
     * @param name name of the expression
     * @param parentName name of parent of expression.
     * @param level level of depth of expression.
     */
    void handle_enum_expression(Value* v, stapinlogic::Expression_iterator* curExpressionIterator,  
                                const std::string&  name, const std::string& parentName, int level)
    {
        handle_expression<int64_t>(v, curExpressionIterator, name, parentName, level, 
                               stapinlogic::Expression_type::ENUM, value_to_signed);
    }

    /**
     * @brief Handles different types of parent level expressions and pushes them into the iterator.
     * @param v A pointer to the Value.
     * @param curExpressionIterator A pointer to Expression_iterator to fill.
     * @param name name of the expression.
     * @param parentName name of parent of expression.
     * @param level level of depth of expression.
     * @param expressionType The type of the expression.
     * @param lengthGetter A function to get the length of the symbol, if applicable.
     */
    void handle_parent_level_expression(const Value* v, stapinlogic::Expression_iterator* curExpressionIterator,  
                                        const std::string& name, const std::string& parentName, int level,
                                        stapinlogic::Expression_type expressionType, int (*lengthGetter)(const Symbol*, ContextAddress*) = nullptr)
    {
        stapinlogic::Expression_attributes expressionAttr{};
        expressionAttr.type = expressionType;
        if (lengthGetter && v->type)
        {
            ContextAddress length;
            if (0 == lengthGetter(v->type, &length))
            {
                expressionAttr.dataSize = length;
            }
        }
        fill_common_expression_fields(&expressionAttr, v, name, parentName, level);
        curExpressionIterator->expressions.push_back(expressionAttr);
    }

    /**
     * @brief Handles the parent array type expression and pushes it into the iterator.
     * @param v A pointer to the Value.
     * @param curExpressionIterator A pointer to Expression_iterator to fill.
     * @param name name of the expression.
     * @param parentName name of parent of expression.
     * @param level level of depth of expression.
     */
    void handle_array_parent_level(const Value* v, stapinlogic::Expression_iterator* curExpressionIterator,  
                                const std::string& name, const std::string& parentName, int level)
    {
        handle_parent_level_expression(v, curExpressionIterator, name, parentName, level, 
                                    stapinlogic::Expression_type::ARRAY, get_symbol_length);
    }

    /**
     * @brief Handles the parent composite type expression and pushes it into the iterator.
     * @param v A pointer to the Value.
     * @param curExpressionIterator A pointer to Expression_iterator to fill.
     * @param name name of the expression.
     * @param parentName name of parent of expression.
     * @param level level of depth of expression.
     */
    void handle_composite_parent_level(const Value* v, stapinlogic::Expression_iterator* curExpressionIterator,  
                                    const std::string& name, const std::string& parentName, int level)
    {
        handle_parent_level_expression(v, curExpressionIterator, name, parentName, level, 
                                    stapinlogic::Expression_type::CLASS);
    }

    /**
     * @brief Handles the parent function type expression and pushes it into the iterator.
     * @param v A pointer to the Value.
     * @param curExpressionIterator A pointer to Expression_iterator to fill.
     * @param name name of the expression.
     * @param parentName name of parent of expression.
     * @param level level of depth of expression.
     */
    void handle_function_parent_level(const Value* v, stapinlogic::Expression_iterator* curExpressionIterator,  
                                    const std::string& name, const std::string& parentName, int level)
    {
        handle_parent_level_expression(v, curExpressionIterator, name, parentName, level, 
                                    stapinlogic::Expression_type::FUNCTION);
    }

    /**
     * @brief Function to get all the elements from composite type expression .
     * @details This function goes over the symbol chilldren, makes an expression from each one and evaluates it. 
     * Then, it chooses the action by the result of the new evaluation.
     * @param curExpressionIterator A pointer to Expression_iterator to fill.
     * @param sym A pointer to Symbol of the expression.
     * @param level level of depth of expression.
     * @param v A pointer to the Value.
     * @param parentName name of parent of expression.
     */
    void get_all_composite_elements(stapinlogic::Expression_iterator* curExpressionIterator, const Symbol *sym, 
                                    int level, const Value* v, const std::string& parentName)
    {
        if(nullptr == sym)
        {
            //TODO write to log
            return;
        }
        char* childName = nullptr;
        Symbol** children;
        int count = 0;
        get_symbol_children(sym, &children, &count);
        for (size_t i = 0; i < count; ++i)
        {
            Value newVal;         
            if(0 > get_symbol_name(children[i], &childName) || nullptr == childName)
            {
                continue;
            }
            auto name = parentName + '.' + childName;
            if (0 > evaluate_expression(v->ctx, 0, 0, name.data(), 0, &newVal))
            {
                continue;
            } 
            choose_action_by_type_class(&newVal, curExpressionIterator, name, parentName, level);
        }
    }

    /**
     * @brief Function to convert TCF type class to expression type.
     * @param symbolType The TCF type class .
     * @return the result converted Expression_type.
     */
    stapinlogic::Expression_type type_class_to_expression_type(int symbolType)
    {
        switch (symbolType)
        {
            case TYPE_CLASS_COMPOSITE:
                return stapinlogic::Expression_type::CLASS;
            case TYPE_CLASS_FUNCTION:
                return stapinlogic::Expression_type::FUNCTION;
            case TYPE_CLASS_POINTER:
                return stapinlogic::Expression_type::POINTER;
            case TYPE_CLASS_ARRAY:
                return stapinlogic::Expression_type::ARRAY;
            case TYPE_CLASS_REAL:
                return stapinlogic::Expression_type::DOUBLE;
            case TYPE_CLASS_INTEGER:
                return stapinlogic::Expression_type::SIGNED;
            case TYPE_CLASS_CARDINAL:
                return stapinlogic::Expression_type::UNSIGNED;
            case TYPE_CLASS_ENUMERATION:
                return stapinlogic::Expression_type::ENUM;  
            case  TYPE_CLASS_MEMBER_PTR : 
                return stapinlogic::Expression_type::MEMBER_PTR;  
            case  TYPE_CLASS_COMPLEX:     //TBD
            default:
                return stapinlogic::Expression_type::UNKNOWN;
                break;
        }
    }

    /**
     * @brief Function to Handle single function type element.
     * @param curExpressionIterator A pointer to Expression_iterator.
     * @param sym the relevant symbol.
     * @param level the current level of expression
     */
    void handle_function_element(stapinlogic::Expression_iterator* curExpressionIterator, Symbol* sym, int level)
    {
        stapinlogic::Symbol_attributes childSymAttr{0};
        stapinlogic::Expression_attributes expressionAttr{};
        symbolsandexpressionslogic::fill_symbol_attributes(sym, &childSymAttr);
        symbolsandexpressionslogic::handle_type_name(sym, &childSymAttr);
        expressionAttr.dataSize = childSymAttr.size;
        expressionAttr.type = type_class_to_expression_type(childSymAttr.symbolClass);
        expressionAttr.parentName = curExpressionIterator->expression;
        expressionAttr.level = level;
        expressionAttr.typeUniqueID = std::move(childSymAttr.typeUniqueID);
        expressionAttr.name = std::move(childSymAttr.name);
        curExpressionIterator->expressions.push_back(expressionAttr);
    }

    /**
     * @brief Function to get all the elements from function type expression .
     * @details This function goes over the symbol children (each one is an argument of the function) and handle it.
     * @param curExpressionIterator A pointer to Expression_iterator to fill.
     * @param sym A pointer to Symbol of the expression.
     * @param level level of depth of expression.
     * @param v A pointer to the Value.
     * @param parentName name of parent of expression.
     */
    void get_all_function_elements(stapinlogic::Expression_iterator* curExpressionIterator, const Symbol *sym, 
                                int level, Value* v, const std::string& parentName)
    {
        if(nullptr == sym)
        {
            //TODO write to log
            return;
        }
        Symbol** children;
        int count;
        get_symbol_children(sym, &children, &count);
        for (size_t i = 0; i < count; ++i)
        {
            handle_function_element(curExpressionIterator, children[i], level);
        }
    }  

    /**
     * @brief Function to get all the elements from array type expression .
     * @details This function goes over the array length to find each entry of it, makes an expression from it and evaluates it. 
     * Then, it chooses the action by the result of the evaluation.
     * @param curExpressionIterator A pointer to Expression_iterator to fill.
     * @param sym A pointer to Symbol of the expression.
     * @param level level of depth of expression.
     * @param v A pointer to the Value.
     * @param parentName name of parent of expression.
     */
    void get_all_array_elements(stapinlogic::Expression_iterator* curExpressionIterator, const Symbol *sym, 
                                int level, Value* v, const std::string& parentName) {
        Symbol *baseType = NULL;
        ContextAddress length = 0;
        if(nullptr != sym && 0 > get_symbol_length(sym, &length))
        {
            //could not retrive elements!
            //TODO write to log
            return;
        }
        for (ContextAddress i = 0; i < length; ++i) 
        {
            Value newVal;
            auto name = parentName + stapinlogic::OPEN_ARRAY + std::to_string(i) + stapinlogic::CLOSE_ARRAY;
            if (0 > evaluate_expression(v->ctx, 0, 0, name.data(), 0, &newVal)) 
            {
                continue;
            }
            choose_action_by_type_class(&newVal, curExpressionIterator, name, parentName, level);
        }
    }

    /**
     * @brief Function to get all the elements from composite type expression .
     * @param v A pointer to the Value.
     * @param curExpressionIterator A pointer to Expression_iterator to fill.
     * @param name name of  of expression.
     * @param parentName name of parent of expression.
     * @param level level of depth of expression.
     * @param baseType A pointer to Symbol of the base type of the expression.
     */
    void choose_action_by_type_class(Value* v, stapinlogic::Expression_iterator* curExpressionIterator,
                                    const std::string& name, const std::string& parentName, int level)
    {
        int typeClass;
        if(0 > get_symbol_type_class(v->type, &typeClass))
        {
            return;
        }
        switch (typeClass)
        {
            case TYPE_CLASS_CARDINAL:
                {
                    handle_singed_expression(v, curExpressionIterator, name, parentName, level);
                    break;
                }
            case TYPE_CLASS_INTEGER:
                {
                    handle_unsinged_expression(v, curExpressionIterator, name, parentName, level);
                    break;
                }
            case TYPE_CLASS_REAL:
                {
                    handle_double_expression(v, curExpressionIterator, name, parentName, level);
                    break;
                }
            case TYPE_CLASS_POINTER:
                {
                    handle_pointer_expression(v, curExpressionIterator, name, parentName, level);
                    break;
                }
            case TYPE_CLASS_ARRAY:
                {
                    handle_array_parent_level(v, curExpressionIterator, name, parentName, level);
                    get_all_array_elements(curExpressionIterator, v->type, level+1, v, name);
                    break;
                }
            case TYPE_CLASS_COMPOSITE:
                {
                    handle_composite_parent_level(v, curExpressionIterator, name, parentName, level);
                    get_all_composite_elements(curExpressionIterator, v->type, level+1, v, name);
                    break;
                }
            case TYPE_CLASS_ENUMERATION:
                {
                    handle_enum_expression(v, curExpressionIterator, name, parentName, level);
                    break;
                }
            case TYPE_CLASS_FUNCTION:
                {
                    handle_function_parent_level(v, curExpressionIterator, name, parentName, level);
                    get_all_function_elements(curExpressionIterator, v->type, level+1, v, name);
                    break;
                }
            case TYPE_CLASS_MEMBER_PTR:
                {
                    handle_member_ptr_expression(v, curExpressionIterator, name, parentName, level);
                    break;
                }
            case TYPE_CLASS_COMPLEX:
            default:
                break;
        }
    }


    /**
     * @brief Function to get the expression evaluation.
     * @param arg A void* which will be casted to Expression_iterator.
     */
    void get_expression_evaluation(void* arg){
        auto curExpressionIterator = static_cast<stapinlogic::Expression_iterator*>(arg);
        auto tcfContext = stapincontext::get_TCF_context(curExpressionIterator->pid, curExpressionIterator->tid );
        if(nullptr == tcfContext)
        {
            //TODO write to log
            notify_conditional_variable(curExpressionIterator);
            return;
        }
        stapincontext::notify_stop_context(tcfContext);
        Value v;
        if (0 <= evaluate_expression(tcfContext, 0, 0, curExpressionIterator->expression.data(), 0, &v)) 
        {
            choose_action_by_type_class(&v, curExpressionIterator, curExpressionIterator->expression, "", 0);
        }
        stapincontext::notify_resume_context(tcfContext);
        notify_conditional_variable(curExpressionIterator);
    }

    /**
     * @brief Function to fill common attributes in expression.
     * @param rpcExpression the rpc expression to fill.
     * @param expressionAttr The expression attributes to fill.
     */
    void fill_common_attributes_in_expression(stapin::Rpc_expression* rpcExpression, const stapinlogic::Expression_attributes* expressionAttr, stapinlogic::Registry* curExpressionRegistry)
    {
        rpcExpression->level = expressionAttr->level;
        rpcExpression->dataSize = expressionAttr->dataSize;
        rpcExpression->type = (int)expressionAttr->type;
        rpcExpression->isValue = false;
        if(0 < expressionAttr->data.size())
        {
            rpcExpression->isValue = true;
            memcpy(curExpressionRegistry->expressionsDataBuffer.get() + curExpressionRegistry->curDataIndex, expressionAttr->data.data(), expressionAttr->data.size());
            rpcExpression->dataOffset = curExpressionRegistry->curDataIndex;
            curExpressionRegistry->curDataIndex += expressionAttr->data.size();
            // memcpy(rpcExpression->data, expressionAttr->data.data(), expressionAttr->data.size());
        }
    }

    /**
     * @brief Converts expression data to an RPC-compatible format and stores it in a Expression registry.
     * @details This function checks for new unique names and IDs, copies them into buffers, and updates
     *          indices and offsets. It handles the conversion of symbol flags and registration details.
     *          It ensures that there is enough capacity in the RPC symbols array before adding new entries.
     * @param expressionAttr A pointer to Expression_attributes containing the expressions's details.
     * @param curExpressionRegistry A pointer to Registry where the RPC-compatible expression data is stored.
     * @return A boolean indicating success (true) or failure (false) of the operation.
     */
    bool expressions_to_RPC_expressions_wrapper(const stapinlogic::Expression_attributes* expressionAttr, stapinlogic::Registry* curExpressionRegistry ){
        // Ensure there is enough capacity in the RPCexpressions array
        auto rpcExpressionsSize = sizeof(stapin::Rpc_expression) + expressionAttr->data.size();
        if (curExpressionRegistry->curIndex + rpcExpressionsSize > curExpressionRegistry->rpcItemsArraySize) 
        {
            //TODO write to log
            return false;
        }
        auto rpcExpression = (stapin::Rpc_expression*)(curExpressionRegistry->rpcItemsArray.get() + curExpressionRegistry->curIndex);
        auto nameSearch = curExpressionRegistry->name2index.end();
        auto parentNameSearch = curExpressionRegistry->name2index.end();
        auto typeUniqueIDSearch =  curExpressionRegistry->uniqueID2index.end();
        auto symbolUniqueIDSearch = curExpressionRegistry->uniqueID2index.end();
        /** filling the arrays if needed */
        {
            std::lock_guard<std::mutex> lock(curExpressionRegistry->mtx);
            nameSearch = curExpressionRegistry->name2index.find(expressionAttr->name);
            parentNameSearch = curExpressionRegistry->name2index.find(expressionAttr->parentName);
            typeUniqueIDSearch =  curExpressionRegistry->uniqueID2index.find(expressionAttr->typeUniqueID);
            symbolUniqueIDSearch = curExpressionRegistry->uniqueID2index.find(expressionAttr->symbolUniqueID);
        }
        //NAMES
        if(curExpressionRegistry->name2index.end() == nameSearch)
        { //new name! 
            symbolsandexpressionslogic::insert_name_or_ID_to_registry_buffer(expressionAttr->name, curExpressionRegistry, true, false);
        } 
        //PARENT NAMES
        if(curExpressionRegistry->name2index.end() == parentNameSearch)
        { //new name! 
            symbolsandexpressionslogic::insert_name_or_ID_to_registry_buffer(expressionAttr->parentName, curExpressionRegistry, true, false);
        } 
        //TYPE UNIQUE IDS
        if( curExpressionRegistry->uniqueID2index.end() == typeUniqueIDSearch)
        { // new unique ID! 
            symbolsandexpressionslogic::insert_name_or_ID_to_registry_buffer(expressionAttr->typeUniqueID, curExpressionRegistry, false, false);
        }
        //SYMBOL UNIQUE ID
        if( curExpressionRegistry->uniqueID2index.end() == symbolUniqueIDSearch)
        { // new unique ID! 
            symbolsandexpressionslogic::insert_name_or_ID_to_registry_buffer(expressionAttr->symbolUniqueID, curExpressionRegistry, false, false);
        }
        fill_common_attributes_in_expression(rpcExpression, expressionAttr, curExpressionRegistry);
        {
            std::lock_guard<std::mutex> lock(curExpressionRegistry->mtx);
            rpcExpression->expressionNameOffset = curExpressionRegistry->name2index[expressionAttr->name];     
            rpcExpression->parentExpressionNameOffset =  curExpressionRegistry->name2index[expressionAttr->parentName]; 
            rpcExpression->typeUniqueIDOffset = curExpressionRegistry->uniqueID2index[expressionAttr->typeUniqueID];
            rpcExpression->symbolUniqueIDoffset = curExpressionRegistry->uniqueID2index[expressionAttr->symbolUniqueID];
        }
        curExpressionRegistry->curIndex += rpcExpressionsSize;
        return true;
    }
}//anonymus namespace

namespace expressionslogic{
    stapinlogic::Expression_iterator* get_expressions_iterator_by_id(uintptr_t iteratorID) {
        return symbolsandexpressionslogic::get_iterator_by_id<stapinlogic::Expression_iterator>(iteratorID, expressins_id_to_iterator(), expressionIteratorMapMutex);
    }

    stapinlogic::Expression_iterator* get_expressions_start(NATIVE_TID pid, NATIVE_TID tid, const std::string& expression)
    {
        std::shared_ptr<stapinlogic::Expression_iterator> curExpressionIterator(new (std::nothrow) stapinlogic::Expression_iterator());
        if(nullptr == curExpressionIterator)
        {
            //TODO write to log
            return nullptr;
        }
        std::unique_lock<std::mutex> lock(curExpressionIterator->mtx); 
        curExpressionIterator->id = reinterpret_cast<uintptr_t>(curExpressionIterator.get());
        curExpressionIterator->pid = pid;
        curExpressionIterator->tid = tid;
        curExpressionIterator->expression = expression;
        curExpressionIterator->ready = false;
        stapinlogic::push_func_to_event_loop(get_expression_evaluation ,curExpressionIterator.get());
        curExpressionIterator->cv.wait(lock, [&curExpressionIterator] { return curExpressionIterator->ready; });
        auto rawPtr = curExpressionIterator.get();
        auto gotInserted = false;
        {
            std::lock_guard<std::mutex> mapLock(expressionIteratorMapMutex);
            auto insertRes = expressins_id_to_iterator().insert(std::make_pair(curExpressionIterator->id, curExpressionIterator));
            gotInserted = insertRes.second;
        }
        return gotInserted ? rawPtr: nullptr;
    }


    uint16_t expressions_to_RPC_expressions(uint16_t count, stapinlogic::Expression_iterator* curExpressionsIterator ,  stapinlogic::Registry* curExpressionsRegistry)
    {
        if(nullptr == curExpressionsIterator || nullptr == curExpressionsRegistry)
        {
            //TODO write to log
            return false;
        }
        uint16_t foundCount = 0;
        for (size_t i = 0; i < count; ++i)
        {        
            if(curExpressionsIterator->expressions.empty()) 
            {
                break;
            }
            auto& expression = curExpressionsIterator->expressions.front();
            auto res = expressions_to_RPC_expressions_wrapper(&expression, curExpressionsRegistry);
            if(!res)
            {
                break;
            }
            curExpressionsIterator->expressions.pop_front();
            foundCount++;
        }   
        return foundCount; 
    }

    bool remove_expressions_iterator(uintptr_t iteratorID){
        std::lock_guard<std::mutex> lock(expressionIteratorMapMutex);
        expressins_id_to_iterator().erase(iteratorID);
        return true;
    }
};
