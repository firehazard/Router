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
pub_urls = ['http://labs.google.com/', 'HTTP://www.stanford.edu/', 
               'http://WWW.microsoft.com/en/us/default.aspx', 
               'http://www.cnn.com/',
               'http://yuba.stanford.edu/vns/images/su.gif'] 

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
     cid = os.spawnl(os.P_NOWAIT, proxy_bin, proxy_bin, port)
     # Give the proxy time to start up and start listening on the port
     time.sleep(2)
     proxy_map = {'http':'http://localhost:' + port}

     count = ""
     foblist = []
     for url in pub_urls:
		  #print("./proxyget -U localhost:" + port +" "+url+" 1> outfile"+count+".txt")
		  os.system("./proxyget -U localhost:" + port +" "+url+" 1> outfile"+count+".txt") 
		  f = open("outfile"+count+".txt", "r")
		  foblist.append(f)
                  count+="x"
		
          
     outfile = open('log1.txt', 'w')
     outfile2 = open('log2.txt', 'w')
     passcount = 0
     for n in range(len(foblist)):
          proxy_file = foblist[n]
          outfile.write(proxy_file.read())
          direct_file = urllib.urlopen(pub_urls[n])
          outfile2.write(direct_file.read())

     outfile.close()
     outfile2.close()
          

     # Cleanup
     os.kill(cid, signal.SIGINT)
     os.kill(cid, signal.SIGKILL)
     os.waitpid(cid, 0)

   
def usage():
     print "Usage: proxy_grader.py path/to/proxy/binary port"


if __name__ == '__main__':
     main()
