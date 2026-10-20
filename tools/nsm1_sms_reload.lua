-- Cold Outbox read following physical PIN and phone-code authentication.
_G.nsm1_followup_contacts={{1,2,0x19},{1,2,0x19},{1,8,0x18},{1,2,0x19},{1,2,0x19}}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_pin_menu.lua')
local machine=manager.machine
local times={23,25,27}
local index=1
emu.register_frame_done(function()
    if times[index] and machine.time:as_double()>=times[index] then
        machine.screens[':screen']:snapshot(string.format('nsm1_sms_read_%d.png',times[index]))
        index=index+1
    end
end)
