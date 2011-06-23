#!/usr/bin/python

import sys
import os
import time
import signal
import subprocess
import datetime
import re
from datetime import datetime
from datetime import timedelta
from math import sqrt

debugMode = False

resCount = 0
cancelCount = 0
failCount = 0
overallResCount = 0
benchLaunched = False

negCount = 0
negLog = "test_neg.txt"

def do_exit(sig, stack):
    print "Killing processes"
    #killBench()
    raise SystemExit('Exiting')

def runFg(command):
    if debugMode:
        print "> " + command
    cmd = command.split(" ")
    if os.path.exists(".log"):
        os.remove(".log")
    outFile = open(".log", 'w')
    try:
        process = subprocess.Popen(cmd, stdout=outFile, stderr=outFile)
        status = os.waitpid(process.pid, 0)
    except OSError, e:
        return None
    outFile.close()
    outFile = open(".log", 'r')
    lines = outFile.readlines()
    outFile.close()
    return {"status": status[1], "log": lines}

def runFg2(cmd, out):
    if debugMode:
        print "> %s > %s" %(cmd, out)
    return os.system("%s 1> %s 2>> /dev/null" %(cmd, out))

def runBg(cmd):
    if debugMode:
        print "> %s &" %(cmd)
    return os.system("%s 1>> /dev/null 2>> /dev/null &" %(cmd))

def runBg2(cmd, out):
    if debugMode:
        print "> %s > %s &" %(cmd, out)
    return os.system("%s 1> %s 2>> /dev/null &" %(cmd, out))

        
def getLatency(log):
    global negCount
    lastline = log[len(log)-2]
    print lastline
    if re.search("-", lastline):
        print "Negative RTT!"
        negCount += 1
        if os.path.exists(negLog):
            neg_file = open(negLog, 'a')
        else:
            neg_file = open(negLog, 'w')
        neg_file.write("".join(log))
        neg_file.close()
        return None
    
    try:
        return int(lastline.split(":")[1])
    except:
        print "Latency parse error:"
        print str(log)
        return None

def onerun():
    global failCount
    
    #ret = runFg("java -cp java_tools/bin COAPClient 6 sky%d 61616 get hello %d" %(hops+1, size))
    ret = runFg("java -jar java_tools/bin/SampleClient.jar POST coap://vs0.inf.ethz.ch/toUpper test")
    
    if ret["status"] != 0:
        print "Bench failed"
        print ret["log"]
        return
    if re.search("Request timed out", ret["log"][len(ret["log"])-2]):
        print "No reply"
        failCount += 1
        return
                
    latency = getLatency(ret["log"])
    if latency == None:
        print "Failed to get latency information!"
        return None
    
    return {"latency": latency}

def main():

    # init
    signal.signal(signal.SIGINT, do_exit)

    for niter in range(1000):
        print "\n() Iteration #%d, results: %d (new: %d, aborted: %d, fails: %d, negs: %d)" %( niter+1, overallResCount, resCount, cancelCount, failCount, negCount)
        onerun()
        
main()
