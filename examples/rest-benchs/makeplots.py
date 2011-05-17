#!/usr/bin/python

import sys
import os
from math import sqrt

voltage = 3
txPower = 0.001 * 17.4 * voltage
rxPower = 0.001 * 19.7 * voltage
freadPower = 0.001 * 4 * voltage
fwritePower = 0.001 * 20 * voltage
cpuPower = 0.001 * 1.8 * voltage
lpmPower = 0.001 * .0545 * voltage

def meanstdv(x):	
	n, mean, stdev = len(x), 0, 0
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

def getDutycycle(cpu, lpm, transmit, listen):
	return 100 * (transmit + listen) / (cpu + lpm)

def getPower(cpu, lpm, transmit, listen, fread, fwrite):
	time = cpu + lpm
	energy = transmit * txPower + listen * rxPower + fread * freadPower + fwrite * fwritePower + cpu * cpuPower + lpm * lpmPower
	return energy / float(time)

def getResults(fileName):
	file = open(fileName, 'r')
	lines = file.readlines()
	if len(lines) == 0:
		return None
	latencyList = []
	for line in lines:
		results = line.split()
		latency = float(results[0])
		latencyList.append(latency)
	return {"latency": meanstdv(latencyList)}

def getval(str, token):
	str = str.split(".")[0]
	idx = str.find(token) + len(token)
	return int(str[idx:])

def getvalStr(str, token):
	str = str.split(".")[0]
	idx = str.find(token) + len(token)
	return str[idx:]

def main():
	plotsDir = "plots"
	overallResults = {}

	dstDir = "benchs"
	if os.path.exists(dstDir):
		for file in os.listdir(dstDir):
			print file
			splitted = file.split("_")
			if len(splitted) == 3:
				hops = getval(splitted[0], "hops")
				payload = getval(splitted[1], "payload") 
				rdc = getvalStr(splitted[2], "rdc")
				results = getResults(os.path.join(dstDir, file))
				if results != None:
					key = (hops, payload, rdc)
					overallResults[key] = results
	
	#create plot files
	if not os.path.exists(plotsDir):
		os.makedirs(plotsDir)
	
	print overallResults
	return
						
main()
