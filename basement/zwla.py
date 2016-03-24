#!/usr/bin/python3
#
# Generate time-based histogram data form zwave installation log files.
# Plot with gnuplot.
# Fill style reference,
#   http://gnuplot.sourceforge.net/demo/fillstyle.html

import sys
import os
from glob import glob
import string
# import datetime
from datetime import datetime, timedelta

def sort_and_show(dict, fmt, td, output):
    keys = sorted(dict.keys())
    t = datetime.strptime(keys[0], fmt)
    tlast = datetime.strptime(keys[-1], fmt)
    while (t < tlast):
        t += td
        tk = t.strftime(fmt)
        if tk in keys:
            output.write("%s %s\n" % (tk, dict[tk]))
        else:
            print(tk, 0)

def plot(fmt, _input, output, style):
    p = os.popen('gnuplot', 'w')    
    p.write("""
set timefmt "%s"
set xdata time
set terminal png
set output "%s"
set style %s
plot '%s' using 1:2 with boxes
    """ % (fmt, output, style, _input))
    p.close()


def main():
    os.chdir("/home/guinan/tmp/")
    byhour = {}
    byday = {}
    target = 'presumed'
    for f in glob("zwctrl.log*"):
        for line in open(f).readlines():
            if line.find(target) == -1:
                continue
            day,hms = line.split()[:2]
            if not day in byday:
                byday[day] = 0
            byday[day] += 1
            h,m,s = hms.split(':')
            #print(day)
            dh = day + '.' + h
            if not dh in byhour:
                byhour[dh] = 0
            byhour[dh] += 1
            #dt = datetime.datetime.strptime(" ".join(), "%Y-%m-%d %H:%M:%S.%f")
            #print(dt)

    f = open('byhour.dat', 'w')
    sort_and_show(byhour, "%Y-%m-%d.%H", timedelta(hours=1), f)
    f.close()

    plot("%Y-%m-%d.%H", 'byhour.dat', "/var/www/bluebutton/htdocs/logs/zwave-byhour.png", "fill solid 1.0")
    
    f = open('byday.dat', 'w')
    sort_and_show(byday, "%Y-%m-%d", timedelta(days=1), f)
    f.close()

    plot("%Y-%m-%d", "byday.dat", "/var/www/bluebutton/htdocs/logs/zwave-byday.png", "fill solid 1.0 border -1")
    

main()
