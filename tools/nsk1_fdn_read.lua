-- Own Numbers/Search, empty name, then held # for the stored number.
_G.nsk1_fdn_keys={'menu','down','down','down','menu','down','down','menu','down','menu',
    'down','menu','menu','menu','hash'}
_G.nsk1_fdn_hold_last=4
_G.nsk1_fdn_final_time=65
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_fdn_input.lua')
