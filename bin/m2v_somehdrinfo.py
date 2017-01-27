#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#       Copyright (c) 2004 Tomi Ollila
#           All rights reserved
#
# Created: Wed Sep 15 22:06:57 EEST 2004 too
# Last modified: Wed 06 May 2015 22:44:15 +0300 too
#
# This program is released under GNU GPL. Check
# http://www.fsf.org/licenses/licenses.html
# to get your own copy of the GPL license.

from __future__ import print_function

import sys
import struct

class m2vbuf:
    SIZE = 65536

    def __init__(self, filename, seek):
        self.file = open(filename, 'rb')
        self.buf = ''
        self.offset = 0
        #self.pos = 0
        self.len = 0
        self.gone = 0

        self.file.seek(seek)

        while True:
            self.fillbuf(self.len - 0)
            try:
                self.pos = self.buf.index(b'\000\000\001', 0, self.len)
                return
            except ValueError:
                pass

    def fillbuf(self, rest):
        if self.gone > 32768: raise EOFError # a hax
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
                self.pos = self.buf.index(b'\000\000\001',
                                          self.offset + 3, self.len)
                return self.buf[self.offset:self.pos]
            except ValueError:
                pass

            try:
                self.fillbuf(self.len - self.offset)
            except EOFError:
                if self.len <= self.offset + 4:
                    raise EOFError
                self.len = self.offset
                return self.buf[self.offset:]


###### Code copied (and edited) from python_mpeg/mpeg.py... #####

zigzag = [[ 0, 1, 5, 6,14,15,27,28],
          [ 2, 4, 7,13,16,26,29,42],
          [ 3, 8,12,17,25,30,41,43],
          [ 9,11,18,24,31,40,44,53],
          [10,19,23,32,39,45,52,54],
          [20,22,33,38,46,51,55,60],
          [21,34,37,47,50,56,59,61],
          [35,36,48,49,57,58,62,63]]

# t = reduce(lambda x,y: x+y, zigzag[0])
# t.sort()
# if t!=range(8*8): raise repr(t)
# del t

def unzigzag(array):

    m = [[ x for x in range(8)] for y in range(8)]

    for v in range(8):
        for u in range(8):
            m[v][u] = array[zigzag[v][u]]

    return m

def print_matrix(ofile, m):
    ofile.write('\n'.join(map(','.join, map(lambda x: map(str,x),m)))+'\n')

##### end of copy+edit. #####

class G:
    progressive = 0
    intra = 0
    non_intra = 0
    dispext_hdr = 0
    intra_dc_prec = 0

def f_picture(G, buf):    pass
def f_user_data(G, buf):  pass
def f_seq_error(G, buf):  pass
def f_seq_end(G, buf):    pass
def f_gop(G, buf):        pass

def f_seq_header(G, buf):
    if G.intra or G.non_intra:
        return

    bytes = struct.unpack('BBBBBBBB', buf[4:12])

    # horiz_size = (bytes[0] << 4) + (bytes[1] >> 4)
    # vert_size = ((bytes[1] & 0x0f) << 8) + bytes[2]
    # aspect_ratio = bytes[3] >> 4
    # frame_rate = bytes[3] & 0x0f
    # bit_rate = (bytes[4] << 10) + (bytes[5] << 2) + (bytes[6] >> 6)
    # vbv_buffer_size = (bytes[6] << 5) + (bytes[7] >> 3)
    # constrained_parameter = (bytes[7] >> 2) & 0x01

    load_intra_quantizier_matrix = (bytes[7] >> 1) & 0x01

    if load_intra_quantizier_matrix:
        G.intra = 1
        intrax = struct.unpack('B'*64, buf[12:76])

        hbit = bytes[7] & 0x01
        intra = []
        for b in intrax:
            intra.append((hbit << 7) + (b >> 1))
            hbit = b & 0x01
        del intrax
        imatrix = unzigzag(intra)
        del intra

        load_non_intra_quantizier_matrix = hbit
        if load_non_intra_quantizier_matrix:
            G.non_intra = 1
            non_intra = struct.unpack('B'*64, buf[76:140])
        else:
            non_intra = [16] * 64
    else:
        load_non_intra_quantizier_matrix = bytes[7] & 0x01
        if load_non_intra_quantizier_matrix:
            G.non_intra = 1
            non_intra = struct.unpack('B'*64, buf[12:76])
            imatrix = [[ 8,16,19,22,26,27,29,34],
                       [16,16,22,24,27,29,34,37],
                       [19,22,26,27,29,34,34,38],
                       [22,22,26,27,29,34,37,40],
                       [22,26,27,29,32,35,40,48],
                       [26,27,29,32,35,40,48,58],
                       [26,27,29,34,38,46,56,69],
                       [27,29,35,38,46,56,69,83]]
        else:
            return

    nimatrix = unzigzag(non_intra)

    ofile = open(sys.argv[3], 'w')
    print_matrix(ofile, imatrix)
    print_matrix(ofile, nimatrix)


def f_ext_start(G, buf):
    bytes = struct.unpack('BB', buf[4:6])
    ext_id = bytes[0] & 0xf0

    if ext_id == 0x10:
        if bytes[1] & 0x08:
            G.progressive = 1
        else:
            G.progressive = 0
        return

    if ext_id == 0x20:
        G.dispext_hdr = 1
        return

    if ext_id == 0x80: # picture coding extension
        b = buf[6]
        if type(b) is not int: b = ord(b) # python 2 compatibility
        G.intra_dc_prec = ((b & 0x0c) >> 2) + 8
        return

scode_actions = {
    0x00: f_picture,
    0xb2: f_user_data,
    0xb3: f_seq_header,
    0xb4: f_seq_error,
    0xb5: f_ext_start,
    0xb7: f_seq_end,
    0xb8: f_gop
    }


if len(sys.argv) < 4:
    exit(1)

f = m2vbuf(sys.argv[2], int(sys.argv[1]))

#x = None

while True:
    try:
        buf = f.next()
    except EOFError:
        print('progressive:', G.progressive, 'intra_matrix:', G.intra,
              'non_intra_matrix:', G.non_intra,
              'disp_ext_hdr:', G.dispext_hdr, "intra_dc_prec:", G.intra_dc_prec)
        sys.exit(0)

    a = buf[3]
    if type(a) is not int: a = ord(a) # python 2 compatibility

    try:
        scode_actions[a](G, buf)
    except KeyError:
        if a < 0xa0:
            # slice, ignoring for now.
            pass
        else:
            print("File contains mpeg header 0x%02x." % a, file=sys.stderr)
            print("Did you demultiplex your source?", file=sys.stderr)
            sys.exit(1)
