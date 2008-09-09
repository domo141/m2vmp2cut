#!/bin/sh
# re-encode to mpeg2, using mpeg2enc. original mp2 audio is muxed in.
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2008 Tomi Ollila
#	    All rights reserved
#
# Created: Mon Aug 18 18:54:24 EEST 2008 too
# Last modified: Mon Aug 18 20:49:45 EEST 2008 too


case $1 in
	4:3) a=2 ;; 16:9) a=3 ;; '') cat <<.

 re-encode to mpeg2. used to reduce bitrates with possibly better quality
 than with requantisation (with aid of filtering). cropping and rescaling
 can give aspect ratio changes (4:3 -> 16:9).

 initial version, most of the planned features missing (not much even tested!).

 current options: (4:3|16:9) <dir>
        <dir>: m2vmp2cut-created directory containing source to be encoded

 currently fixed bitrate: 2000 kb/s

 future options: denoise and deinterlace filters. bitrate option.
                 cropping, padding, aspect ratio changes.
                 quantisation matrix selection (kvcd, tmpgenc, hi-res)

.
exit 0 ;; esac

case $2 in '') exit 1;; esac
test -d "$2" || exit 2;

denoisefilt=
deintfilter=

filters="$denoisefilt $deintfilter"

trap "rm -f fifo.video.$$ fifo.audio.$$" 0
mkfifo fifo.video.$$ fifo.audio.$$

$M2VMP2CUT_CMD_DIRNAME/bin/getmp2.sh "$2" > fifo.audio.$$ &

eval "$M2VMP2CUT_CMD_DIRNAME/bin/getyuv.pl '$2' $filters" | \
	mpeg2enc -f 3 -a $a -b 2000 -R 2 -K kvcd -s -o fifo.video.$$ 2>&1 &

mplex -f 8 -o out.mpg fifo.video.$$ fifo.audio.$$

echo result is in './out.mpg'