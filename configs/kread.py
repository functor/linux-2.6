#!/bin/env python
#
# compare: a tool to compare kernel config files
#
# Marc E. Fiuczynski <mef@cs.princeton.edu>
# Copyright (C) 2006 The Trustees of Princeton University
#
# $Id: kompare,v 1.2 2006/11/30 17:07:03 mef Exp $
#

import sys, re, os

currentconfig = ""
configs = {}

def _config(parts,fb):
    global currentconfig
    currentconfig = parts[1]

def _help(parts,fb):
    global currentconfig
    helptxt = ""
    lineno = 0
    while True:
        line = fb.readline()
        lineno = lineno + 1
        if len(line)==0 or not line[0].isspace():break
        if len(line)>1: line = line.lstrip()
        helptxt = helptxt+line

    configs[currentconfig]=helptxt

def _source(parts,fb):
    filename = "".join(parts[1:])
    if filename[0]=='"' or filename[0]=='\'':
        filename=filename[1:]
    if filename[-1]=='"' or filename[-1]=='\'':
        filename=filename[:-1]
    process(filename)

def _noop(parts,fb):
    pass

keywords = {"config":_config,
            "help":_help,
            "---help---":_help,
            "source":_source,
            "#":_noop}

def process(filename):
    fb = open(filename)
    lineno = 0
    while True:
        line = fb.readline()
        lineno = lineno + 1
        if len(line)==0:break

        line  = line.strip()
        parts = line.split()
        if len(parts)==0:continue

        func  = keywords.get(parts[0],_noop)
        func(parts,fb)

    fb.close()

def gethelp(option):
    if option[:len("CONFIG_")] == "CONFIG_":
        option=option[len("CONFIG_"):]
    helptxt = configs.get(option,"")
    return helptxt

ARCH=os.getenv("ARCH","i386")
process("arch/%s/Kconfig" % ARCH)

if __name__ == '__main__':
    if len(sys.argv) == 1:
        print """USAGE\n%s configoptionname""" % os.path.basename(sys.argv[0])
    else:
        option = sys.argv[1]
        helptxt = gethelp(option)
        print "CONFIG_%s:\n%s" % (option,helptxt)

