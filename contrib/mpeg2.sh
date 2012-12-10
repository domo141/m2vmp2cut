#!/bin/sh
# re-encode mpeg2 video using mpeg2enc, with original mp2 audio
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2008 Tomi Ollila
#	    All rights reserved
#
# Created: Mon Aug 18 18:54:24 EEST 2008 too
# Last modified: Wed 31 Oct 2012 16:42:11 EET too


case $1 in
	4:3) a=2 ;; 16:9) a=3 ;; '') cat <<.

 re-encode to mpeg2. used to reduce bitrates with possibly better quality
 than with requantisation (with aid of filtering). cropping and rescaling
 can give aspect ratio changes (4:3 -> 16:9).

 initial version, most of the planned features missing (not much even tested!).

 current options: (4:3|16:9)

 currently fixed bitrate: 2000 kb/s

 future options: denoise and deinterlace filters. bitrate option.
                 cropping, padding, aspect ratio changes.
                 quantisation matrix selection (kvcd, tmpgenc, hi-res)

.
exit 0 ;; esac

src=$M2VMP2CUT_MEDIA_DIRECTORY

denoisefilt=
deintfilter=

filters="$denoisefilt $deintfilter"

trap "rm -f fifo.video.$$ fifo.audio.$$" 0
mkfifo fifo.video.$$ fifo.audio.$$

$M2VMP2CUT_CMD_PATH/getmp2.sh "$src" > fifo.audio.$$ &

eval "$M2VMP2CUT_CONTRIB_PATH/ffgetyuv.pl $filters" | \
	mpeg2enc -f 3 -a $a -b 2000 -R 2 -K kvcd -s -o fifo.video.$$ 2>&1 &

mplex -f 8 -o out.mpg fifo.video.$$ fifo.audio.$$

echo result is in './out.mpg'
