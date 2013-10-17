#!/usr/bin/python

from qmf.console import Session
import time

sess = Session()

try:
        broker = sess.addBroker("amqp://localhost:5672")
except:
        print "Connection failed"
        exit(1)

dptcores = sess.getObjects(_class="dptcore", _package="de.bisdn.dptcore")

for dptcore in dptcores:
	print dptcore
        props = dptcore.getProperties()
        for prop in props:
                print prop
        stats = dptcore.getStatistics()
        for stat in stats:
                print stat
        methods = dptcore.getMethods()
        for method in methods:
                print method


if len(dptcores) == 0:
	exit(0)


#for dptcore in dptcores:
#    print dptcore.test("blub")

dptcore = dptcores[0]

dptcore.vethLinkCreate('blab', 'blub')
dptcore.linkAddIP('blub', '3000::1/64')

time.sleep(8)

dptcore.linkDelIP('blub', '3000::1/64')
dptcore.vethLinkDestroy('blab')

sess.close()