#!/usr/bin/python

import sys
import os
import time
import signal
import subprocess
import datetime
from datetime import datetime
from datetime import timedelta
from math import sqrt

debugMode = False

resCount = 0
cancelCount = 0
overallResCount = 0
benchLaunched = False

compilationNeeded = True
resetNeeded = False

contikiPath = "../.."

def meanstdv(x):    
    n, mean, stdev = len(x), 0, 0
    if n == 0:
        return None
    for a in x:
        mean = mean + a
    mean /= float(n)
    for a in x:
        stdev = stdev + (a - mean)**2
    stdev = sqrt(stdev / float(n))
    if stdev > mean * 0.8:
        print "Warning, very high stdev: ",
        for a in x:
            print "%.2f"%a,
        print ""
    return {"mean": mean, "stdev": stdev}

def do_exit(sig, stack):
    print "Killing processes"
    killBench()
    raise SystemExit('Exiting')

def killBench():
    global benchLaunched
    runFg("killall -q make")
    runFg("killall tunslip6")
    runFg("killall serialdump-linux")
    benchLaunched = False

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

def getTty(mote):
    try:
        tty = runFg("../../tools/sky/motelist-linux")["log"][mote+1].split()[1]
    except:
        tty = None
        
    if debugMode:
        print "Getting mote %u: %s" % (mote, tty)
    return tty
        
def getLatency(log):
    lastline = log[len(log)-1]
    try:
        return int(lastline.split(":")[1])
    except:
        return None

def getPower(hops):
    try:
        res = {}  
        for mote in range(hops):
            logfile = "dump%d.log" %(mote+2)
            lines = open(logfile, 'r').readlines()
            powerLine = lines[len(lines)-3].split("(")[0]
            powerLine.split()
            (start, clock_time, P, addr, seqno,
             all_cpu, all_lpm, all_transmit, all_listen, all_idle_transmit, all_idle_listen,
             cpu, lpm, transmit, listen, idle_transmit, idle_listen) = powerLine.split() 
            res[mote+2] = (cpu, lpm, transmit, listen, idle_transmit, idle_listen)
        return res
    except:
        return None

def onerun(hops, size, rdc):
    global compilationNeeded
    global resetNeeded
    global benchLaunched
    
    print "Preparing bench"
    if compilationNeeded:
        print "-- Compiling"
        killBench()
        compileCmd = "make RDC=%s" %(rdc+"_driver")
        runFg("make clean")
        if runFg(compileCmd)["status"] != 0:
            print "Compilation failed: %s" %(compileCmd)
            return
        print "-- Programming servers"
        compileCmd2 = compileCmd + " rest-server.upload" 
        if runFg(compileCmd2)["status"] != 0:
            print "Programmation failed: %s" %(compileCmd2)
            return
        print "-- Programming border router"
        compileCmd2 = compileCmd + " border-router.upload MOTE=1" 
        if runFg(compileCmd2)["status"] != 0:
            print "Programmation failed: %s" %(compileCmd2)
            return
        compilationNeeded = False
        resetNeeded = False

    if resetNeeded:
        print "-- Resetting"
        killBench()
        for mote in range(hops):
            runFg("../../tools/sky/msp430-bsl-linux --telosb -c /dev/ttyUSB0 -r")
        resetNeeded = False
        benchLaunched = False
        
    if not benchLaunched:
        print "Preparing network"
        runBg("make connect-router")
        time.sleep(3)
        ret = runFg("ping6 sky5 -c1 -w30")
        if ret["status"] != 0:
            print "Ping failed"
            return
        time.sleep(3)
        benchLaunched = True
    
    print "Starting execution"
    
    runFg("killall serialdump-linux")
    runFg("rm dump*.log")
    for mote in range(hops):
        ret = runBg2("../../tools/sky/serialdump-linux -b115200 %s" %(getTty(mote+2)), "dump%d.log"%(mote+2))
        
    time.sleep(1)        
    for mote in range(hops):
        runFg2("echo 'powertrace on'", getTty(mote+2))

    ret = runFg("java -cp java_tools/bin COAPClient 6 sky%d 61616 get hello %d" %(hops+1, size))
    if ret["status"] != 0:
            print "Bench failed"
            print ret["log"]
            return
        
    for mote in range(hops):
        runFg2("echo 'powertrace off'", getTty(mote+2))
    time.sleep(1)
    runFg("killall serialdump-linux")
                
    latency = getLatency(ret["log"])
    if latency == None:
        print "Failed to get latency information!"
        return None
    
    power = getPower(hops)
    
    if power == None:
        print "Failed to get compower information!"
        return None
    
    return {"latency": latency, "power": power}

def dobench(hops, size, rdc, niter, dstDir):
    global resCount
    global overallResCount
    global cancelCount
    global compilationNeeded
    global resetNeeded

    dstFile = os.path.join(dstDir, "hops%d_payload%d_rdc%s.txt" %(hops, size, rdc))

    nres = 0;
    if os.path.exists(dstFile):
        result_file = open(dstFile, 'r+')
        nres = len(result_file.readlines())
        if nres >= niter:
            print "File %s already done (%d results), no need to bench\n" %(dstFile, nres)
            overallResCount += 1
            return
        else:
            print "File %s needs %d more result(s)" %(dstFile, niter-nres)
    else:
        result_file = open(dstFile, 'w')
    
    print "Starting benchmarks: hops: %d, size: %d, rdc %s" %(hops, size, rdc)
    
    while nres < niter:
        print "\n(%d,%d,%s) Iteration #%d, results: %d (new: %d, aborted: %d)" %(hops, size, rdc, nres+1, overallResCount, resCount, cancelCount)
        localCancelCount = 0
        ret = onerun(hops, size, rdc)
        if ret != None:
            print "Result:",
            print ret
            #result_file.write("throughput\tcpu\tlpm\ttransmit\tlisten\t\n")
            result_file.write("%d\t" %(ret["latency"]))
            powermap = ret["power"]
            for mote in range(hops):
                power = powermap[mote+2]
                result_file.write("(#%d)\t" %(mote+2))
                for val in power:
                    result_file.write("%s\t" %(val))
            result_file.write("\n")
            result_file.flush()
            nres += 1
            resCount += 1
            overallResCount += 1
            #killBench()
        else:
            print "Cancelled (%d, %d)" %(localCancelCount, cancelCount)
            cancelCount += 1
            localCancelCount += 1
            if localCancelCount > 2:
                resetNeeded = True

    result_file.close()
    print ""

def main():
    global compilationNeeded
    global resetNeeded

    # init
    signal.signal(signal.SIGINT, do_exit)

    # bench
    killBench()

    niter = 50
    
    hopsList1 = [1, 4]
    
    rdcList = ["contikimac", "nullrdc"]
    hopsList2 = [1, 2, 3, 4]   

    sizeList = [0, 77, 78, 512]
    sizeList += range(169, 553, 96)
    sizeList += range(169-1, 553-1, 96)
    sizeList.sort()

    dstDir = "benchs"
    if not os.path.exists(dstDir):
        compilationNeeded = True
        os.makedirs(dstDir)
    else:
        print "WARNING: Assert that the motes are already programmed!"
        compilationNeeded = False
    
    print "\n\nEXPERIMENT 1\n============\n"
    rdc = "contikimac"
    for hops in hopsList1:
        for size in sizeList:
            dobench(hops, size, rdc, niter, dstDir)
    
    print "\n\nEXPERIMENT 2\n============\n"
    size = 64
    for rdc in rdcList:    
        compilationNeeded = True
        for hops in hopsList2:
            dobench(hops, size, rdc, niter, dstDir)
        
main()
