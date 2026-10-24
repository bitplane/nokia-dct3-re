_G.nsm2_fdn_toggle=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki8850_fdn.lua')
