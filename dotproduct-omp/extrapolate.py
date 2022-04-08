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
import os, sys, getopt, glob

def usage(rc = 0):
  print('''Usage:
  \t %s
  \t -h | --help
  \t -w | --wp-dir=<path/to/whole-program-directory>
  \t -r | --region-dir=<path/to/regions-directory>
  \t -d | --data-dir=<path/to/Data-directory>''' % sys.argv[0])
  exit(rc)

def read_pb_csv(data):
  csv_file = ''
  if data.endswith('.csv'):
    csv_file = data
  else:
    for _f in glob.glob(os.path.join(data + '/*.pinpoints.csv')):
      csv_file = _f
  region_multiplier = {}
  with open (csv_file) as _f:
    for line in _f:
      if not line:
        continue
      if line.startswith('cluster'):
        line = line.strip()
        fields = line.split(",")
        sim_region = int(fields[2])
        region_multiplier[sim_region] = float(fields[14])
  return region_multiplier

def get_wp_rdtsc(wp_dir):
  res_file = glob.glob(os.path.join(wp_dir + '/*.BIN.perf.txt*'))
  wp_moving_ave = 0
  ctr = 0
  for wp_res_file in res_file:
    start_tsc = 0
    end_tsc = 0
    ctr += 1
    with open(wp_res_file) as _f:
      for line in _f:
        if 'ROI start' in line and 'TSC' in line:
          start_tsc = int(line.split()[-1])
        elif 'ROI end' in line and 'TSC' in line:
          end_tsc = int(line.split()[-1])
    wp_moving_ave = (wp_moving_ave * (ctr - 1) + (end_tsc - start_tsc)) / ctr
  return wp_moving_ave

def get_region_rdtsc(region_dir, regionids):
  region_rdtsc_ave = {}
  for _r in regionids:
    res_file = glob.glob(os.path.join(region_dir, '*_globalr%s.0.BIN.perf.txt*' % str(_r)))
    region_moving_ave = 0
    ctr = 0
    for region_res_file in res_file:
      warmup_end = 0
      sim_end = 0
      ctr += 1
      with open(region_res_file) as _f:
        for line in _f:
          if 'Warmup end' in line and 'TSC' in line:
            warmup_end = int(line.split()[-1])
          elif 'Simulation end' in line and 'TSC' in line:
            sim_end = int(line.split()[-1])
      region_moving_ave = (region_moving_ave * (ctr - 1) + (sim_end - warmup_end)) / ctr
    region_rdtsc_ave[_r] = region_moving_ave
  return region_rdtsc_ave

def extrapolate(scaling_factor, region_rdtsc):
  proj_rdtsc = 0
  for _r in scaling_factor.keys():
    proj_rdtsc += scaling_factor[_r] * region_rdtsc[_r]
  return proj_rdtsc

if __name__=="__main__":
  region_dir = ''
  data_dir = ''
  wp_dir = ''

  try:
    opts, args = getopt.getopt(sys.argv[1:], 'hr:d:w:', [ 'help', 'region-dir=', 'data-dir=', 'wp-dir=' ])
  except getopt.GetoptError as e:
    print (e)
    usage(1)
  for o, a in opts:
    if o == '-h' or o == '--help':
      usage(0)
    if o == '-r' or o == '--region-dir':
      region_dir = a
    if o == '-d' or o == '--data-dir':
      data_dir = a
    if o == '-w' or o == '--wp-dir':
      wp_dir = a

  if not (os.path.exists(region_dir) and os.path.exists(data_dir)  and os.path.exists(wp_dir) and
        os.listdir(region_dir) and os.listdir(data_dir) and os.listdir(wp_dir)):
    print ('Error: Some directories do not exist or are empty.')
    sys.exit(1)
  data_dir = os.path.abspath(data_dir)
  region_dir = os.path.abspath(region_dir)
  wp_dir = os.path.abspath(wp_dir)
  scaling_factor = read_pb_csv(data_dir)
  regionids = list(scaling_factor.keys())
  wp_rdtsc = get_wp_rdtsc(wp_dir)
  region_rdtsc = get_region_rdtsc(region_dir, regionids)
  proj_rdtsc = extrapolate(scaling_factor, region_rdtsc)
  pred_err = round(((wp_rdtsc - proj_rdtsc)/wp_rdtsc) * 100, 2)
  print('wp_rdtsc,region_rdtsc,err%')
  print('%s,%s,%s%%' % (wp_rdtsc, proj_rdtsc, pred_err))



