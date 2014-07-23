#!/usr/bin/env python
#$Rev: 1265 $
#$LastChangedDate: 2007-01-16 19:30:08 -0800 (Tue, 16 Jan 2007) $

import urllib
import sys
import os
import signal
import time
try:
     import proxy_grade_private
     use_private = True
except ImportError:
     use_private = False

# You can create additional automated tests for your proxy by
# adding URLs to this array.  This will have no effect on your
# grade, but may be helpful in testing and debugging your proxy.
pub_urls = ['http://WWW.cnn.com'] 

def main():
     global pub_urls
     try:
          proxy_bin = sys.argv[1]
          port = sys.argv[2]
     except IndexError:
          usage()
          sys.exit(2)

     print 'Binary: %s' % proxy_bin
     print 'Running on port %s' % port

     # Start the proxy running in the background
     cid = os.spawnl(os.P_NOWAIT, proxy_bin, proxy_bin, '-U', '-t','225','-s', 'vns-2.stanford.edu', '-v', 'vhost', '-r', 'rtable.vhost', port)
     # Give the proxy time to start up and start listening on the port
     time.sleep(2)
     proxy_map = {'http':'http://192.168.129.107:' + port}

     foblist = []
     for url in pub_urls:
          foblist.append(urllib.urlopen(url, None, proxy_map))

     passcount = 0
     for n in range(len(foblist)):
          proxy_file = foblist[n]
          lines = proxy_file.readlines()
          if (len(lines) == 0):
               print "No data received for " + pub_urls[n]
               continue

          direct_file = urllib.urlopen(pub_urls[n])
          direct_lines = direct_file.readlines()

          fail = False
          for (prox_line, dir_line) in zip(lines, direct_lines):
               if prox_line != dir_line:
                    print '>>> ' + prox_line
                    print '<<< ' + dir_line
                    direct_file.close()
                    fail = True
                    break

          direct_file.close()
          if not fail:
               passcount += 1

     for fob in foblist:
          fob.close()

     print r'Basic HTTP transactions: %d of %d tests passed' % (passcount, len(pub_urls))


     if (use_private):
          priv_passed = proxy_grade_private.runall(port, cid)


     # Cleanup
     os.kill(cid, signal.SIGINT)
     time.sleep(2)
     os.kill(cid, signal.SIGKILL)
     os.waitpid(cid, 0)

     print 'Summary: '
     print '\t%d of %d tests passed.' % (passcount, len(pub_urls))
     score = 0
     if (use_private):
          if (priv_passed):
               print '\t100% of extended tests passed.'
               score += 1
          else:
               print'\tNot all extended tests passed.'

     score += (float(passcount) / float(len(pub_urls))) * 8
     print 'Base Score: %d/10 (Remember, one point is based on style!)' % score

def usage():
     print "Usage: proxy_grader.py path/to/proxy/binary port"


if __name__ == '__main__':
     main()
