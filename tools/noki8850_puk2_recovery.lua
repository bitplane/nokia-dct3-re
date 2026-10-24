_G.nsm2_puk2_recovery=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki8850_pin2_change.lua')
