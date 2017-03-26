What is it?
-----------

This Project is the successor of [shntool](http://www.etree.org/shnutils/shntool/) and aims to provide support of modern high definition audio formats and new features.


shdtool is a multi-purpose WAVE data processing and reporting utility. File formats are abstracted from its core, so it can process any file that contains WAVE data, 
compressed or not - provided there exists a format module to handle that particular file type.

shdtool has native support for .wav files. If you want it to work with other lossless audio formats, you must have the appropriate helper program installed. 
The "Helper programs" section below contains links to helper programs for each format that shdtool supports.


Documentation
-------------

A description of shdtool's modes and command-line arguments are contained in the man page.


Helper programs
---------------

Here are some links to various helper programs you may need. For full functionality with shntool, make sure to use the patched version of any helper program if your platform appears under the "patched for" column.

Windows users: Installation of helper programs is easy. Simply copy the appropriate helper program to the same directory where shntool.exe is installed, or any other directory in your PATH.



Format | Helper program                                             | Read | Write
-------|------------------------------------------------------------|------|------
aiff   | [SOX](http://sox.sourceforge.net/)                         | yes  | yes
alac   | [ALAC](http://craz.net/programs/itunes/alac.html)          | yes  | no
als    | [mp4als](http://www.nue.tu-berlin.de/menue/forschung/projekte/beendete_projekte/mpeg-4_audio_lossless_coding_als/) | yes | yes
ape    | [mac-port](http://supermmx.org/linux/mac/)                 | yes  | yes
bonk   | [Bonk](http://www.logarithmic.net/pfh/bonk)                | yes  | yes
flac   | [FLAC](http://flac.sourceforge.net/)                       | yes  | yes
kxs    | [Kexis](http://www.sourceforge.net/projects/kexis/)        | yes  | no
la     | [La](http://www.lossless-audio.com/)                       | yes  | no
lpac   | [LPAC](http://www.nue.tu-berlin.de/wer/liebchen/lpac.html) | yes  | no
mkw    | [mkwcon](http://www.etree.org/shnutils/mkwcon/)            | yes  | yes
ofr    | [OptimFROG](http://www.losslessaudio.org/)                 | yes  | yes
shn    | [shorten](http://www.etree.org/shnutils/shorten/)          | yes  | yes
tak    | -                                                          | yes  | yes
tta    | [TTA](http://sourceforge.net/projects/tta/)                | yes  | yes
wav    | -                                                          | yes  | yes
wv     | [WavPack](http://www.wavpack.com/)                         | yes  | yes

