/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include <stdint.h>
#include <iostream>

void referenceCheck(int& referenceVar, int&& rValReferenceVal){
    std::cout << "The reference value is " << referenceVar << ", and the Rvalue reference is " << rValReferenceVal << std::endl;
}

int main()
{
    struct mySrtuct{
        int innerField;
    };
    mySrtuct structVar;
    structVar.innerField = 3;


    // 1. char* const ptr1; 
    // ptr1 is a constant pointer to a char
    char* const ptr1 = nullptr;

    // 2. const char* ptr2; 
    // ptr2 is a pointer to a constant char
    const char* ptr2 = nullptr;

    // 3. const char* const ptr3; 
    // ptr3 is a constant pointer to a constant char
    const char* const ptr3 = nullptr;

    // 4. char const* ptr4; 
    // ptr4 is a pointer to a constant char (same as const char*)
    char const* ptr4 = nullptr;

    // 5. char** const ptr5; 
    // ptr5 is a constant pointer to a pointer to a char
    char** const ptr5 = nullptr;

    // 6. const char** ptr6; 
    // ptr6 is a pointer to a pointer to a constant char
    const char** ptr6 = nullptr;

    // 7. char* const* ptr7; 
    // ptr7 is a pointer to a constant pointer to a char
    char* const* ptr7 = nullptr;

    // 8. const char* const* ptr8; 
    // ptr8 is a pointer to a constant pointer to a constant char
    const char* const* ptr8 = nullptr;

    // 9. char*** const ptr9; 
    // ptr9 is a constant pointer to a pointer to a pointer to a char
    char*** const ptr9 = nullptr;

    // 10. const char*** ptr10; 
    // ptr10 is a pointer to a pointer to a pointer to a constant char
    const char*** ptr10 = nullptr;

    // 11. char** const* ptr11; 
    // ptr11 is a pointer to a constant pointer to a pointer to a char
    char** const* ptr11 = nullptr;

    // 12. const char** const* ptr12; 
    // ptr12 is a pointer to a constant pointer to a constant pointer to a char
    const char** const* ptr12 = nullptr;

    // 13. char* const* const ptr13; 
    // ptr13 is a constant pointer to a constant pointer to a char
    char* const* const ptr13 = nullptr;

    // 14. const char* const* const ptr14; 
    // ptr14 is a constant pointer to a constant pointer to a constant char
    const char* const* const ptr14 = nullptr;

    // 15. char**** const ptr15; 
    // ptr15 is a constant pointer to a pointer to a pointer to a pointer to a char
    char**** const ptr15 = nullptr;

    // 16. const char**** ptr16; 
    // ptr16 is a pointer to a pointer to a pointer to a pointer to a constant char
    const char**** ptr16 = nullptr;

    // 17. char*** const* ptr17; 
    // ptr17 is a pointer to a constant pointer to a pointer to a pointer to a char
    char*** const* ptr17 = nullptr;

    // 18. const char*** const* ptr18; 
    // ptr18 is a pointer to a constant pointer to a constant pointer to a pointer to a char
    const char*** const* ptr18 = nullptr;

    // 19. char** const* const ptr19; 
    // ptr19 is a constant pointer to a constant pointer to a pointer to a char
    char** const* const ptr19 = nullptr;

    // 20. const char** const* const ptr20; 
    // ptr20 is a constant pointer to a constantpointer to a constant pointer to a char
    const char** const* const ptr20 = nullptr;

    const int constInt = 0;

    char* charPtrVariable = nullptr;

    int intVariable = 0;

    double doubleVariable = 0.0;

    uint64_t uInt64Variable = 0;

    const int boolVariable = 0;

    int32_t* int32PtrVariable = nullptr;

    int intArr[7] = {0};

    const int constIntArr[13] = {0};

    char** charPtrPtrArr[2][2][3] = {{{nullptr}}};

    const char* constCharPtrArr[20][11] = {{nullptr}};

    volatile const int volatileConstInt = 0;

    volatile int volatileInt = 100;

    //volatile AND pointers

    // `ptr21` is a volatile pointer to volatile data
    volatile int* volatile ptr21 = nullptr; 

     // `ptr22` is a volatile pointer to non-volatile data
    int* volatile ptr22 = nullptr; 

    // `ptr23` is a non-volatile pointer to volatile data        
    volatile int* ptr23 = nullptr;    

    // `ptr25` is a non-volatile pointer to a constant volatile integer.      
    const volatile int* ptr25 = nullptr;    

    //Constant volatile array of integers
    const volatile int constVolatileIntArr[5] = {0}; 

    //Array of pointers to constant integers
    const int* constPtrArr[5] = {nullptr}; 

    //Array of pointers to volatile integers
    volatile int* volatilePtrArr[5] = {nullptr}; 

    //  Constant array of volatile pointers to integers
    volatile int* const constVolatilePtrIntArr[5] = {nullptr}; 

    //Array of constant pointers to constant integers
    const int* const constPtrConstArr[5] = {nullptr}; 
    //  Volatile array of constant pointers to integers
    const int* volatile volatileConstPtrArr[5] = {nullptr}; 

    // Array of arrays of integers (2D array)
    int int2DArray[2][3] = {{0}}; // 2D array of integers 

    // Constant 2D array of integers
    const int constInt2DArray[2][3] = {{0}}; 

    //Volatile 2D array of integers
    volatile int volatileInt2DArray[2][3] = {{0}}; 

    //Constant volatile 2D array of integers
    const volatile int constVolatileInt2DArray[2][3] = {{0}}; 

    int refTry = 5;
    int rValueRefTry = 8;
    referenceCheck(refTry, std::move(rValueRefTry));

    return 0;
}