# Copyright 2017 Asher Blum

import argparse
import re
import sys

BS = 512
RATE = 32000
FRAME_SECONDS = float(BS-1)/RATE
CMD_NONE = 0
CMD_STOP = 2
CMD_LOOP = 3



def seconds_to_frames(seconds):
  return int(float(seconds)/float(FRAME_SECONDS))

def parse_line(lineno, line):
  for pat in [
    r'(\d+\.\d+)\s+\d+\.\d+\s+a(\d+)',
    r'\d+\.\d+\s+\d+\.\d+\s+\+',
    r'#',
    r'\s*$'
  ]:
    m = re.match(pat, line)
    if m:
      return m.groups()
  sys.stderr.write("Syntax error at line %d\n" % lineno)
  sys.exit(1)


def read_commands_from_audacity_labels(filename):
  '''Read an Audacity labels file and return [(frameno, cmd), ...]'''
  res = []
  labels = [parse_line(lineno+1, line) for lineno, line in enumerate(open(filename))]
  labels = [l for l in labels if l]
  labels = [(float(s), int(op)) for s, op in labels]
  labels = [(seconds_to_frames(s), op) for s, op in labels]
  return labels

def make_meta_buf():
  '''Make the two-block metadata header - all zeros for now'''
  return '\0' * (2 * BS)

def make_last_block(cmd):
  print "*** mlb"
  zeros = '\x80' * (BS-1)
  return chr(cmd) + zeros
  
def write_combined_file(args):
  ops = read_commands_from_audacity_labels(args.label_file)
  ifh = open(args.audio_file)
  ofh = open(args.out_file, 'w')
  ofh.write(make_meta_buf())
  spf = BS - 1 # samples per frame
  frameno = 0
  last_cmd = CMD_LOOP if args.loop else CMD_STOP

  while True:
    buf = ifh.read(spf)
    if len(buf) < spf:
      ofh.write(make_last_block(last_cmd))
      return
    op = CMD_NONE
    if ops and ops[0][0] == frameno:
      op = ops[0][1]
      print "wrote op: %r" % ops[0][0]
      ops.pop(0)
    ofh.write(chr(op))
    ofh.write(buf)
    frameno += 1

def usage():
  print "Invoke with AUDIO_FILE LABEL_FILE OUT_FILE"
  sys.exit(1)
  
if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument("audio_file", help="Unsigned byte PCM file to read")
  parser.add_argument("label_file", help="Label file to read, in Audacity format")
  parser.add_argument("out_file", help="File to write combined audio/commands to")
  parser.add_argument("--loop", action="store_true", help="Should the track loop when reaching end?")
  args = parser.parse_args()

  write_combined_file(args)

