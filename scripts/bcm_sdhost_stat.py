import sys

ts0 = None

def timestr_to_ns(v):
  global ts0
  ns = int(int(v) / (19200000.0 / 1000.0 / 1000.0 / 1000.0))
  if ts0 is None:
    ts0 = ns
  return ns - ts0

class IOStat:
  def __init__(self, nblocks, ts_start, ts_dma_done, block_irqs, ts_wait_start, ts_wait_end):
    self.nblocks = nblocks
    self.ts_start = ts_start
    self.ts_dma_done = ts_dma_done
    self.block_irqs = block_irqs
    self.ts_wait_start = ts_wait_start
    self.ts_wait_end = ts_wait_end


def to_ms_str(v):
  return f'{v / 1000.0 / 1000.0:.06f} ms'


def parse_line(l):
  tokens = l.strip().split()
  idx = tokens[0]
  ts_start = timestr_to_ns(tokens[1])
  nblocks = int(tokens[2])
  nirq = int(tokens[3])
  block_irqs = [timestr_to_ns(tokens[4 + i]) for i in range(nirq)]
  irqsstr = ', '.join([to_ms_str(x) for x in block_irqs])
  tsstr = f'{ts_start}'
  ts_dma_done = timestr_to_ns(tokens[-3])
  ts_wait_start = timestr_to_ns(tokens[-2])
  ts_wait_end = timestr_to_ns(tokens[-1])
  string = f'{nblocks}/{nirq}'
  if nblocks != nirq:
    string += ' (mismatch)'
  print(string)
  string = ''
  string += f'{idx}:{nblocks}/{nirq} ts:{tsstr}'
  string += f', irqs:'
  prev_irq = None
  for irq in block_irqs:
    string += f', {to_ms_str(irq - ts_start)}'
    if prev_irq:
      string += f'(+{to_ms_str(irq - prev_irq)})'
    prev_irq = irq
  string += f', dma:{to_ms_str(ts_dma_done - ts_start)}'
  string += f', ws:{to_ms_str(ts_wait_start - ts_start)}'
  string += f', we:{to_ms_str(ts_wait_end - ts_start)}'
  string += f', delta:{to_ms_str(ts_wait_end - ts_start)}'
  string += f', per_block:{to_ms_str((ts_wait_end - ts_start)/nblocks)}'
  print(string)
  return IOStat(nblocks, ts_start, ts_dma_done, block_irqs, ts_wait_start, ts_wait_end)

def print_ts(v, ts_prev):
  string = to_ms_str(v)
  if ts_prev:
    string += ' +' + to_ms_str(v - ts_prev)
  print(string)

def main(input_file):
  ios = []
  with open(input_file, 'rt') as f:
    for l in f.readlines():
      ios.append(parse_line(l))
      # print(ts)
  for i in ios:
    print_ts(i.ts_start, None)
    print_ts(i.ts_dma_done, i.ts_start)
    print_ts(i.ts_wait_start, i.ts_start)
    print_ts(i.ts_wait_end, i.ts_start)

if __name__ == '__main__':
  input_file = sys.argv[1]
  main(input_file)

