-- Cold Search with an empty name; no record action or number-view hold.
_G.nsk1_fdn_keys={'menu','down','down','down','menu','down','down','menu','down','menu',
    'down','menu','menu','menu'}
_G.nsk1_fdn_final_time=65
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_fdn_input.lua')
