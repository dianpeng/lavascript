#!/usr/bin/env python

import os
from subprocess import Popen, PIPE

def is_executable(path):
    return os.access(path,os.X_OK)

def run_file(path):
    failed = []
    count  = 0
    for a,_,fn in os.walk(path):
        for f in fn:
            fname = os.path.join(a,f)
            if is_executable(fname):
                print('***********************************************************')
                print("**{0}. {1}".format(count,fname))

                p = Popen([fname], stdout=PIPE, stderr=PIPE)
                out,err = p.communicate()

                if p.returncode != 0:
                    print(out)
                    print(err)
                    failed.append(fname)

                count = count + 1
                print('***********************************************************')


    print('-----------------------------------------------------')
    print('                       Stat                          ')
    print("runned: {0}".format(count))
    print("failed: {0}".format(';'.join(failed)))
    print('-----------------------------------------------------')

if __name__ == "__main__":
    run_file("unittest")
