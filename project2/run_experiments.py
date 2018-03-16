from itertools import ifilter, product
import os
import subprocess
import sys

rangeJ = [1, 2]
rangeK = [1, 2]
rangeL = [1, 2]
rangeF = [4, 8]
rangeR = xrange(1, 9)

def allConfigurations():
  for r, f, j, k, l in product(rangeR, rangeF, rangeJ, rangeK, rangeL):
    yield (r, f, j, k, l)

def getFieldValue(output, fieldStr):
  indexStart = output.rfind(fieldStr) + len(fieldStr)
  indexEnd = output.find('\n', indexStart)
  return output[indexStart:indexEnd] 

def main():
  allTraceFiles = sys.argv[1:] if len(sys.argv) > 1 else [os.path.join('traces', t + '.100k.trace') for t in ['gcc', 'gobmk', 'hmmer', 'mcf']]
  for traceFile in allTraceFiles:
    t = os.path.splitext(os.path.split(traceFile)[-1])[0]
    with open(t + '.txt', 'wb') as of:
      for config in allConfigurations():
        strConfig = tuple(str(p) for p in config)
        executable = os.path.join(os.getcwd(), 'procsim')
        p = subprocess.Popen([executable, '-r', strConfig[0], '-f', strConfig[1], '-j', strConfig[2], '-k', strConfig[3], '-l', strConfig[4]],
                             stdin = subprocess.PIPE, stdout = subprocess.PIPE)
        with open(traceFile, 'rb') as f:
          output, error = p.communicate(input = f.read())
          adqs = getFieldValue(output, 'Avg Dispatch queue size: ')
          mdqs = getFieldValue(output, 'Maximum Dispatch queue size: ')
          ipc = getFieldValue(output, 'Avg inst retired per cycle: ')
          rt = getFieldValue(output, 'Total run time (cycles): ')
          of.write(', '.join(strConfig) + ',\t%s, %s, %s, %s\n'%(adqs, mdqs, ipc, rt))
          of.flush()

if __name__ == '__main__':
  main()
