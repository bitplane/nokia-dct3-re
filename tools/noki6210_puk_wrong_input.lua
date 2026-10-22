-- Consume one PUK retry through the real blocked-card editor.
_G.npe3_puk_wrong_only=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_puk_input.lua')
