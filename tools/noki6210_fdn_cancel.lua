-- Cancel the physical erase confirmation after PIN2 authentication.
_G.npe3_fdn_cancel=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_fdn_erase.lua')
