-- Reject an incorrect PUK2 without replacing the blocked PIN2.
_G.npe3_puk2='99999999'
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_puk2_input.lua')
