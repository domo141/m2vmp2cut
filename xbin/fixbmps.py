#!/usr/bin/env python
# -*- coding: utf-8 -*-
# bmps with odd width may have "tilted" content; fix w/ user help
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2015 Tomi Ollila
#	    All rights reserved
#
# Created: Wed 28 Jan 2015 18:01:46 EET too
# Last modified: Sat 14 Feb 2015 12:13:27 +0200 too

import os
import sys
import glob
import struct

# Die as exception class is just for emacs indentation (as extra 'pass'es).
class Die(Exception):
    def __init__(self, *_list):
        print >> sys.stderr
        print >> sys.stderr, _list[0] % _list[1:]
        print >> sys.stderr
        raise SystemExit(1)
    pass

skip_pfx = ''
class Skip(Exception):
    def __init__(self, *_list):
        global skip_pfx
        if len(skip_pfx):
            print >> sys.stderr, skip_pfx,
            skip_pfx = ''
        print >> sys.stderr, _list[0] % _list[1:],
        print >> sys.stderr, "Skipping."
    pass

def pread(f, l, n):
    d = f.read(n);
    if len(d) == 0: raise EOFError
    l.append(d)
    return d

def dobmp(infile, outfile):
    f = file(infile)
    l = list()

    global skip_pfx
    skip_pfx = infile + ':'

    # http://en.wikipedia.org/wiki/BMP_file_format

    ## Bitmap file header

    if pread(f, l, 2) != 'BM':
        raise Skip("BM header missing.")

    file_size = struct.unpack('<L', pread(f, l, 4))[0]
    pread(f, l, 4) # skip reserved data
    bitmap_offset = struct.unpack('<L', pread(f, l, 4))[0]
    #print file_size, bitmap_offset

    ## DIB header (bitmap information header)

    dib_size = struct.unpack('<L', pread(f, l, 4))[0]
    # this is the only we support (for now, hopefully ever)
    if dib_size != 40:
        raise Skip("DIB header size not 40")
    # expect Windows BITMAPINFOHEADER data
    bitmap_width = struct.unpack('<L', pread(f, l, 4))[0]

    # skip creating output file in case bitmap width is even
    if not bitmap_width & 1:
        return 0

    bitmap_height = struct.unpack('<L', pread(f, l, 4))[0]
    if struct.unpack('<H', pread(f, l, 2))[0] != 1:
        raise Skip("# of color planes is not 1.")
    bits_per_pixel = struct.unpack('<H', pread(f, l, 2))[0]

    #print bitmap_width, bitmap_height, bits_per_pixel

    if bits_per_pixel != 8:
        raise Skip("bits per pixel '%d' not 8", bits_per_pixel)

    compression_method = struct.unpack('<L', pread(f, l, 4))[0]
    if compression_method != 0:
        raise Skip("compression method '%d' not 0", compression_method)

    pread(f, l, 20) # skip rest of DIB

    ## bitmap data.
    pread(f, l, bitmap_offset - 14  - 40) # skip to bitmap data

    rw = (bitmap_width + 3) & ~3
    if rw - bitmap_width == 1:    # padded 2 too much
        rc = rw + 2
        skip = True
        pass
    elif rw - bitmap_width == 3:  # padded 2 too little
        rc = rw - 2
        skip = False
        pass
    else:
        raise Skip("Programmer error. '%d' not odd width.", bitmap_width)

    print "File '%s' has odd width (%d). Writing '%s'" % \
        (infile, bitmap_width, outfile)

    o = file(outfile, "w")
    o.writelines(l)
    del l

    #print bitmap_width, rw, rc
    try:
        while True:
            line = f.read(rc)
            if len(line) == 0: raise EOFError
            if skip:
                o.write(line[:-2])
            else:
                o.write(line)
                o.write("\000\000")
            pass
        pass
    except EOFError:
        pass

    return 1

def visrename(old, new):
    print "Renaming '{}' to '{}'".format(old, new)
    os.rename(old, new)
    pass

def yesno(text):
    while True:
        sys.stdout.write(text + " (yes/no)? ")
        ans = sys.stdin.readline()
        if ans.lower() == "yes\n": return 1
        if ans.lower() == "no\n": return 0
        print "Please answer 'yes' or 'no'\n"
        pass
    pass

if __name__ == '__main__':
    feh = os.environ.get("M2VMP2CUT_MEDIA_DIRECTORY", 0)
    if feh:
        # more hax (done after the hax below...)
        sys.argv = [ sys.argv[0], 'feh', feh + '/in1' ]
        pass
    if len(sys.argv) > 1 and sys.argv[1] == 'feh':
        # a hax (good enough for the time being)
        feh = sys.argv[0]
        sys.argv = sys.argv[1:]
        sys.argv[0] = feh
        feh = 1
        pass
    if len(sys.argv) != 2:
        raise Die("Usage: %s [feh] {directory}", sys.argv[0])
    os.chdir(sys.argv[1])
    for fn in glob.iglob("*.bmp"):
        if fn.endswith("-mod.bmp") or fn.endswith("-old.bmp"):
            print "Skipping '%s'" % fn
            continue
        try:
            nfn = fn.replace(".bmp", "-mod.bmp")
            if dobmp(fn, nfn) and feh:
                print "Press 'q' on top of feh window to close the picture window"
                os.spawnlp(os.P_WAIT, "feh", "feh", fn)
                os.spawnlp(os.P_WAIT, "feh", "feh", nfn)
                if yesno("Do you want to replace the first one "
                         "with the second"):
                    visrename(fn, fn.replace(".bmp", "-old.bmp"))
                    visrename(nfn, fn)
                    pass
        except Skip:
            pass

    pass  # pylint: disable=W0107
