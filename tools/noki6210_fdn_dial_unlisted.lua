-- The unlisted number differs by one digit from the physically saved record.
_G.npe3_fdn_dial_digits='12346'
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_fdn_dial.lua')
