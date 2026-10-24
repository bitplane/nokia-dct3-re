_G.nsm2_wrong_puk2=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki8850_puk2_recovery.lua')
