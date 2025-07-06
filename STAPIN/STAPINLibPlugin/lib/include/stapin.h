/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 * @file
 * Main header for STAPIN Symbol Table API for Pin Library
 *
 * @defgroup STAPIN STAPIN APIs
 */

#ifndef _STAPIN_H_
#define _STAPIN_H_

#include <pin.H>

#include "enum_utils.h"

#include <vector>

/**
 * @brief STAPIN namespace
 * 
 */
namespace stapin
{

    constexpr size_t EXPRESSION_DATA_BYTES = 64;

    /** @ingroup STAPIN
     * @brief Context update method for @ref set_context
     * 
     */
    enum class E_context_update_method
    {
        FULL,   //!< The entire CPU context including floating point state and registers is set
        FAST,   //!< Only general purpose registers, IP, SP, and flags are set
        IP_ONLY //!< Only the Instruction Pointer (IP) is set
    };

    /** @ingroup STAPIN
     * @brief Symbol type
     * 
     */
    enum class E_symbol_type
    {
        UNKNOWN,  //!< The symbol type is unknown.
        CLASS,    //!< The symbol represents a class.
        FUNCTION, //!< The symbol represents a function.
        INTEGER,  //!< The symbol represents an integer.
        POINTER,  //!< The symbol represents a pointer.
        REAL,     //!< The symbol represents a real number (floating point).
        ARRAY,    //!< The symbol represents an array.
        VALUE     //!< The symbol represents a value.
    };

    /** @ingroup STAPIN
     * @brief Symbol flags
     * 
     */
    enum class E_symbol_flags
    {
        LOCATION_FETCH_ERROR = 0, //!< There was an error fetching the location of the symbol.
        VALUE_IN_MEM         = 1, //!< The value of the symbol is stored in memory.
        VALUE_IN_REG         = 2, //!< The value of the symbol is stored in a register.
        OPTIMIZED_AWAY       = 3, //!< The symbol has been optimized away.
        SYMBOL_IS_TYPE       = 4, //!< The symbol represents a type.
        DW_STACK_VALUE       = 5, //!< The symbol is a stack value.
        IS_BIT_FIELD         = 6  //!< The symbol is a bit field.
    };

    /** @ingroup STAPIN
     * @brief Source location information
     * 
     */
    struct Source_loc
    {
        const char* srcFileName; //!< The name of the source file.
        ADDRINT startAddress;    //!< The starting address of the source location.
        int32_t startLine;       //!< The starting line number of the source location.
        int32_t startColumn;     //!< The starting column number of the source location.
        ADDRINT endAddress;      //!< The ending address of the source location.
        int32_t endLine;         //!< The ending line number of the source location.
        int32_t endColumn;       //!< The ending column number of the source location.
        ADDRINT nextAddress;     //!< The address of the next source location.
        ADDRINT nextStmtAddress; //!< The address of the next statement.
    };

    /** @ingroup STAPIN
     * @brief Symbol information
     * 
     */
    struct Symbol
    {
        const char* name;                //!< The name of the symbol.
        const char* uniqueId;            //!< A unique identifier for the symbol.
        const char* typeUniqueId;        //!< A unique identifier for the symbol's type. If not applicable, it is an empty string (not null).
        const char* baseTypeUniqueId;    //!< A unique identifier for the symbol's base type. If not applicable, it is an empty string (not null).
        E_symbol_type type;              //!< The type of the symbol, relating to its data type.
        size_t size;                     
        ADDRINT memory;                  //!< The memory address of the symbol if it resides in memory. If the symbol is in a register, this is the index for the register.
        REG reg;                         //!< The register where the symbol is stored if it resides in a register. If not, it is null.
        E_symbol_flags flags;            //!< Flags that relate to the memory placement of the symbol.
    };

    /** @ingroup STAPIN
     * @brief Symbol iterator
     * @tparam VALUE_TYPE the type of the values in the iterator
     */
    template <typename VALUE_TYPE>
    struct STAPIN_iterator;

    using Symbol_iterator = STAPIN_iterator<Symbol>;

    /** @ingroup STAPIN
     * @brief   Expression type flags
     *
     */
    enum class E_expression_type
    {
        UNKNOWN, //!< The expression type is unknown.
        UNSIGNED,//!< The expression represents an unsigned value.
        SIGNED,  //!< The expression represents a signed value.
        DOUBLE,  //!< The expression represents a double precision floating point value.
        CLASS,   //!< The expression represents a class.
        FUNCTION,//!< The expression represents a function.
        POINTER, //!< The expression represents a pointer.
        ARRAY,   //!< The expression represents an array.
        ENUM,    //!< The expression represents an enumeration.
    };

    /** @ingroup STAPIN
     * @brief Expression information
     *
     */
    struct Expression
    {
        const char* name;                //!< The name of the expression (the expression itself).
        const char* typeUniqueID;        //!< A unique identifier for the expression's type. If not applicable, it is an empty string (not null).
        const char* parentName;          //!< The name of the parent expression. If there is no parent, it is an empty string (not null).
        const char* symbolUniqueID;      //!< A unique identifier for the symbol representing the expression. If not applicable, it is an empty string (not null).
        E_expression_type typeFlag;      //!< The type flag of the expression.
        int level;                       //!< The level of the field: 0 indicates the main parent, higher values indicate deeper nested fields.
        uint16_t size;                   
        uint8_t data[EXPRESSION_DATA_BYTES];                //!< The data of the expression, up to EXPRESSION_DATA_BYTES bytes.
    };

    using Expression_iterator = STAPIN_iterator<Expression>;

    /** @ingroup STAPIN
     * @brief Initialize STAPIN.
     * 
     * To use STAPIN this function must be called after a call to PIN_Init and before a call to PIN_StartProgram.
     * 
     * @return true if STAPIN was successfully initialized, false otherwise. 
     *         If STAPIN initialization failed any call to STAPIN API will fail.
     */
    bool init();

    /** @ingroup STAPIN
     * @brief Notify STAPIN of an image load event. 
     * 
     * This function must be called from an Image Load Callback for every image the caller wishes to inspect using STAPIN.
     * 
     * @param[in] img The image of interest
     * @return true If the iamge was correctly loaded by STAPIN, false otherwise.
     *         If this function fails for a particular image then trying to use STAPIN to inspect code from
     *         that image will fail.
     */
    bool notify_image_load(IMG img);

    /** @ingroup STAPIN
     * @brief Notify STAPIN of an image unload event
     * 
     * This function must be called from an Image Unload Callback for every image registered with @ref notify_image_load.
     * 
     * @param[in] img  The image of interest
     * @return true If the image was successfully unregistered.
     * @return false If the image was not previously registered with STAPIN.
     */
    bool notify_image_unload(IMG img);

    /** @ingroup STAPIN
     * @brief Notify STAPIN of a new application thread.
     * 
     * This function must be called from a Thread Start Callback for every thread the caller wishes to use STAPIN
     * to inspect running code in the given thread context.
     * 
     * @param[in] ctx       Pin'a context at thread start. The context will be set for the current thread as by a call
     *                      to @ref set_context with @ref E_context_update_method::FULL.
     * @param[in] threadId  Pin's thread Id for the new thread
     * @return true If thread registration succeeded, false otherwise.
     *         If thread registration failed for a given thread then further calls to STAPIN for that thread will fail.
     */
    bool notify_thread_start(const CONTEXT* ctx, THREADID threadId);

    /** @ingroup STAPIN
     * @brief Notify STAPIN of application thread exit
     * 
     * This function mst be called from a Thread Fini Callback for every thread registered with @ref notify_thread_start.
     * 
     * @param[in] threadId The thread Id for the exiting thread.
     * @return true If the thread was successfully unregistered.
     * @return false If the thread was not previously registered with STAPIN.
     */
    bool notify_thread_fini(THREADID threadId);

    /** @ingroup STAPIN
     * @brief Notify STAPIN that fork occurred
     * 
     * @param[in] ctx       Pin'a context after fork in child. The context will be set for the current thread as by a call
     *                      to @ref set_context with @ref E_context_update_method::FULL.
     * @param[in] threadId  Pin's thread Id for child after fork
     * @return true If child registration succeeded, false otherwise.
     *         If child process registration failed for a given child then further calls to STAPIN for that child will fail.
     */
    bool notify_fork_in_child(const CONTEXT* ctx, THREADID threadId);

    /** @ingroup STAPIN
     * @brief Set the context for future STAPIN queries.
     * 
     * The given context will be used for the current thread until the next call to set_context.
     * 
     * @param[in] ctx                  The Pin context under which STAPIN should operate for the current thread. 
     * @param[in] contextUpdateMethod  The context update method. The default is @ref E_context_update_method::FULL.
     *                                 
     * @return true If the context for the current thread was set successfully, false otherwise.
     *         If the function failed, then the last context successfully set will be used. 
     */
    bool set_context(const CONTEXT* ctx, E_context_update_method contextUpdateMethod = E_context_update_method::FULL);

    /** @ingroup STAPIN
     * @brief Get an array of source locations for the given address range.
     * 
     * @param[in] startAddress  The start of the address range to get source location information for.
     * @param[in] endAddress    The end of the address range to get source location information for.
     * @param[in] locations     A pointer to an array that can receive the source location information for
     *                          the given address range.
     * @param[in] locCount      The number of entries in the array. This is the maximum number of locations
     *                          that will be returned in the array even if there are more locations for the given
     *                          address range.
     * @return The number of source locations filled inside \a locations. If this value is \c 0, it means that no
     *         source code locations were found for the given address.
     * 
     * @note With optimized code, it is possible that multiple source locations
     *       map to this same address even if \a startAddress is equal to \a endAddress.
     */
    size_t get_source_locations(ADDRINT startAddress, ADDRINT endAddress, Source_loc* locations, size_t locCount);

    /** @ingroup STAPIN
     * @brief Get an iterator that can be used to go over symbols visible in the current context.
     * 
     * @param[in] parent This argument can be used to specify a symbol scope. For instance if parent is a symbol of a function
     *                   then the iterator will allow iterating over the function arguments and return type (first child is return type, arguments follow in left to right order). 
     *                   If the symbol belongs to a structure or a class, the iterator will allow iterating members. If \c nullptr is passed then an iterator to the
     *                   symbols visible in the current context will be returned. The default is \c nullptr.
     * @return A symbol iterator or \c nullptr if an error occurred.
     */
    Symbol_iterator* get_symbols(const Symbol* parent = nullptr);

    /** @ingroup STAPIN
     * @brief Reset the iterator to the beginning
     * 
     * @param[in] iterator The symbol iterator to reset.
     * 
     */
    void reset_symbols_iterator(Symbol_iterator* iterator);

    /** @ingroup STAPIN
     * @brief Get an iterator that can be used to go over symbols with the same name visible from the current context.
     * 
     * @param[in] symbolName The symbol name to search for in the current context and \a parent scope.
     * @param[in] parent This argument can be used to specify a symbol scope. For instance if parent is a symbol of a function
     *                   then the iterator will allow iterating over the function arguments and return type (first child is return type,  arguments follow in left to right order). 
     *                   If the symbol belongs to a structure or a class, the iterator will allow iterating members. If \c nullptr is passed then an iterator to the
     *                   symbols visible in the current context will be returned. The default is \c nullptr.
     * 
     * @return A symbol iterator or \c nullptr if an error occurred.
     */
    Symbol_iterator* find_symbol_by_name(const char* symbolName, const Symbol* parent = nullptr);

    /** @ingroup STAPIN
     * @brief Get the next symbol object from a symbol iterator
     * 
     * @param[in]  iterator The symbol iterator from which to bring the next symbol.
     * @param[out] symbol   The next symbol information.
     * @return true  If the next symbol was retrieved.
     * @return false If there are no more symbols in the iterator or an error occurred.
     */
    bool get_next_symbol(Symbol_iterator* iterator, Symbol* symbol);

    /** @ingroup STAPIN
     * @brief Close and release a symbol iterator opened by @ref get_symbols or @ref find_symbol_by_name.
     * 
     * @param[in] iterator The symbol iterator to close and release resources for.
     */
    void close_symbol_iterator(Symbol_iterator* iterator);

    /** @ingroup STAPIN
     * @brief Fill the symbol information for the symbol belonging to a given address.
     * 
     * @param[in] address The address of the symbol in the current context.
     * @param[out] symbol The symbol belonging to this address.
     * @return true If symbol information was retrieved, false otherwise.
     * 
     * @note Not all symbols may have an address associated with them. For instance a symbol may reside in a register for
     *       a given context. See @ref get_symbol_by_reg.
     */
    bool get_symbol_by_address(ADDRINT address, Symbol* symbol);

    /** @ingroup STAPIN
     * @brief Fill the symbol information for the symbol belonging to a given register.
     * 
     * @param[in]  reg    The register of the symbol in the current context.
     * @param[out] symbol The symbol belonging to this register.
     * @return true If symbol information was retrieved, false otherwise. 
     * 
     * \sa get_symbol_by_address
     */
    bool get_symbol_by_reg(REG reg, Symbol* symbol);

    /** @ingroup STAPIN
     * @brief Fill the symbol information for the symbol belonging to a given unique Id.
     * 
     * @param[in]  uniqueId    The unique Id of the symbol.
     * @param[out] symbol      The symbol belonging to this register.
     * @return true If symbol information was retrieved, false otherwise. 
     * 
     */
    bool get_symbol_by_id(const char* uniqueId, Symbol* symbol);

    /** @ingroup STAPIN
     * @brief Fill the symbol information for the symbol belonging to a given routine specified by a Pin RTN.
     * 
     * @param[in]  rtn      The routine (RTN) the symbol in the current context.
     * @param[out] symbol   The symbol belonging to this routine.
     * @return true If symbol information was retrieved, false otherwise.
     */
    bool get_rtn_symbol(RTN rtn, Symbol* symbol);

    /** @ingroup STAPIN
     * @brief Get an iterator that can be used to go over expressions that got evaluated.
     * @param[in] expression This argument is the expression to be evaluated.
     * @return An expression iterator or \c nullptr if an error occurred.
     */
    Expression_iterator* evaluate_expression(const std::string& expression);

    /** @ingroup STAPIN
     * @brief Reset the iterator to the beginning
     * @param[in] iterator The expression iterator to reset.
     */
    void reset_expressions_iterator(Expression_iterator* iterator);

       /** @ingroup STAPIN
     * @brief Get the next expression object from an expression iterator
     * @param[in]  iterator The expression iterator from which to bring the next expression.
     * @param[out] expression   The next expression information.
     * @return true  If the next expression was retrieved.
     * @return false If there are no more expressions in the iterator or an error occurred.
     */
    bool get_next_expression(Expression_iterator* iterator, Expression* expression);

    /** @ingroup STAPIN
     * @brief Close and release an expression iterator opened by evaluate_expression.
     * @param[in] iterator The expression iterator to close and release resources for.
     */
    void close_expression_iterator(Expression_iterator* iterator);


} // namespace stapin

MAKE_ENUM_FLAG(stapin::E_symbol_flags);

#endif // _STAPIN_H_