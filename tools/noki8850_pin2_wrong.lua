_G.nsm2_pin2_wrong=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki8850_pin2_change.lua')
