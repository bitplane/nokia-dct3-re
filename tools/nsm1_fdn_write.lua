-- Physical Numbers/Add entry exploration on the explicit FDN card.
local source=debug.getinfo(1,'S').source:sub(2)
_G.nsm1_followup_contacts={
    {1,2,0x19},{2,4,4},{3,4,5},{4,2,3},{1,2,0x19},
    {1,8,0x18},{1,2,0x19},{1,8,0x18},{1,2,0x19},
    {3,4,5},{4,4,6},{2,8,7},{3,8,8},{1,2,0x19},
    {3,2,2},{1,2,0x19},{2,2,1},{3,2,2},{4,2,3},{2,4,4},{3,4,5},{1,2,0x19}
}
if _G.nsm1_fdn_wrong_pin2 then
    _G.nsm1_followup_contacts[13]={3,4,5}
    while #_G.nsm1_followup_contacts>14 do table.remove(_G.nsm1_followup_contacts) end
end
dofile(source:match('^(.*[/\\])') .. 'nsm1_pin_menu.lua')
local machine=manager.machine
local index=1
local times={25,27,29,31,33,35,37,39,41}
emu.register_frame_done(function()
    if times[index] and machine.time:as_double()>=times[index] then
        machine.screens[':screen']:snapshot('nsm1_fdn_write_'..times[index]..'.png')
        index=index+1
    end
end)
