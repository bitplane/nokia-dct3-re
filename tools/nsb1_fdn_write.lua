-- Own physical FDN Numbers/Add entry; PIN2 and file update stay firmware-owned.
_G.nsb1_fdn_contacts={
    {1,4},{2,4},{3,2},{2,4},{1,4},{1,4},{0,8},{1,4},{0,8},{1,4},
    {3,4},{4,4},{2,8},{3,8},{1,4}, -- PIN25678
    {3,2},{1,4}, -- Name A
    {2,2},{3,2},{4,2},{2,4},{3,4},{1,4} -- Number12345
}
if _G.nsb1_fdn_wrong_pin2 then
    _G.nsb1_fdn_contacts[14]={3,4}
    while #_G.nsb1_fdn_contacts>15 do table.remove(_G.nsb1_fdn_contacts) end
end
_G.nsb1_fdn_frames={39,43,45,55,57,59,69,71,73,75}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_fdn_input.lua')
