-- Cold Outbox read through observed Messages/Down/Select physical contacts.
_G.nsk3_sim_tx_limit=4096
_G.nsk3_followup_contacts={{1,2,0x19},{1,2,0x19},{1,8,0x18},{1,2,0x19},{1,2,0x19}}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_security_input.lua')
local machine=manager.machine
local captured=false
emu.register_frame_done(function()
    if not captured and machine.time:as_double()>=19 then
        machine.screens[':screen']:snapshot('nsk3_sms_read_19.png')
        captured=true
    end
end)
