import fnmatch
import os

for dirpath, dirnames, filenames in os.walk('.'):
  for xcodeproj in fnmatch.filter(dirnames, '*.xcodeproj'):
	build_cmd = "xcodebuild -project " + dirpath + "/" + xcodeproj + " -alltargets"
	print "running: ", build_cmd 
	os.system( build_cmd )
      
print "build_all.py complete."
