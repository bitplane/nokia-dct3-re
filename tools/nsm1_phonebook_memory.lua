-- Physical Names/Options/Memory in use; no provisioning writes.
_G.nsm1_followup_contacts={{1,16,0x1a},{1,8,0x18},{1,8,0x18},{1,8,0x18},{1,8,0x18},{1,2,0x19},{1,2,0x19}}
if _G.nsm1_select_phone then _G.nsm1_followup_contacts[#_G.nsm1_followup_contacts+1]={1,8,0x18} end
_G.nsm1_followup_contacts[#_G.nsm1_followup_contacts+1]={1,2,0x19}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_security_input.lua')
local machine=manager.machine
local times={19,20,21,22,23,25,27}
local index=1
emu.register_frame_done(function()
    if times[index] and machine.time:as_double()>=times[index] then
        machine.screens[':screen']:snapshot(string.format('nsm1_memory_%d.png',times[index]))
        index=index+1
    end
end)
