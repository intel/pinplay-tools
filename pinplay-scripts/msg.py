#!/usr/bin/env python3
#BEGIN_LEGAL 
#BSD License 
#
#Copyright (c)2022 Intel Corporation. All rights reserved.
#
#Redistribution and use in source and binary forms, with or without modification, 
# are permitted provided that the following conditions are met:
#
#1. Redistributions of source code must retain the above copyright notice, 
#   this list of conditions and the following disclaimer.
#
#2. Redistributions in binary form must reproduce the above copyright notice, 
#   this list of conditions and the following disclaimer in the documentation 
#   and/or other materials provided with the distribution.
#
#3. Neither the name of the copyright holder nor the names of its contributors 
#   may be used to endorse or promote products derived from this software without 
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED.
# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#END_LEGAL

# Print out messages, including error messages
#
#

import os
import string
import sys
import subprocess


def PrintMsg(msg):
    """
    Prints a message to stdout.

    Use flush in order to ensure the strings printed are in order.

    """

    print(msg, flush=True)


def PrintMsgNoCR(msg):
    """
    Prints a message to stdout, but don't add CR.

    Use flush in order to ensure the strings printed are in order.

    """

    print(msg, flush=True, end='')


def PrintAndExit(msg):
    """
    Prints an error message exit.

    Use flush in order to ensure the strings printed are in order.

    """

    string = os.path.basename(sys.argv[0]) + ' ERROR: ' + msg + '\n'
    print('\n' + string, flush=True)
    sys.exit(-1)


def PrintHelpAndExit(msg):
    """ Prints an error message with help to stderr and exits """

    string = os.path.basename(sys.argv[0]) + ' ERROR: ' + \
                msg + '.\n' + 'Use --help to see valid argument options.\n'
    sys.stderr.write(string)
    sys.exit(-1)


def PrintMsgDate(string):
    """
    Print out a msg with three '*' and a timestamp.

    """

    import time

    pr_str = '***  ' + string + '  ***    ' + time.strftime('%B %d, %Y %H:%M:%S')
    PrintMsg('')
    PrintMsg(pr_str)


def PrintMsgPlus(string):
    """
    Print out a msg with three '+'.

    """

    pr_str = '+++  ' + string
    PrintMsg('')
    PrintMsg(pr_str)


def PrintStart(options, start_str):
    """If not listing, print a string."""

    if not options.list:
        # import pdb;  pdb.set_trace()
        PrintMsgDate(start_str)

def ensure_string(text):
    if isinstance(text, bytes):
        return text.decode('utf-8')
    if isinstance(text, list):
        return [(x.decode('utf-8') if isinstance(x, bytes) else x) for x in text]
    return text
