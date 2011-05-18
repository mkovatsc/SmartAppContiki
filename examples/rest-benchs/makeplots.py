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

def getEnergy(cpu, lpm, transmit, listen, fread, fwrite):
	energy = transmit * txPower + listen * rxPower + cpu * cpuPower + lpm * lpmPower + fread * freadPower + fwrite * fwritePower
	return energy
	
def getPower(cpu, lpm, transmit, listen, fread, fwrite):
	time = cpu + lpm
	return getEnergy(cpu, lpm, transmit, listen, fread, fwrite)/ float(time)

def getResults(fileName):
	file = open(fileName, 'r')
	lines = file.readlines()
	
	if len(lines) == 0:
		return None
		
	latencyList = []
	nodeEnergyList = []
	
	# one run per line
	for line in lines:
		results = line.split()
		
		# latency of run
		latencyList.append( float(results[0]) )
		
		i = 0
		# Line format
		# latency	(\(#id\)	(cpu)	(lpm)	(tx)	(rx)	void	void	?)+
		for id in range(1, 29, 7):
			
			if id >= len(results):
				break
			
			print "  %s (%i)" % (results[id], i)
			
			if i >= len(nodeEnergyList):
				nodeEnergyList.append([])
			
			nodeEnergyList[i].append( getEnergy(float(results[id+1]), float(results[id+2]), float(results[id+3]), float(results[id+4]), 0, 0) )
			
			i += 1
	
	print "  nodes/hops: %u" % (len(nodeEnergyList))
	
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
	plotsDir = "plots"
	overallResults = {}
	
	payloads = []

	dataDir = "benchs"
	
	if os.path.exists(dataDir):
		for file in os.listdir(dataDir):
			print file
			splitted = file.split("_")
			if len(splitted) == 3:
				hops = getval(splitted[0], "hops")
				payload = getval(splitted[1], "payload") 
				rdc = getvalStr(splitted[2], "rdc")
				results = getResults(os.path.join(dataDir, file))
				if results != None:
					payloads.append(payload)
					key = (hops, payload, rdc)
					overallResults[key] = results
	
	#create plot files
	if not os.path.exists(plotsDir):
		os.makedirs(plotsDir)
	
	file = open(os.path.join(plotsDir, 'hops4_rdccontikimac.txt'), 'w')
	
	# Header
	file.write('payload	latency	energy2	energy3	energy4	energy5\r\n');
	
	for p in sorted(payloads):
		print "payload: %u" % p
		
		file.write("%u	%f	" % (p, overallResults[(4, p, 'contikimac')]['latency']['mean']))
		
		for i in range(0, len(overallResults[(4, p, 'contikimac')]['nodes']), 1):
			file.write("%f	" % (overallResults[(4, p, 'contikimac')]['nodes'][i]['mean']))
		
		file.write("\r\n");
	
	file.close()
	
	return
						
main()
