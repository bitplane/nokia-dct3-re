_G.npe3_phone_memory_empty=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_phone_memory.lua')
