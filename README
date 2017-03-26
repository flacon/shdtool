Installation
------------

You will most likely need GNU make to build and install shntool.  So far, the
standard make program is known to fail on NetBSD and Solaris.  If 'make' fails,
try 'gmake'.  If you don't have 'gmake', then you will need to install GNU make.
The latest version of GNU make as of shntool's release is 3.81, and you can
build it from source available here:   http://ftp.gnu.org/gnu/make/

Generic instructions on how to build and install shntool are contained in the
INSTALL file.  Be sure to read it, especially if you have any problems, as it
will contain important notes related to building and installing.  However, for
most people this process will be as simple as:

% ./configure
% make                           (or gmake, if make gives you trouble)
% su -c "make install"           (or install-strip, where strip is supported)

If you want to customize which mode and/or format modules are built, see the
"shntool-specific configure options" section below.


Documentation
-------------

A description of shntool's modes and command-line arguments are contained in
the man page.

To see what changes were made since previous releases, consult the ChangeLog
file.


shntool-specific configure options
----------------------------------

In addition to the standard command-line switches described in the "Basic
Installation" section below, the configure script for shntool also recognizes
the following four options:

  --with-modes=LIST           Specify default modes
  --with-formats=LIST         Specify default file formats
  --with-extra-modes=LIST     Specify additional modes
  --with-extra-formats=LIST   Specify additional file formats

The --with-modes and --with-formats switches are intended to allow you to
selectively compile certain modes and/or file formats from the list of built-in
modules.  They also allow you to alter the order of their appearance in the
modes[] and formats[] arrays internal to shntool, which may be useful if, for
instance, you want to have the default output file format be 'shn' instead of
'wav' for modes that create files (since they default to the first file format
that supports output, which will be 'wav' if the default order is not changed).
For example, if you want support only for WAVE and shorten-compressed files, and
you want 'shn' to be the default output format, then pass '--with-formats=shn,wav'
to the configure script.  Or, suppose you never intend to fix files, and would
only like to view information about them.  Then you can configure shntool to
only support the 'len' and 'info' modes by passing '--with-modes=len,info' to
the configure script.

The --with-extra-modes and --with-extra-formats switches are intended to be used
by module developers as a way to get their module compiled into shntool.  For
example, suppose you create a file format module for the 'bar' format.  Then you
can simply run './configure --with-extra-formats=bar' and your module will be
included in the list of modules to be built.  Mode modules are configured similarly.


==================
Document revision:
==================

$Id: README,v 1.10 2007/01/08 15:26:07 jason Exp $
