-- Own physical Numbers/Add, protected PIN2 and A/12345 entry.
_G.nsk1_fdn_keys={'menu','down','down','down','menu','down','down','menu','down','menu',
    'down','menu','down','menu',
    'five','six','seven','eight','menu',
    'two','menu','one','two','three','four','five','menu'}
_G.nsk1_fdn_times={}
for index=1,14 do _G.nsk1_fdn_times[index]=32+index*2 end
for index=15,19 do _G.nsk1_fdn_times[index]=62+(index-15)*0.5 end
_G.nsk1_fdn_times[20]=66
_G.nsk1_fdn_times[21]=66.5
for index=22,27 do _G.nsk1_fdn_times[index]=68+(index-22)*0.5 end
if _G.nsk1_fdn_wrong_pin2 then
    _G.nsk1_fdn_keys[18]='five'
    while #_G.nsk1_fdn_keys>19 do table.remove(_G.nsk1_fdn_keys) end
    _G.nsk1_fdn_final_time=66
end
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_fdn_input.lua')
