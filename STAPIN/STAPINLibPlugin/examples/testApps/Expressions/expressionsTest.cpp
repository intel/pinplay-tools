/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include <iostream>


typedef struct {
    int inner;
} innerStruct;

typedef struct {
    int myNumber;
    int myNumber_2;
    int myNumber_3;
    int myNumber_4;
    int myNumber_5;
    innerStruct _innerStruct;
    int innerArr[5][7];
} MyStruct;

void foo(int a, char* b, int c[8])
{
    std::cout<<"a is "<<a<<std::endl;
    std::cout<<"b is "<<a<<std::endl;
    for (size_t i = 0; i < 8; ++i)
    {
        std::cout<<c[i]<<std::endl;
    }
    
}

int main()
{
    signed x = 4;
    MyStruct arr[5][7];
    for (int i = 0; i < 5; ++i)
    {
        for (int j = 0; j < 7; j++)
        {
            MyStruct myStruct;
            arr[i][j].myNumber = 1;
            arr[i][j].myNumber_2 = 2;
            arr[i][j].myNumber_3 = 3;
            arr[i][j].myNumber_4 = 4;
            arr[i][j].myNumber_5 = 5;
            arr[i][j]._innerStruct.inner = 7;
            for (int ii = 0; ii < 5; ++ii)
            {
                for (int jj = 0; jj < 7; ++jj)
                {
                    arr[i][j].innerArr[ii][jj] = ii*jj;
                }
            }
        }
    }
    int intTry = 7;
    int simpleArr[2][4];
    for (int i = 0; i < 2; ++i)
    {
        for (int j = 0; j < 4; j++)
        {
            simpleArr[i][j] = i+j;
        }
    }

    enum myEnum  {A, B, C, D};
    myEnum myEnumValue = myEnum::C; 

    int checkSizeArr[2];
    checkSizeArr[0] = 2;
    checkSizeArr[1] = 1;

    const char* simpleCharPtr = "123456789123456";

    std::string simpleString = "1234567891234567";

    class Window
    {
    public:
    Window();                               // Default constructor.
    Window( int x1, int y1,                 // Constructor specifying
    int x2, int y2 );                       // Window size.
    bool SetCaption( const char *szTitle ); // Set window caption.
    const char *GetCaption();               // Get window caption.
    char *szWinCaption;                     // Window caption.
    };

    // Declare a pointer to the data member szWinCaption.
    char * Window::* pwCaption = &Window::szWinCaption;

    return 0;
}