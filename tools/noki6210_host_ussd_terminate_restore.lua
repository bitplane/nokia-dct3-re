_G.npe3_ussd_termination_restore=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_host_ussd_terminate.lua')
