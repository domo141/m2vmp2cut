#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Author: Tomi Ollila -- too ät iki fi
#
#	Copyright (c) 2004 Tomi Ollila
#	    All rights reserved
#
# Created: Mon Aug 23 21:27:18 EEST 2004 too
# First version ready: Wed Aug 25 22:35:10 EEST 2004 too
# Last modified: Sat Oct 22 09:51:06 EEST 2005 too
#
# This is my first python program.
#
# The information for mpeg headers are from page `mpeghdr.htm'
# -- google `I'm feeling lucky' found this page Tue Aug 24 2004.
#
# This program is released under GNU GPL. Check
# http://www.fsf.org/licenses/licenses.html
# to get your own copy of the GPL license.

# This code waits for cleanup, after proof-of-concept stage has been passed.

import sys
import os
from sys import stderr
import struct

#c# os.dup2(3, 0)

#c# sys.argv[0] = 'm2vmp2cut builtin mp2cutpoints.py'

#c# try: os.wait();
#c# except: pass

# mpeg1 layer 2 bitrate indexes (<<4)
bidict = { 0x10:  32, 0x20:  48, 0x30:  56, 0x40:  64, 0x50:  80, 0x60:  96,
           0x70: 112, 0x80: 128, 0x90: 160, 0xa0: 192, 0xb0: 224, 0xc0: 256,
           0xd0: 320, 0xe0: 384 }

# mpeg1 sampling rate indexes (<< 2)
sidict = { 0x00: 44100, 0x04: 48000, 0x08: 32000 }

# mpeg1 layer 1 ...
#def bidict(i):
#    return 32 * (i >> 4)


class simplefilebuf:
    SIZE = 65536

    def __init__(self, filename):
        self.file = open(filename, 'rb')
        self.buf = ''
        self.offset = 0
        self.len = 0
        self.gone = 0

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

    def filepos(self):
        return self.gone + self.offset

    def unusedbytes(self, bytes):
        self.offset -= bytes

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

    def dumpto(self, chr):
        rv = 0
        while True:
            s = self.buf.find(chr, self.offset)
            if s >= 0:
                adv = s - self.offset
                rv += adv
                self.offset += adv + 1
                return rv
            rv += self.len - self.offset
            self.fillbuf(0)


from optparse import OptionParser

usage = """
%prog [-v] timespecs input-file info-file')

timespec format: start-end[,start-end]. values either `0' or [[hh:]mm:]ss.ms
	example: 0-1.240,1:10.100-25:0.0
"""


parser = OptionParser(usage=usage)

parser.add_option('-v', '--verbose', action='store_true', dest='verbose',
                  default=False, help='Verbose mode')

(options, args) = parser.parse_args()

if len(args) != 3:
    parser.print_usage()
    sys.exit(1)

def tcode2ms(val):
    try:
        (timestr, msecs) = val.split('.')
    except ValueError:
        if val == '0':
            return 0
        print >> stderr, "ValueError with arg 1:<%s> at `%s'." % (args[0], val)
        sys.exit(1)

    time = timestr.split(':')
    time.reverse()
    rv = int(msecs);
    try:
        rv = rv + int(time[0]) * 1000		# seconds
        rv = rv + int(time[1]) * 1000 * 60	# minutes
        rv = rv + int(time[2]) * 1000 * 3600	# hours
    except IndexError:
        pass
    #print rv
    return rv

#times = [ (0, '0'), ( 1500, '1.5' ), ( 61300, '1:01.3' ) ]
times = []
try:
    for arg in args[0].split(','):
        (start,end) = arg.split('-');
        times.append( ( tcode2ms(start), start ) )
        times.append( ( tcode2ms(end), end ) )
        #print ( tcode2ms(start), start ), ( tcode2ms(end), end )
except ValueError:
    print >> stderr, "ValueError with arg 1:<%s> at `%s'." % (args[0], arg)
    sys.exit(1)

lasttime = tcode2ms(end)
stdout = sys.stdout
afn = 0
skipped = 0
totaltime = 0
prevtotal = 0
offset = 0
ppc = 0
partstart = ''
partsline = ''

ind = 0
(time, tcode) = times[0]

sys.stdout = open(args[2], 'w')

try:
    f = simplefilebuf(args[1])
except IOError, (errno, strerror):
    print >> stderr, 'Opening file %s failed: (%s): %s.' \
          % (args[1], errno, strerror)
    sys.exit(1)


while True:
    try:
        skipped += f.dumpto('\377')
    except EOFError:
        print 'cut:', tcode, 'filepos:', f.filepos(), 'sync:', '#EOF!'
        print 'File ended: audio frames:', afn, 'Total time:', totaltime, 'ms.'
        os.write(0, '\r-Scanning audio at %d ms of %d ms (100%%).' \
              % (lasttime, lasttime))
        if partstart:
            if partsline:
                partsline = partsline + ',' + partstart + '-' \
                            + str(f.filepos())
            else:
                partsline = partstart + '-' + str(f.filepos())
        print >> stdout, partsline
        sys.exit(0)

    if skipped > 0:
        print >> stderr, 'Skipped %d bytes of garbage before audio frame %d.' \
              % (skipped, afn+1)
        skipped = 0

    cpc = totaltime * 100 / lasttime
    if ppc != cpc:
        os.write(0, '\r- Scanning audio at %d ms of %d ms (%d%%)' \
              % (totaltime, lasttime, cpc))
        ppc = cpc

    if totaltime >= time:
        if (totaltime - time) <= (time - prevtotal):
            pos = f.filepos() - 1
            sync = totaltime - time
        else:
            pos = offset
            sync = prevtotal - time

        if partstart:
            if partsline:
                partsline = partsline + ',' + partstart + '-' + str(pos)
            else:
                partsline = partstart + '-' + str(pos)
            partstart = ''
        else:
            partstart = str(pos)

        print 'cut:', tcode, 'filepos:', pos, 'sync:', sync, 'ms.'

        ind = ind + 1
        if ind >= len(times):
            os.write(0, '.\n')
            print >> stdout, partsline
            sys.exit(0)
        time = times[ind][0] + sync
        tcode = times[ind][1]

    offset = f.filepos() - 1

    x = struct.unpack('BBB', f.needbytes(3))

    # print '>>>', x, ' %x' % x[1]

    #                    84218421 84218421 84218421 84218421
    #                    76543210 76543210 76543210 76543210
    # Mpeg audio header: AAAAAAAA AAABBCCD EEEEFFGH IIJJKLMM

    # Check that we have header (frame sync: 11 bits set (A above).)

    if (x[0] & 0xe0) == 0xe0:
        afn += 1
        id = x[0] & 0x18  #  B above	(mpeg audio version id)
        ld = x[0] & 0x06  #  C above	(layer description)
        pb = x[0] & 0x01  #  D above	(Protection bit)

        bi = x[1] & 0xf0  #  E above	(bitrate index)
        si = x[1] & 0x0c  #  F above	(sampling rate frequency index)
        pad= x[1] & 0x02  #  G above	(padding bit)
        pri= x[1] & 0x01  #  H above	(private bit)

        cm = x[2] & 0xc0  #  I above	(channel mode)
        me = x[2] & 0x30  #  J above	(mode extension)
        c  = x[2] & 0x08  #  K above	(copyright)
        o  = x[2] & 0x04  #  L above	(original)
        e  = x[2] & 0x03  #  M above	(emphasis)

        if id != 0x18:
            print >> stderr, \
                  'Frame', afn, '(pos %d) not mpeg version 1.' % f.filepos()
            sys.exit(1)

        if ld != 0x04:
            print >> stderr, \
                  'Frame', afn, '(pos %d) not mpeg1 layer 2.' % f.filepos()
            sys.exit(1)

        flib = 144000 * bidict[bi] / sidict[si] + (pad >> 1);
        ms = flib * 8 / bidict[bi]

        if options.verbose:
            print 'Frame %d: bitrate %d, sample rate %d, pad %d, '\
                  'bytes %d, %d ms of audio' \
                  % ( afn, bidict[bi], sidict[si], pad, flib, ms )

        #print 'private bit', pri

        #print 'channel mode', cm
        #print 'mode extension', me
        #print 'copyright', c
        #print 'original', o
        #print 'emphasis', e

        prevtotal = totaltime
        totaltime += ms

	try:
            f.needbytes(flib - 4)
        except EOFError:
            print 'File ended: audio frames:', afn, \
                  'Total time:', totaltime, 'ms.' # XXX total time incorrect.
            print 'Last frame:', f.len + 4, 'bytes.'
            sys.exit(0)
    else:
        f.unusedbytes(3)

#EOF
