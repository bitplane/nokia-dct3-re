-- Observe the own Messages menu through physical contacts after phone unlock.
_G.nsk3_sim_tx_limit=4096
_G.nsk3_followup_contacts={{1,2,0x19},{1,2,0x19},{1,8,0x18},{1,8,0x18},
    {1,2,0x19},{3,2,2},{1,2,0x19},{1,8,0x18},{1,8,0x18},{1,2,0x19}}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_security_input.lua')
local machine=manager.machine
local next_frame=1
local frames={19,23,25}
emu.register_frame_done(function()
    if frames[next_frame] and machine.time:as_double()>=frames[next_frame] then
        machine.screens[':screen']:snapshot('nsk3_sms_'..frames[next_frame]..'.png')
        next_frame=next_frame+1
    end
end)
