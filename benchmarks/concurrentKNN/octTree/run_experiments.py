import argparse
import os
import sys
import multiprocessing

from create_graphs import *

# parameters:
#  - datastructures: neighbors_bench only for now
#  - threads, update_percent, number of nearest neighbors

parser = argparse.ArgumentParser()
parser.add_argument("threads", help="Number of threads")
parser.add_argument("ratios", help="Update ratio, number between 0 and 100.")
parser.add_argument("query_sizes", help="size of query (k)")
parser.add_argument("dimension", help="dimension of data")
parser.add_argument("input_file", help="input file")
parser.add_argument("input_name", help="input name")
parser.add_argument("-t", "--test_only", help="test script",
                    action="store_true")
parser.add_argument("-g", "--graphs_only", help="graphs only",
                    action="store_true")
parser.add_argument("-p", "--paper_ver", help="paper version of graphs, no title or legends", action="store_true")

args = parser.parse_args()
print("threads: " + args.threads)
print("update percent: " + args.ratios)
print("query_sizes: " + args.query_sizes)
print("dimension: " + args.dimension)
print("input_file: " + args.input_file)

test_only = args.test_only
graphs_only = args.graphs_only
rounds = 3

if test_only:
  rounds = 1

maxcpus = multiprocessing.cpu_count()
already_ran = set()

def string_to_list(s):
  s = s.strip().strip('[').strip(']').split(',')
  return [ss.strip() for ss in s]

def to_list(s):
  if type(s) == list:
    return s
  return [s]

def runstring(test, op, outfile, k):
    if op in already_ran:
        return
    already_ran.add(op)
    os.system("echo \"" + op + "\"")
    os.system("echo \"datastructure: " + test + "-"+str(k)+"\"")
    os.system("echo \"" + op + "\" >> " + outfile)
    os.system("echo \"datastructure: " + test+ "-"+str(k) + "\" >> " + outfile)
    x = os.system(op + " >> " + outfile)
    if (x) :
        if (os.WEXITSTATUS(x) == 0) : raise NameError("  aborted: " + op)
        os.system("echo Failed")
    
def runtest(test,procs,u,k,d,infile,extra,outfile) :
    r = 1
    otherargs = " -c -t 1.0 "
    if test_only:
        otherargs = " -c -t 0.1 "

    runstring(test, "PARLAY_NUM_THREADS=" + str(min(int(procs), maxcpus)) + " numactl -i all ./" + test + " -r " + str(r) + " -d " + str(d) + " -k " + str(k) + " -p " + str(procs) + extra + " -u " + str(u) + otherargs + " " + infile, outfile, k)


exp_type = ""
threads = args.threads
ratios = args.ratios
query_sizes = args.query_sizes
dimension = args.dimension
input_file = args.input_file
input_name = args.input_name

if '[' in args.threads:
  exp_type = "scalability"
  threads = string_to_list(threads)
else:
  print('invalid argument')
  exit(1)

if '[' in args.query_sizes:
  query_sizes = string_to_list(query_sizes)
else:
  print('invalid argument')
  exit(1)

outfile = "results/" + "-".join([ratios+"up", input_name]) + ".txt"

datastructures=["neighbors_bench"]

if not graphs_only:
  # clear output file
  os.system("echo \"\" > " + outfile)
  for ds in datastructures:
    for th in to_list(threads):
      for k in to_list(query_sizes):
        runtest(ds,th,ratios,k,dimension,input_file,"",outfile)

throughput = {}
stddev = {}
threads = []
ratios = []
algs = []

readResultsFile(outfile, throughput, stddev, threads, ratios, algs)

threads.sort()
ratios.sort()

print('threads: ' + str(threads))
print('update ratios: ' + str(ratios))
print('algs: ' + str(algs))
print(throughput)

graph_name="NN_"+input_name

ds_type="neighbors_bench"
alg_names=[]
for k in query_sizes:
  alg_names.append(ds_type+"-"+str(k))

plot_scalability_graphs(throughput, stddev, threads, ratios, alg_names, graph_name, args.paper_ver)
