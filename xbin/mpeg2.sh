#!/bin/sh
# re-encode mpeg2 video using mpeg2enc, with original mp2 audio
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2008 Tomi Ollila
#	    All rights reserved
#
# Created: Mon Aug 18 18:54:24 EEST 2008 too
# Last modified: Fri 13 Feb 2015 19:32:32 +0200 too


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

saved_IFS=$IFS
readonly saved_IFS

src=$M2VMP2CUT_MEDIA_DIRECTORY

denoisefilt=
deintfilter=

filters="$denoisefilt $deintfilter"

RUNTIME_DIR=
case ${XDG_RUNTIME_DIR-} in '' | *["$IFS"]*) ;; *)
	if test -d $XDG_RUNTIME_DIR -a test -x $XDG_RUNTIME_DIR
	then RUNTIME_DIR=$XDG_RUNTIME_DIR
	fi
esac
case ${RUNTIME_DIR} in '')
	uid () { uid=$2; }
	IFS="=($IFS"
	uid `/usr/bin/id`
	IFS=$saved_IFS
	RUNTIME_DIR=/tmp/runtime-$uid
	mkdir $RUNTIME_DIR 2>/dev/null || :
	chmod 700 $RUNTIME_DIR
esac

fifo_video=$RUNTIME_DIR/fifo.video.$$
fifo_audio=$RUNTIME_DIR/fifo.audio.$$

trap "rm -f $fifo_video $fifo_audio" 0
mkfifo $fifo_video $fifo_audio

$M2VMP2CUT_CMD_PATH/getmp2.sh "$src" > $fifo_audio &

eval "$M2VMP2CUT_X_PATH/ffgetyuv.pl $filters" | \
	mpeg2enc -f 3 -a $a -b 2000 -R 2 -K kvcd -s -o $fifo_video 2>&1 &

mplex -f 8 -o out.mpg $fifo_video $fifo_audio

echo result is in './out.mpg'
