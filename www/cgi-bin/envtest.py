#!/usr/bin/env python3
import os
 
print("Content-Type: text/plain\r\n\r\n", end="")
for key in sorted(os.environ):
    print("{}={}".format(key, os.environ[key]))
 
