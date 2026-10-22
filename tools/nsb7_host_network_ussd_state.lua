-- External host dialogue: restore pending editor, not an injected transaction.
_G.nsb7_ussd_state_host=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'nsb7_network_ussd_state.lua')
