_G.nsm2_fdn_unlisted=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki8850_fdn_dial.lua')
