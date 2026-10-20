-- Physical SIM PIN, phone code, and observed menu4-5-6 Change PIN workflow.
local source=debug.getinfo(1,'S').source:sub(2)
local directory=source:match('^(.*[/\\])')
dofile(directory .. 'nsk3_pin_reload.lua')
_G.nsk3_phone_code_start=15
_G.nsk3_followup_contacts={
    {1,2,0x19},{2,4,4},{3,4,5},{4,4,6},{1,2,0x19},{1,8,0x18},{1,2,0x19},
    {2,2,1},{3,2,2},{4,2,3},{2,4,4},{1,2,0x19},
    {2,4,4},{4,2,3},{3,2,2},{2,2,1},{1,2,0x19},
    {2,4,4},{4,2,3},{3,2,2},{2,2,1},{1,2,0x19}
}
dofile(directory .. 'nsk3_security_input.lua')
local machine=manager.machine
local next_frame=1
local frames={25,29,35,37}
emu.register_frame_done(function()
    if frames[next_frame] and machine.time:as_double()>=frames[next_frame] then
        machine.screens[':screen']:snapshot('nsk3_change_pin_'..frames[next_frame]..'.png')
        next_frame=next_frame+1
    end
end)
