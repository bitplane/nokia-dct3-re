-- Wrong PIN2 must deny FDN Add before any record editor or UPDATE.
_G.npe3_fdn_write_wrong=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_fdn_write.lua')
