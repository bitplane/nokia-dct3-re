-- Own observed menu4-5-1: authenticate PIN, choose On and persist ENABLE CHV.
_G.nsk3_followup_contacts={{1,2,0x19},{2,4,4},{3,4,5},{2,2,1},{1,2,0x19},
    {2,2,1},{3,2,2},{4,2,3},{2,4,4},{1,2,0x19},{1,4,0x17},{1,2,0x19}}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_security_input.lua')
local machine=manager.machine
local next_frame=1
local frames={16,18,20,24}
emu.register_frame_done(function()
    if frames[next_frame] and machine.time:as_double()>=frames[next_frame] then
        machine.screens[':screen']:snapshot('nsk3_settings_'..frames[next_frame]..'.png')
        next_frame=next_frame+1
    end
end)
