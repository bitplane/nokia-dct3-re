-- Physical menu4-5-1 authentication followed by Off.
local source=debug.getinfo(1,'S').source:sub(2)
_G.nsm1_followup_contacts={{1,2,0x19},{2,4,4},{3,4,5},{2,2,1},{1,2,0x19},
    {2,2,1},{3,2,2},{4,2,3},{2,4,4},{1,2,0x19},{1,8,0x18},{1,2,0x19}}
dofile(source:match('^(.*[/\\])') .. 'nsm1_pin_menu.lua')
local machine=manager.machine
local index=1
local times={25,27,29,31,33}
emu.register_frame_done(function()
    if times[index] and machine.time:as_double()>=times[index] then
        machine.screens[':screen']:snapshot('nsm1_pin_request_off_'..times[index]..'.png')
        index=index+1
    end
end)
