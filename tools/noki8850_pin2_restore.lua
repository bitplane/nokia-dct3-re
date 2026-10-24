_G.nsm2_pin2_reverse=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki8850_pin2_change.lua')
