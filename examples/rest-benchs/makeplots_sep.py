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

wins = 0
fails = 0

def meanstdv(x):	
	n, mean, stdev = len(x), 0, 0
	
	if int(n)==0:
		return
	
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

def getEnergy(cpu, lpm, transmit, listen, fread, fwrite):
	energy = (transmit * txPower + listen * rxPower + cpu * cpuPower + lpm * lpmPower + fread * freadPower + fwrite * fwritePower) / float(4096*8)
	return energy
	
def getPower(cpu, lpm, transmit, listen, fread, fwrite):
	time = cpu + lpm
	return getEnergy(cpu, lpm, transmit, listen, fread, fwrite)/ float(time)

def getResults(fileName, wait):
	global fails, wins

	file = open(fileName, 'r')
	lines = file.readlines()
	
	if len(lines) == 0 or lines[0]=='\n':
		return None
		
	latencyList = []
	nodeEnergyList = []
	
	# one run per line
	for line in lines:
		results = line.split()
		
		if int(results[0]) > wait*100 + 800:
			print fileName
			print "ERROR: RTT %ums out of reasonable bound for %ums" % (int(results[0]), wait*100)
			raw_input()
			continue
		
		# latency of run
		latencyList.append( float(results[0]) )
		
		i = 0
		# Line format
		# latency	(\(#id\)	(cpu)	(lpm)	(tx)	(rx)	void	void	?)+
		for id in range(1, 29, 7):
			
			if id >= len(results):
				break
			
			#print "  %s (%i)" % (results[id], i)
			
			if i >= len(nodeEnergyList):
				nodeEnergyList.append([])
			
			nodeEnergyList[i].append( getEnergy(float(results[id+1]), float(results[id+2]), float(results[id+3]), float(results[id+4]), 0, 0) )
			
			i += 1
	
	#print "  nodes/hops: %u" % (len(nodeEnergyList))
	
	# Calculate mean energy per node over runs
	nodeList = []
	for i in range(0, len(nodeEnergyList), 1):
		nodeList.append(meanstdv(nodeEnergyList[i]))
	
	return {"latency": meanstdv(latencyList), "nodes": nodeList}

def getval(str, token):
	str = str.split(".")[0]
	idx = str.find(token) + len(token)
	return int(str[idx:])

def getvalStr(str, token):
	str = str.split(".")[0]
	idx = str.find(token) + len(token)
	return str[idx:]

def main():
	overallResults = {}
	
	definedWaits = [5, 30, 60, 70, 80, 90, 100, 110, 120, 240]
	
	waits = []

	dataDir = "benchs"
	plotsDir = "plots_20_100"
	
	if os.path.exists(dataDir):
		for file in os.listdir(dataDir):
			#print file
			splitted = file.split("_")
			if len(splitted) == 4:
				hops = getval(splitted[0], "hops")
				ack = getval(splitted[1], "ack")
				wait = getval(splitted[2], "wait") 
				rdc = getvalStr(splitted[3], "rdc")
				if wait in definedWaits:
					results = getResults(os.path.join(dataDir, file), wait)
					if results != None:
						if not wait in waits:
							waits.append(wait)
						key = (hops, ack, wait, rdc)
						overallResults[key] = results
	
	print waits
	
	#create plot files
	if not os.path.exists(plotsDir):
		os.makedirs(plotsDir)
		
	for hops in [2, 4]:
		for a in [0, 1]:
	
			file = open(os.path.join(plotsDir, 'separate_quick_compare_hops%u_ack%u.txt' % (hops, a)), 'w')
			
			# Header
			file.write('wait	latency	energy2	energy3	energy4	energy5\r\n')
			
			for w in sorted(waits):
				
				file.write("%u	%f" % (w, overallResults[(hops, a, w, 'contikimac')]['latency']['mean']))
				
				for i in range(0, len(overallResults[(hops, a, w, 'contikimac')]['nodes']), 1):
					file.write("	%f" % (overallResults[(hops, a, w, 'contikimac')]['nodes'][i]['mean']))
				
				file.write("\r\n");
			
			file.close()
	
	return
						
main()
