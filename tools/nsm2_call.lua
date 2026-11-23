-- Minimal driver for the native 8850 lab call: no observers, only the inputs.
-- Security code 12345 and Menu (OK) at 12 s; the laboratory BTS pages at
-- 44 s, the handset answers with Send at 48 s and ends with End at 58 s.
local machine=manager.machine
assert(machine.system.name=='nsm2ap5')
local function hold(port,name,seconds)
    local field=assert(machine.ioport.ports[port].fields[name],name)
    machine:logerror(string.format('nsm2_call_input: key=%s t=%.3f\n',name,machine.time:as_double()))
    field:set_value(1)
    local running=emu.wait(seconds)
    field:set_value(0)
    return running
end
local function until_time(t) return emu.wait(t-machine.time:as_double()) end
_G.nsm2_call=coroutine.create(function()
    if not until_time(12) then return end
    for _,item in ipairs({{2,'Keypad 1'},{3,'Keypad 2'},{4,'Keypad 3'},{2,'Keypad 4'},{3,'Keypad 5'},{1,'Menu'}}) do
        if not hold(':COL.'..item[1],item[2],0.15) or not emu.wait(0.35) then return end
    end
    if not until_time(22) then return end
    machine.screens[':screen']:snapshot('nsm2_idle.png')
    if not until_time(44) or not hold(':LAB_BTS','Laboratory BTS page',0.05) then return end
    if not until_time(47.9) then return end
    machine.screens[':screen']:snapshot('nsm2_ringing.png')
    if not until_time(48) or not hold(':COL.0','Call / Send',0.15) then return end
    if not until_time(58) or not hold(':COL.0','End',0.15) then return end
end)
assert(coroutine.resume(_G.nsm2_call))
