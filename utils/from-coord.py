#!/usr/bin/python
import sys

def round_down(x):
    r = x % 16
    return x - r

while True:
    line = sys.stdin.readline()
    if line == "":
        break
    things = line.split()
    for a in things:
        x,y = map(round_down,map(int,a.split(',')))
        print 16 * (y / 32) + x / 32 + 1
