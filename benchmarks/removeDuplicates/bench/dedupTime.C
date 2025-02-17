// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2010 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "parlay/parallel.h"
#include "parlay/primitives.h"
#include "common/time_loop.h"
#include "common/parse_command_line.h"
#include "common/sequenceIO.h"
#include <iostream>
#include <algorithm>

using namespace std;
using namespace benchIO;

using parlay::sequence;

template <typename T>
int timeDedup(sequence<T> &A, int rounds, char* outFile) {
  size_t n = A.size();
  sequence<T> R;
  time_loop(rounds, 1.0,
       [&] () {R.clear();},
       [&] () {R = dedup(A);},
       [] () {});
  if (outFile != NULL) writeSequenceToFile(R, outFile);
  return 1;
}

int main(int argc, char* argv[]) {
  commandLine P(argc,argv,"[-o <outFile>] [-r <rounds>] <inFile>");
  char* iFile = P.getArgument(0);
  char* oFile = P.getOptionValue("-o");
  int rounds = P.getOptionIntValue("-r",1);
  int verbose = P.getOption("-v");

  FileReader reader{iFile};
  elementType in_type = elementTypeFromHeader(reader.readHeader());

  if (in_type == intType) {
    auto In = reader.readSeq<int>();
    return timeDedup<int>(In, rounds, oFile);
  } else if (in_type == stringT) {
    using str = sequence<char>;
    auto In = reader.readSeq<str>();
    return timeDedup<str>(In, rounds, oFile);
  } else {
    cout << "dedupTime: input file not of right type" << endl;
    return(1);
  }
}
