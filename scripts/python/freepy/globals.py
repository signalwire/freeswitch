
import os

if os.environ.has_key('FREEPY_DEBUG_ON'):
    # pull from environment if avail
    FREEPY_DEBUG_ON = os.environ['FREEPY_DEBUG_ON']
else:
    # fall back to hardcoded value
    #FREEPY_DEBUG_ON = False
    FREEPY_DEBUG_ON = True

def debug(msg):
    if FREEPY_DEBUG_ON:
        print msg    
