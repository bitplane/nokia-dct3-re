_G.nsm2_pin_exhaust=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki8850_pin_wrong.lua')
