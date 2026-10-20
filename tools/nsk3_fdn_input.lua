-- Own NSK-3 physical PIN/phone input; FDN remains a standard card contract.
local source=debug.getinfo(1,'S').source:sub(2)
local directory=source:match('^(.*[/\\])')
_G.nsk3_sim_tx_limit=4096
dofile(directory .. 'nsk3_pin_reload.lua')
_G.nsk3_phone_code_start=15
_G.nsk3_followup_contacts={{1,2,0x19},{2,4,4},{3,4,5},{4,2,3},{1,2,0x19},
    {1,4,0x17},{1,2,0x19},{3,4,5},{4,4,6},{2,8,7},{3,8,8},{1,2,0x19}}
if _G.nsk3_fdn_disable then
    _G.nsk3_followup_contacts[6]={1,8,0x18}
elseif _G.nsk3_fdn_observe then
    _G.nsk3_followup_contacts={{1,2,0x19},{2,4,4},{3,4,5},{4,2,3}}
end
dofile(directory .. 'nsk3_security_input.lua')
local machine=manager.machine
local index=1
local times={24,26,28,30,32}
emu.register_frame_done(function()
    if times[index] and machine.time:as_double()>=times[index] then
        machine.screens[':screen']:snapshot('nsk3_fdn_'..times[index]..'.png')
        index=index+1
    end
end)
