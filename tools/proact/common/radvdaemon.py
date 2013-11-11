#!/usr/bin/python

import basecore
import subprocess
import ipv6prefix


class RAdvd(object):
    """Class for controlling an radvd instance"""
    STATE_STOPPED = 0
    STATE_ANNOUNCING = 1
    radvd_binary = '/sbin/radvd'
    
    def __init__(self, baseCore, devname, conffiledir='.'):
        self.state = self.STATE_STOPPED
        self.baseCore = baseCore
        self.devname = devname
        self.prefixes = []
        self.process = None
        self.conffiledir = conffiledir
        self.conffile = self.conffiledir + '/radvd.' + self.devname + '.conf'
        self.pidfile = '/var/run/radvd/radvd.' + self.devname + '.pid'
    
    def __str__(self, *args, **kwargs):
        return '<RAdvd [' + self.devname + '] ' + '[state: ' + str(self.state) + '] ' + ' >'
    
    def addPrefix(self, prefix):
        if not isinstance(prefix, ipv6prefix.IPv6Prefix):
            print "not of type IPv6Prefix, ignoring"
            return
        if not prefix in self.prefixes:
            self.prefixes.append(prefix)
        
    def delPrefix(self, prefix):
        if not isinstance(prefix, ipv6prefix.IPv6Prefix):
            print "not of type IPv6Prefix, ignoring"
            return
        if prefix in self.prefixes:
            self.prefixes.remove(prefix)

    def start(self):
        if self.process != None:
            self.stop()
        self.__rebuild_config()
        if len(self.prefixes) == 0:
            print 'no prefixes available for radvd, suppressing start ' + str(self)
            return
        self.state = self.STATE_ANNOUNCING
        radvd_cmd = self.radvd_binary + ' -C ' + self.conffile + ' -p ' + self.pidfile
        print 'radvd start: executing command => ' + str(radvd_cmd)
        self.process = subprocess.Popen(radvd_cmd.split())
        self.baseCore.addEvent(basecore.BaseCoreEvent(self, basecore.BaseCore.EVENT_RADVD_START))
    
    def stop(self):
        self.state = self.STATE_STOPPED
        if self.process == None:
            return
        kill_cmd = 'kill -INT ' + str(self.process.pid)
        print 'radvd stop: executing command => ' + str(kill_cmd)
        subprocess.call(kill_cmd.split())
        self.process = None
        self.baseCore.addEvent(basecore.BaseCoreEvent(self, basecore.BaseCore.EVENT_RADVD_STOP))

    def restart(self):
        if self.state == self.STATE_ANNOUNCING:
            self.stop()
        self.__rebuild_config()
        self.start()

    def __rebuild_config(self):
        try:
            f = open(self.conffile, 'w')
            f.write('interface ' + self.devname + ' {\n');
            f.write('    AdvSendAdvert on;\n')
            f.write('    MaxRtrAdvInterval 15;\n')

            for prefix in self.prefixes:
                f.write('    prefix ' + str(prefix.prefix) + '/' + str(prefix.prefixlen) + '\n')
                f.write('    {\n')
                f.write('        AdvOnLink on;\n')
                f.write('        AdvAutonomous on;\n')
                f.write('        AdvValidLifetime 3600;\n')
                f.write('        AdvPreferredLifetime 3600;\n')
                f.write('        DeprecatePrefix on;\n')
                f.write('    };\n')
                f.write('\n')
            f.write('};\n')
            f.close()
        except IOError:
            pass


