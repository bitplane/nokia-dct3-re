-- Cold Outbox read following physical PIN and phone-code authentication.
_G.nsm1_followup_contacts={{1,2,0x19},{1,2,0x19},{1,8,0x18},{1,2,0x19},{1,2,0x19}}
if _G.nsm1_sms_empty then table.remove(_G.nsm1_followup_contacts) end
if _G.nsm1_sms_delete then
    for index=1,3 do
        _G.nsm1_followup_contacts[#_G.nsm1_followup_contacts+1]=
            (_G.nsm1_sms_cancel and index==3) and {1,16,0x1a} or {1,2,0x19}
    end
end
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_pin_menu.lua')
local machine=manager.machine
local times={23,24,25,26,27,28,30}
local index=1
emu.register_frame_done(function()
    if times[index] and machine.time:as_double()>=times[index] then
        machine.screens[':screen']:snapshot(string.format('nsm1_sms_read_%d.png',times[index]))
        index=index+1
    end
end)
