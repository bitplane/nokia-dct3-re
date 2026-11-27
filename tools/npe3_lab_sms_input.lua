-- Physical keys for native 6210 SMS on the laboratory cell (no observers).
-- NPE3_LAB_SMS=incoming: press the lab page at 5 s (the cell pages once
-- registration is complete), then read the message with Left Softkey twice.
-- NPE3_LAB_SMS=outgoing: compose "A" to 5551234 and send it.
local machine=manager.machine
local scenario=assert(os.getenv('NPE3_LAB_SMS'))
local function snap(name) machine.screens[':screen']:snapshot(name..'.png') end
local function press(tag,name,action,settle)
    local key=assert(machine.ioport.ports[tag].fields[name])
    machine:logerror(string.format('npe3_lab_sms_key: action=%s t=%.3f\n',action,machine.time:as_double()))
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0);return false end
    key:set_value(0)
    return emu.wait(settle or 0.85)
end
_G.npe3_lab_sms=coroutine.create(function()
    if scenario=='incoming' then
        if not emu.wait(5) or not press(':LAB_BTS','Laboratory BTS page','page',0.85) then return end
        if not emu.wait(30-machine.time:as_double()) then return end
        snap('sms_received')
        for index=1,2 do
            if not press(':COL.1','Left Softkey / Menu','read_'..index,1.85) then return end
            snap('sms_read_'..index)
        end
    elseif scenario=='outgoing' then
        if not emu.wait(25) then return end
        for _,step in ipairs({{1,'messages'},{1,'message_list'},{1,'write'},{3,'Keypad 2','text_A'},
                {1,'options'},{1,'send'}}) do
            if not press(':COL.'..step[1],step[2]=='Keypad 2' and 'Keypad 2' or 'Left Softkey / Menu',step[#step]) then return end
        end
        for _,digit in ipairs({5,5,5,1,2,3,4}) do
            if not press(':COL.'..(2+(digit-1)%3),'Keypad '..digit,'recipient_'..digit) then return end
        end
        if not press(':COL.1','Left Softkey / Menu','confirm_send') then return end
        if not emu.wait(1) then return end
        snap('sms_sending')
        -- The handset reports the network's RP-ACK.
        if not emu.wait(41-machine.time:as_double()) then return end
        snap('sms_sent')
    else
        error('NPE3_LAB_SMS must be incoming or outgoing')
    end
end)
assert(coroutine.resume(_G.npe3_lab_sms))
