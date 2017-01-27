#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Project X sup timing (taken from pxsup2dast.py -- with fixes).
# too ät iki piste fi

from __future__ import print_function

import sys, struct
try:
    import cStringIO.StringIO as StringIO
except ImportError:
    from io import BytesIO as StringIO
    pass

def xord(c): # for python3 (while keeping python2 portability)
    if type(c) is int:
        return c
    return ord(c)

def pts2ts(pts):
    h = pts / (3600 * 90000)
    m = pts / (60 * 90000) % 60
    s = pts / 90000 % 60
    ms = (pts + 45) / 90 % 1000
    return '%02d:%02d:%02d.%03d' % (h, m, s, ms)

# Implementation matches at least with ProjecX 0.82.1.02
def pxsuptime(supfile, base):
    """extract subtitles from ProjectX-generated .sup file. Picky!"""
    # Here thanks go to ProjetcX source, http://www.via.ecp.fr/~sam/doc/dvd/
    # and gtkspu program in gopchop distribution

    f = open(supfile, 'rb')

    if f.read(2) != b'SP':
        raise Exception("Syncword missing. XXX bailing out.")

    image=1 # 1-based for feh(1) file number counting compatibility
    while True:
        # X.java reads 5 bytes of pts, SubPicture.java writes 4. With
        # 4 bytes 47721 seconds (13.25 hours) can be handled.
        pts, = struct.unpack('<Lxxxx', f.read(8))
        size, pack = struct.unpack('>HH', f.read(4))

        #print pts / 90, 'ms.', size, pack

        data = f.read(pack - 4)
        ctrl = StringIO(f.read(size - pack))

        # parsing control info

        prev = 0
        while True:
            date, = struct.unpack('>H', ctrl.read(2))
            next, = struct.unpack('>H', ctrl.read(2))  # XXX

            while True:
                cmd = ord(ctrl.read(1))
                if cmd == 0x00: # force display:
                    continue
                if cmd == 0x01: # start date (read above)
                    start = date # XXX + previous
                    continue
                if cmd == 0x02: # stop date (read above)
                    end = date  # XXX + previous
                    continue
                if cmd == 0x03: # palette
                    xpalette = ctrl.read(2)
                    continue
                if cmd == 0x04: # alpha channel
                    alpha = ctrl.read(2)
                    continue
                if cmd == 0x05: # coordinates
                    coords = ctrl.read(6)
                    x1 = (xord(coords[0]) << 4) + (xord(coords[1]) >> 4)
                    x2 = ((xord(coords[1]) & 0xf) << 8) + xord(coords[2])
                    y1 = (xord(coords[3]) << 4) + (xord(coords[4]) >> 4)
                    y2 = ((xord(coords[4]) & 0xf) << 8) + xord(coords[5])
                    continue
                if cmd == 0x06: # rle offsets
                    top_field,bottom_field = struct.unpack('>HH', ctrl.read(4))
                    continue
                if cmd == 0xff: # end command
                    break
                else:
                    raise Execption("%d: Unknown control sequence" % cmd)
            if prev == next:
                break
            prev = next

        startpts = pts;
        endpts = pts + end * 900 # (matches .son) other values seen: 1000, 1024

        sptstr = pts2ts(pts)

        width = x2 - x1 + 1
        if width % 2 != 0:
            print("Image %d width (%d) is not divisible by 2." % (image, width),
                  file=sys.stderr, end=' ')
            print("Check %s-%05d.bmp" % (base, image), file=sys.stderr)
        print("image='%d' start='%s' end='%s' x='%d' y='%d' w='%d' h='%d'" %
              (image, sptstr, pts2ts(endpts), x1, y1, width, y2 - y1 + 1))
        image = image + 1

        if f.read(2) != b'SP':
            if len(f.read(1)) == 0: return # EOF
            raise Exception("Syncword missing. XXX bailing out.")

def main():
    if len(sys.argv) < 2:
        print("\nUsage: %s supfile [base]" % sys.argv[0], file=sys.stderr)
        sys.exit(1)

    base = sys.argv[2] if len(sys.argv) > 2 else 'in*'
    pxsuptime(sys.argv[1], base)

# for pychecker(1)
if __name__ == '__main__':
    main()
