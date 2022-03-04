#!/usr/bin/env python3

# BEGIN_LEGAL
# The MIT License (MIT)
#
# Copyright (c) 2022, National University of Singapore
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# END_LEGAL

import sys, os, collections
from functools import reduce
import numpy as np

num_threads = 8

if len(sys.argv) <= 1:
  print("Usage: %s <basename> [<num-threads> <out-basename>]" %sys.argv[0])
  sys.exit(1)

basename = sys.argv[1]
print('Using basename = [%s]' % basename)

out_basename = basename
if len(sys.argv) >= 3:
  num_threads = int(sys.argv[2])
  print ('Number of threads = [%d]' % num_threads)
  if len(sys.argv) > 3:
    out_basename = sys.argv[3]
    print ('Using output basename = [%s]' % out_basename)

files = []
out = []
for f in range(num_threads):
  files.append(open("%s.T.%s.bb" % (basename,f), "r"))

# For each T line
max_bbv = int(-1)
thread_line_count = [int(0)]*num_threads
while True:
  end_of_file = [False]*num_threads
  # for each file
  for f in range(num_threads):
    # for each line
    while True:
      line = files[f].readline()
      if line == "":
        end_of_file[f] = True
        break
      if line and line.startswith('T:'):
        thread_line_count[f] += 1;
        max_vals = map(lambda x:int(x.split(':')[1]), filter(lambda x:x, line[1:].rstrip().split(' ')))
        max_bbv_tmp = max(max_vals)
        max_bbv = max(max_bbv, max_bbv_tmp)
        break
  if reduce(lambda x,y:x and y, end_of_file):
    break
  # There can be some threads that end early.

print('max_bbv', max_bbv)

files = []
out = []
for f in range(num_threads):
  files.append(open("%s.T.%s.bb" % (basename,f), "r"))
out.append(open("%s.global.cv" % (out_basename,), "w"))
log = open("%s.log.cv" % (out_basename,), "w")

bb_pieces = {}
global_icount_marker = 0
unfiltered_icount_marker = 0
marker_dict = {}
pcmarker_dict = {}
globalbb_init_str = []

# For each T line
thread_line_count = [int(0)]*num_threads
while True:
  end_of_file = [False]*num_threads
  # for each file
  thread_icounts = [int(0)]*num_threads
  for f in range(num_threads):
    # for each line
    while True:
      line = files[f].readline()
      if line == "":
        end_of_file[f] = True
        break
      if line and line.startswith('# Slice ending') and 'global' in line:
          global_icount_marker = int(line.split()[-1])
      elif line and line[0] == 'T':
        thread_line_count[f] += 1;
        if global_icount_marker not in bb_pieces:
            bb_pieces[global_icount_marker] = {}
        bb_pieces[global_icount_marker][f] = np.array(list(map(lambda x:':%s:%s'%x, map(lambda x:(int(x.split(':')[1])+(max_bbv*f),int(x.split(':')[2])), filter(lambda x:x, line[1:].rstrip().split(' '))))))
        break
  if reduce(lambda x,y:x and y, end_of_file):
    break

global_fn_bkp = basename + '.global.bb.bkp'
global_fn = basename + '.global.bb'
if not os.path.isfile(global_fn_bkp):
    print('Unable to find global BBV file: [%s]. Trying [%s]' % (global_fn_bkp, global_fn))
else:
    global_fn = global_fn_bkp

final_pcmarker = ''
try:
  with open(global_fn, 'r') as global_in:
    curr_pcmarker = ''
    for line in global_in:
      if line and line.startswith('# Slice ending at global'):
        global_icount_marker = int(line.split()[-1])
        pcmarker_dict[global_icount_marker] = curr_pcmarker
        curr_pcmarker = ''
      elif line and line.startswith('# Unfiltered count'):
        unfiltered_icount_marker = int(line.split()[-1])
        marker_dict[global_icount_marker] = unfiltered_icount_marker
      elif line and line.startswith('S:'):
        curr_pcmarker = line
      elif line and (line.startswith('G:') or line.startswith('I:') or line.startswith('C:') or line.startswith('M:')):
        globalbb_init_str.append(line)
    if curr_pcmarker:
      final_pcmarker = curr_pcmarker
except IOError as e:
  err_str = 'Unable to open the global BBV file: [%s]' % global_fn
  print(err_str)
  log.write(err_str)

f_thread_ins = open("%s.threadins.cv" % (out_basename,), "w")
ordered_bb_pieces = collections.OrderedDict(sorted(bb_pieces.items()))

out[0].write('%s num_threads %d\n' % ('#make-balanced-concat-vectors.py', num_threads))
if globalbb_init_str:
  for line in globalbb_init_str:
    out[0].write('%s' % line)

for k,v in ordered_bb_pieces.items():
    if pcmarker_dict[k]:
      out[0].write(pcmarker_dict[k])
    out[0].write('# Slice ending at global %s\n' % str(k))
    out[0].write('# Unfiltered count %s\n' % (str(marker_dict[k]) if k in marker_dict else str(k)))
    out[0].write('T')
    
    ordered_thread_bb_pieces = collections.OrderedDict(sorted(ordered_bb_pieces[k].items()))
    thread_ins_contri = [0.0] * num_threads
    tot_ins = 0
    for k1, v1 in ordered_thread_bb_pieces.items():
        th_ins = sum([int(el.split(':')[-1]) for el in v1])
        tot_ins += th_ins
        thread_ins_contri[k1] = th_ins
        if len(ordered_thread_bb_pieces[k1]) == 0:
          continue
        out[0].write(' '.join(ordered_thread_bb_pieces[k1]))
        out[0].write(' ')
    out[0].write('\n')
    
    if tot_ins == 0:
        err_str = 'Found slice without instructions, icounts: %s' % str(k)
        print (err_str)
        log.write(err_str)
    else:
        f_thread_ins.write( ','.join([ str(round(el/tot_ins, 3)) for el in thread_ins_contri ]) )
        f_thread_ins.write('\n')
    
    # In this version, it is okay to not have all thread data lengths be the same
    #for f in range(7):
    #  assert(thread_line_count[f] == thread_line_count[f+1])

if final_pcmarker:
  out[0].write(final_pcmarker)
  out[0].write('\n')

try:
  with open(global_fn, 'r') as global_in:
    for line in global_in:
      if 'Dynamic instruction count' in line:
        break
    out[0].write(line)
    for line in global_in:
      out[0].write(line)
except IOError as e:
  err_str = 'Unable to open the global BBV file: [%s]' % global_fn
  print(err_str)
  log.write(err_str)

out[0].close()
log.close()
