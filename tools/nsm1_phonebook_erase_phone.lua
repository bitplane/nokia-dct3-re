-- Physical Names/Erase all/Phone/confirmation/security code only.
_G.nsm1_followup_contacts={{1,16,0x1a},{1,8,0x18},{1,8,0x18},{1,2,0x19},{1,8,0x18},{1,2,0x19},{1,2,0x19},{1,2,0x19}}
for _,cell in ipairs({{2,2,1},{3,2,2},{4,2,3},{2,4,4},{3,4,5},{1,2,0x19}}) do
    _G.nsm1_followup_contacts[#_G.nsm1_followup_contacts+1]=cell
end
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_security_input.lua')
local machine=manager.machine
local times={23,26,30,40,50,55}
local index=1
emu.register_frame_done(function()
    if times[index] and machine.time:as_double()>=times[index] then
        machine.screens[':screen']:snapshot(string.format('nsm1_erase_phone_%d.png',times[index]))
        index=index+1
    end
end)
