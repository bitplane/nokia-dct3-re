_G.nhm3_wrong_puk=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_puk_recovery.lua')
