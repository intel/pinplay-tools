/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include <stdint.h>

int main()
{
    // 1. char* const ptr1; 
    // ptr1 is a constant pointer to a char
    char* const ptr1;

    // 2. const char* ptr2; 
    // ptr2 is a pointer to a constant char
    const char* ptr2;

    // 3. const char* const ptr3; 
    // ptr3 is a constant pointer to a constant char
    const char* const ptr3;

    // 4. char const* ptr4; 
    // ptr4 is a pointer to a constant char (same as const char*)
    char const* ptr4;

    // 5. char** const ptr5; 
    // ptr5 is a constant pointer to a pointer to a char
    char** const ptr5;

    // 6. const char** ptr6; 
    // ptr6 is a pointer to a pointer to a constant char
    const char** ptr6;

    // 7. char* const* ptr7; 
    // ptr7 is a pointer to a constant pointer to a char
    char* const* ptr7;

    // 8. const char* const* ptr8; 
    // ptr8 is a pointer to a constant pointer to a constant char
    const char* const* ptr8;

    // 9. char*** const ptr9; 
    // ptr9 is a constant pointer to a pointer to a pointer to a char
    char*** const ptr9;

    // 10. const char*** ptr10; 
    // ptr10 is a pointer to a pointer to a pointer to a constant char
    const char*** ptr10;

    // 11. char** const* ptr11; 
    // ptr11 is a pointer to a constant pointer to a pointer to a char
    char** const* ptr11;

    // 12. const char** const* ptr12; 
    // ptr12 is a pointer to a constant pointer to a constant pointer to a char
    const char** const* ptr12;

    // 13. char* const* const ptr13; 
    // ptr13 is a constant pointer to a constant pointer to a char
    char* const* const ptr13;

    // 14. const char* const* const ptr14; 
    // ptr14 is a constant pointer to a constant pointer to a constant char
    const char* const* const ptr14;

    // 15. char**** const ptr15; 
    // ptr15 is a constant pointer to a pointer to a pointer to a pointer to a char
    char**** const ptr15;

    // 16. const char**** ptr16; 
    // ptr16 is a pointer to a pointer to a pointer to a pointer to a constant char
    const char**** ptr16;

    // 17. char*** const* ptr17; 
    // ptr17 is a pointer to a constant pointer to a pointer to a pointer to a char
    char*** const* ptr17;

    // 18. const char*** const* ptr18; 
    // ptr18 is a pointer to a constant pointer to a constant pointer to a pointer to a char
    const char*** const* ptr18;

    // 19. char** const* const ptr19; 
    // ptr19 is a constant pointer to a constant pointer to a pointer to a char
    char** const* const ptr19;

    // 20. const char** const* const ptr20; 
    // ptr20 is a constant pointer to a constant pointer to a constant pointer to a char
    const char** const* const ptr20;

    const int constInt;
 
    char* charPtrVariable;

    int intVariable;

    double doubleVariable;

    uint64_t uInt64Variable;

    const int boolVariable;

    int32_t* int32PtrVariable;

    int intArr[7];

    const int constIntArr[13];

    char** charPtrPtrArr[2][2][3];

    const char* constCharPtrArr[20][11];

    volatile const int volatileConstInt; 

    volatile int volatileInt = 100;

    //volatile AND pointers

    // `ptr21` is a volatile pointer to volatile data
    volatile int* volatile ptr21; 

     // `ptr22` is a volatile pointer to non-volatile data
    int* volatile ptr22; 

    // `ptr23` is a non-volatile pointer to volatile data        
    volatile int* ptr23;    

    // `ptr25` is a non-volatile pointer to a constant volatile integer.      
    const volatile int* ptr25;    

    //Constant volatile array of integers
    const volatile int constVolatileIntArr[5]; // Constant volatile array of 5 integers 

    //Array of pointers to constant integers
    const int* constPtrArr[5]; // Array of 5 pointers to constant integers 

    //Array of pointers to volatile integers
    volatile int* volatilePtrArr[5]; // Array of 5 pointers to volatile integers 

    //  Constant array of volatile pointers to integers
    volatile int* const constVolatilePtrIntArr[5]; // Constant array of 5 volatile pointers to integers 

    //Array of constant pointers to constant integers
    const int* const constPtrConstArr[5]; // Array of 5 constant pointers to constant integers 
    //  Volatile array of constant pointers to integers
    const int* volatile volatileConstPtrArr[5]; // Volatile array of 5 constant pointers to integers 

    // Array of arrays of integers (2D array)
    int int2DArray[2][3]; // 2D array of integers 

    // Constant 2D array of integers
    const int constInt2DArray[2][3]; // Constant 2D array of integers 

    //Volatile 2D array of integers
    volatile int volatileInt2DArray[2][3]; // Volatile 2D array of integers 

    //Constant volatile 2D array of integers
    const volatile int constVolatileInt2DArray[2][3]; // Constant volatile 2D array of integers 

    return 0;
}
