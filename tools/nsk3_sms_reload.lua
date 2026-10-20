-- Cold Outbox read through observed Messages/Down/Select physical contacts.
_G.nsk3_sim_tx_limit=4096
_G.nsk3_followup_contacts={{1,2,0x19},{1,2,0x19},{1,8,0x18},{1,2,0x19},{1,2,0x19}}
if _G.nsk3_sms_empty then table.remove(_G.nsk3_followup_contacts) end
if _G.nsk3_sms_delete then
    local steps=_G.nsk3_sms_delete_steps or 1
    for i=1,steps do
        _G.nsk3_followup_contacts[#_G.nsk3_followup_contacts+1]=
            (_G.nsk3_sms_cancel and i==steps) and {1,16,0x1a} or {1,2,0x19}
    end
end
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_security_input.lua')
local machine=manager.machine
local next_frame=1
local frames=_G.nsk3_sms_frames or {19}
emu.register_frame_done(function()
    if frames[next_frame] and machine.time:as_double()>=frames[next_frame] then
        machine.screens[':screen']:snapshot('nsk3_sms_read_'..frames[next_frame]..'.png')
        next_frame=next_frame+1
    end
end)
