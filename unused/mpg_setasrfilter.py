#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Author: Tomi Ollila -- too ät iki fi
#
#	Copyright (c) 2004 Tomi Ollila
#	    All rights reserved
#
# Created: Mon Sep 13 17:26:55 EEST 2004 too
# Last modified: Thu Oct 07 00:55:20 EEST 2004 too
#
# This program is released under GNU GPL. Check
# http://www.fsf.org/licenses/licenses.html
# to get your own copy of the GPL license.

# Even quicker hack.

import sys

#c# sys.argv[0] = 'm2vmp2cut builtin mpg_setasrfilter.py'

class stdinbuf:
    SIZE = 65536

    def __init__(self):
        self.file = sys.stdin
        self.buf = self.file.read(self.SIZE)
        self.offset = 0
        self.len = len(self.buf)
        self.gone = 0

    def writeupto(self, file, index):
        while True:
            try:
                pos = self.buf.index(index, self.offset)
                ilen = len(index)
                file.write(self.buf[self.offset:pos + ilen])
                self.offset = pos + ilen
                return

            except ValueError:
                file.write(self.buf[self.offset:-3])
                self.offset = self.len - 3
                self.fillbuf(3)

    def writerest(self, file):
        file.write(self.buf[self.offset:])

    def fillbuf(self, rest):
        if rest > 0:
            self.buf = self.buf[self.offset:] \
                       + self.file.read(self.SIZE - rest)
            self.gone += self.offset
        else:
            self.gone += self.len
            self.buf = self.file.read(self.SIZE)

        self.len = len(self.buf)
        self.offset = 0

        if self.len == rest:
            raise EOFError

    def needbytes(self, bytes):
        while True:
            rest = self.len - self.offset

            #print 'bytes', bytes, 'rest', rest, \
            #      'offset', self.offset, 'len', self.len

            if rest >= bytes:
                oo = self.offset
                self.offset += bytes

                return self.buf[oo:self.offset]

            self.fillbuf(rest)


if len (sys.argv) != 2:
    print >> sys.stderr, 'Usage: ', sys.argv[0], '<asrnum>'
    sys.exit(1)

try:
    asrcode = int(sys.argv[1])
except ValueError:
    print >> sys.stderr, 'Incorrect value for asrnum:', sys.argv[1]
    sys.exit(1)

f = stdinbuf()

while True:

    try:
        f.writeupto(sys.stdout, '\000\000\001\263')

        pre = f.needbytes(3)
        #code = chr(ord(f.needbytes(1)))
        code = chr((ord(f.needbytes(1)) & 0x0f) + (asrcode << 4))
        #print >> sys.stderr, ord(code)

        sys.stdout.write(pre + code)

    except EOFError:
        f.writerest(sys.stdout)
        sys.exit(0)

