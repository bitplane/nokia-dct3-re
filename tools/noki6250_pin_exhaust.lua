_G.nhm3_pin_retry_mode='exhaust'
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_pin_retry.lua')
