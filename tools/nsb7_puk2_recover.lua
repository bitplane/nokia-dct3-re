_G.nsb7_pin2_change=true
_G.nsb7_pin2_unblock=true
_G.nsb7_change_old=_G.nsb7_puk2_wrong and '87654322' or '87654321'
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_pin_change.lua')
