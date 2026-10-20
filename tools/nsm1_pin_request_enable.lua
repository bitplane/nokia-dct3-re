-- PIN request is disabled at boot; authenticate the separate phone code first.
local source=debug.getinfo(1,'S').source:sub(2)
_G.nsm1_followup_contacts={{1,2,0x19},{2,4,4},{3,4,5},{2,2,1},{1,2,0x19},
    {2,2,1},{3,2,2},{4,2,3},{2,4,4},{1,2,0x19},{1,4,0x17},{1,2,0x19}}
dofile(source:match('^(.*[/\\])') .. 'nsm1_security_input.lua')
local machine=manager.machine
local index=1
local times={19,21,23,25,27}
emu.register_frame_done(function()
    if times[index] and machine.time:as_double()>=times[index] then
        machine.screens[':screen']:snapshot('nsm1_pin_request_on_'..times[index]..'.png')
        index=index+1
    end
end)
