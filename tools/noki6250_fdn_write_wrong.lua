_G.nhm3_fdn_record_wrong=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_fdn_write.lua')
