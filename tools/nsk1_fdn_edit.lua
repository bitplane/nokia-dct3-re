-- Own Search/Options/Edit, unchanged name and last-digit replacement.
_G.nsk1_fdn_keys={'menu','down','down','down','menu','down','down','menu','down','menu',
    'down','menu','menu','menu','menu','menu',
    'five','six','seven','eight','menu','menu','clear','six','menu'}
_G.nsk1_fdn_times={}
for index=1,16 do _G.nsk1_fdn_times[index]=32+index*2 end
for index=17,21 do _G.nsk1_fdn_times[index]=66+(index-17)*0.5 end
_G.nsk1_fdn_times[22]=70
_G.nsk1_fdn_times[23]=72
_G.nsk1_fdn_times[24]=72.5
_G.nsk1_fdn_times[25]=73
_G.nsk1_fdn_final_time=78
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_fdn_input.lua')
