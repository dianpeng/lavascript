#!/usr/bin/env python

import os
import subprocess

def is_executable(path):
    return os.access(path,os.X_OK)

def run_file(path):
    failed = []
    for a,_,fn in os.walk(path):
        for f in fn:
            fname = os.path.join(a,f)
            if is_executable(fname):
                print('***********************************************************')
                print('                       {0}'.format(fname))
                if subprocess.call([fname]) != 0:
                    failed.append(fname)
                print('***********************************************************')
    print('-----------------------------------------------------')
    print('                       FAILED                        ')
    print( '\n'.join(failed) )
    print('-----------------------------------------------------')

if __name__ == "__main__":
    run_file("unittest")
