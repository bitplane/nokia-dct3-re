-- Physical FDN Search/Options/Erase, PIN2, then confirm or cancel.
_G.nsk1_fdn_keys={'menu','down','down','down','menu','down','down','menu','down','menu',
    'down','menu','menu','menu','menu','down','menu',
    'five','six','seven','eight','menu',_G.nsk1_fdn_cancel and 'clear' or 'menu'}
_G.nsk1_fdn_times={}
for index=1,17 do _G.nsk1_fdn_times[index]=32+index*2 end
for index=18,22 do _G.nsk1_fdn_times[index]=68+(index-18)*0.5 end
_G.nsk1_fdn_times[23]=72
_G.nsk1_fdn_frame_delay=1.5
_G.nsk1_fdn_final_time=76
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_fdn_input.lua')
