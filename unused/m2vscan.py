#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Author: Tomi Ollila -- too ät iki fi
#
#	Copyright (c) 2004 Tomi Ollila
#	    All rights reserved
#
# Created: Wed Sep 01 21:15:17 EEST 2004 too
# Last modified: Thu Oct 07 23:42:09 EEST 2004 too
#
# This program is released under GNU GPL. Check
# http://www.fsf.org/licenses/licenses.html
# to get your own copy of the GPL license.

# This code waits for cleanup, after proof-of-concept stage has been passed.

import sys
import struct

class m2vbuf:
    SIZE = 65536

    def __init__(self, filename):
        self.file = open(filename, 'rb')
        self.buf = ''
        self.offset = 0
        #self.pos = 0
        self.len = 0
        self.gone = 0

        while True:
            self.fillbuf(self.len - 0)
            try:
                self.pos = self.buf.index('\000\000\001', 0, self.len)
                return
            except ValueError:
                pass

    def fillbuf(self, rest):
        if rest > 0:
            nr = self.SIZE - rest
            if nr == 0:
                raise ValueError
            self.buf = self.buf[self.offset:] + self.file.read(nr)
            self.gone += self.offset
        else:
            self.gone += self.len
            self.buf = self.file.read(self.SIZE)

        self.len = len(self.buf)
        self.offset = 0

        if self.len == rest:
            raise EOFError

    def filepos(self):
        return self.gone + self.offset

    def next(self):
        self.offset = self.pos
        while True:
            try:
                self.pos = self.buf.index('\000\000\001', \
                                          self.offset + 3, self.len)
                return self.buf[self.offset:self.pos]
            except ValueError:
                pass

            try:
                self.fillbuf(self.len - self.offset)
            except EOFError:
                if self.len == self.offset:
                    raise EOFError
                return self.buf[self.offset:]

from optparse import OptionParser

parser = OptionParser(usage='usage: %prog [-v] input-file output-file')

parser.add_option('-v', '--verbose', action='store_true', dest='verbose',
                  default=False, help='Verbose mode')

(options, args) = parser.parse_args()

if len(args) != 2:
    parser.print_usage()
    print 'input-file and/or output-file missing.'
    sys.exit(1)

def f_picture(G, buf):	G.pictures += 1
def f_user_data(G, buf):	pass
def f_seq_error(G, buf):	pass
def f_ext_start(G, buf):	pass
def f_seq_end(G, buf):	pass


def f_seq_header(G, buf):
    G.seqpos = G.f.filepos()
    bytes = struct.unpack('BBBBBBBBB', buf[4:13])

    horiz_size = (bytes[0] << 4) + (bytes[1] >> 4)
    vert_size = ((bytes[1] & 0x0f) << 8) + bytes[2]
    aspect_ratio = bytes[3] >> 4
    frame_rate = bytes[3] & 0x0f
    bit_rate = (bytes[4] << 10) + (bytes[5] << 2) + (bytes[6] >> 6)
    vbv_buffer_size = (bytes[6] << 5) + (bytes[7] >> 3)
    #constrained_parameter =
    #load_intra_quantizier_matrix=

    #print horiz_size, 'x', vert_size
    #print aspect_ratio, frame_rate, bit_rate

    if frame_rate != 3:
        print >> sys.stderr, \
              "Currently we can only support PAL (25fps) frame rate."

def f_gop(G, buf):
    print >> G.ofile, G.infoline, '%02d' % (G.pictures - G.goppic)
    bytes = struct.unpack('BBBB', buf[4:8])

    drop_frame_flag = bytes[0] & 0x80
    hour   = (bytes[0] & 0x7f) >> 2
    minute = ((bytes[0] & 0x03) << 4) + (bytes[1] >> 4)
    second = ((bytes[1] & 0x07) << 3) + (bytes[2] >> 5)
    frame  = ((bytes[2] & 0x1f) << 1) + (bytes[3] >> 7)
    closed_gop_flag = bytes[3] & 0x40
    broken_gop_flag = bytes[3] & 0x20

    # XXX below hardcoded PAL frame rate, 25 fps.
    time = (((hour * 60 + minute) * 60 + second) * 25) + frame
    if time != G.pictures:
        print 'Timecode problem at', G.f.filepos(), '...'
        print '%02d:%02d:%02d.%02d' % (hour, minute, second, frame)
        print time, G.pictures
        sys.exit(1)

    if closed_gop_flag:
        cg = 'c'
    else:
        cg = 'o'

    #print '%02d:%02d:%02d.%02d' % (hour, minute, second, frame)
    #print drop_frame_flag, closed_gop_flag, broken_gop_flag

    G.goppic = G.pictures
    G.infoline = '%10d %5d %6d  %02d:%02d:%02d.%02d  %s ' \
                   % (G.seqpos, G.gops, G.goppic,
                      hour, minute, second, frame, cg)
    G.gops += 1

#    G.infoline = '-- ' + str(G.seqpos) +' '+ str(G.gops) +' '+ str(G.goppic) + ' %02d:%02d:%02d.%02d' % (hour, minute, second, frame)



scode_actions = {
    0x00: f_picture,
    0xb2: f_user_data,
    0xb3: f_seq_header,
    0xb4: f_seq_error,
    0xb5: f_ext_start,
    0xb7: f_seq_end,
    0xb8: f_gop
    }


class G:
    pictures = 0
    seqpos = -1
    goppic = 0
    gops = 0
    infoline = '#file offset gop# frame#  _ time _   c/o  # of frames'

G.f = f = m2vbuf(args[0])
G.ofile = open(args[1], 'w')

while True:
    try:
        buf = f.next()
    except EOFError:
        print >> G.ofile, G.infoline, G.pictures - G.goppic
        print >> G.ofile, 'end: size', f.filepos(), \
              'frames', G.pictures, 'gops', G.gops, \
              'abr', f.filepos() / (G.pictures / 25) * 8
        sys.exit(0)

    a = ord(buf[3])

    try:
        scode_actions[a](G, buf)
    except KeyError:
        if a < 0xa0:
            # slice, ignoring for now.
            pass
        else:
            print >> sys.stderr, "File contains mpeg header 0x%02x." % a
            print >> sys.stderr, "Did you demultiplex your source?";
            sys.exit(1)

#    if G.gops == 20:
#        sys.exit(0)
