#!/usr/bin/env python3
# -*- python -*-
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

import optparse
import os
import re
import sys

import cmd_options
import config
import msg
import util
"""
@package traceinfo

Generate the *.traceinfo files for a set of LIT files.  Also print the traces
and region pinballs, checking to make sure there is one trace for each region
pinball.
"""


def GetOptions():
    """
    Get users command line options/args and check to make sure they are correct.

    @return List of options and one argument: base name for traces
    """

    version = '$Revision: 1.7 $'
    version = version.replace('$Revision: ', '')
    ver = version.replace(' $', '')
    us = '%prog [options] trace_base_name'
    desc = 'Generate the traceinfo XML files for a set of traces.'

    util.CheckNonPrintChar(sys.argv)
    parser = optparse.OptionParser(usage=us, version=ver, description=desc)

    # Command line options to control script behavior.
    #
    # import pdb;  pdb.set_trace()
    # cmd_options.weight_file(parser, '')

    # Parse command line options and get any arguments.
    #
    (options, args) = parser.parse_args()

    # Added method cbsp() to 'options' to check if running CBSP.
    #
    util.AddMethodcbsp(options)

    # Is the required argument given on the command line.
    #
    if len(args) < 1:
        msg.PrintAndExit(
            'Not enough arguments given to script.  Use -h to get help')

    # Check to make sure the required 'blank' XML file are in the
    # current directory.
    #
    #import pdb;  pdb.set_trace()
    for blank_file in config.traceinfo_blank_files:
        if not os.path.isfile(blank_file):
            msg.PrintAndExit(
                'Required \'blank\' xml traceinfo file not found.\n     ' +
                blank_file)

    return options, args


def dump_file(file_name):
    """

    @return cmd line
    """
    with open(file_name, 'r') as fin:
        print(fin.read())

############################################################################

# Local names for the traces files given in the config file.
#
blank_DTD = config.traceinfo_blank_files[0]
blank_head = config.traceinfo_blank_files[1]
blank_foot = config.traceinfo_blank_files[2]

# Get command line options and arguments
#
options, args = GetOptions()
base_name = args[0]

# Running in a LIT directory.
#
param = {'options': options, 'in_lit_dir': True}

# Get the cluster information from the regions CSV file.  Check to make sure we
# have parsed data for every cluster.
#
#import pdb;  pdb.set_trace()
cluster_info, not_used, total_instr = util.GetClusterInfo(base_name, param)
if cluster_info == {}:
    msg.PrintAndExit('Error reading file: ' + base_name)
cluster_list = util.ParseClusterInfo(cluster_info)
if len(cluster_info) != len(cluster_list):
    msg.PrintAndExit('traceinfo.py() did not parse enough clusters.\n' '   Num clusters:         %d\n' \
        '   Num parsed clusters:  %d' % (len(cluster_info), len(cluster_list)))

# First print some info which is independent of the specific traces in the current directory.
#
count = len(cluster_info)
dump_file(blank_DTD)
dump_file(blank_head)
msg.PrintMsg('<traces>')
msg.PrintMsg('<trace-details trace-count="%d" total-instruction-count="%d" publication-workweek="CHANGEME" publication-year="2006">' % \
    (count, total_instr))

# Print the trace specific information
#
err_msg = lambda string: msg.PrintAndExit('traceinfo.py() encountered '
                                          'an error parsing \'' + string)
for cl in cluster_list:
    if ('cluster_num' in cl):
        cluster_num = cl['cluster_num']
    else:
        err_msg('cluster_num')
    if ('tid' in cl):
        tid = cl['tid']
    else:
        err_msg('tid')
    if ('region' in cl):
        region = cl['region']
    else:
        err_msg('region')
    if ('first_icount' in cl):
        if ('first_icount' in cl):
            first_icount = cl['first_icount']
        else:
            err_msg('first_icount')
        if ('last_icount' in cl):
            last_icount = cl['last_icount']
        else:
            err_msg('last_icount')
        icount = last_icount - first_icount
    else:
        if ('length' in cl):
            first_icount = 0
            icount = cl['length']
    if ('weight' in cl):
        weight = cl['weight']
    else:
        err_msg('weight')

    msg.PrintMsg('<trace-data trace-name="%s_%03d" instruction-offset="%s" instruction-count="%s">' % \
          (base_name, region, first_icount, icount))
    msg.PrintMsg(
        '<trace-weight method="SimPoint(K-Means)" weight="%6.5f" cluster-id="%s">' %
        (weight, cluster_num))
    msg.PrintMsg('</trace-weight>')
    msg.PrintMsg('</trace-data>')

# More default info to print.
#
dump_file(blank_foot)
msg.PrintMsg('<![CDATA[')
msg.PrintMsg('CHANGEME: Give the information of your test machine:')
msg.PrintMsg(
    'CHANGEME: brand/model (e.g. "IBM Intellistation", "HP xxx", "Intel prototype"')
msg.PrintMsg('CHANGEME: Memory size')
msg.PrintMsg('CHANGEME: Cache sizes: I-cache, D-cache, L1, L2...')
msg.PrintMsg('')

# Don't try to run these commands on native Windows.
#
platform = util.Platform()
if platform != config.WIN_NATIVE:
    msg.PrintMsg('The follwing is automatically generated using \'uname -a\'')
    result = util.RunCmd('uname -a', options, '',
                         concurrent=False,
                         print_time=False,
                         print_cmd=False)
    msg.PrintMsg(
        'The follwing is automatically generated using \'cat /proc/cpuinfo \'')
    result = util.RunCmd('cat /proc/cpuinfo', options, '',
                         concurrent=False,
                         print_time=False,
                         print_cmd=False)

# Final set of default info to print
#
msg.PrintMsg(']]>')
msg.PrintMsg('</system-info>')
msg.PrintMsg(
    '<!-- Anything else about the platform. You can have more than one. -->')
msg.PrintMsg('<other>')
msg.PrintMsg('CHANGEME: Other information goes here. ')
msg.PrintMsg(
    'CHANGEME: Please include the PinPoint Kit version and other important')
msg.PrintMsg('CHANGEME: or unusual configuration setting.')
msg.PrintMsg('</other>')
msg.PrintMsg('</platform>')
msg.PrintMsg('</trace-info>')

sys.exit(0)
