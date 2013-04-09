"""
Launching and monitoring a subprocess so that the environment variables can be set.
"""

import subprocess
import atexit
import os
import signal

import logging
logging.basicConfig(format='%(levelname)s:%(asctime)s:%(message)s', filename='plauncher.log',level=logging.DEBUG)
logging.debug("starting plauncher")

plist = []

def exitFunc(plist):
    # clean up processes
    logging.debug("atexit called: exitFunc")
    for p in plist:
        p.terminate()

def stHandler(signum, frame):
    global plist
    exitFunc(plist)

def main(plist):
    # start process
    env = {}
    env["SystemRoot"] = 'C:\\Windows'
    env["PYTHONPATH"] = 'C:\\Python27\\Lib\\site-packages'
    print env
    p = subprocess.Popen(['c:\pypy-1.9\pypy.exe', os.path.expanduser('soundTest1.py')], env=env)

    plist.append(p)

    logging.debug("subprocess started");

    p.wait()

    logging.debug("subprocess ended");

if __name__ == '__main__':
    atexit.register(exitFunc, plist)
    #signal.signal(signal.SIGTERM, stHandler)
    #signal.signal(signal.SIGINT, stHandler)

    main(plist)
