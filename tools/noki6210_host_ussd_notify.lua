-- Authenticate, render a host notification, then physically dismiss with Back.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_staged_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(name,when)
    assert(emu.wait(when-machine.time:as_double()))
    local field=assert(fields[name])
    machine:logerror(string.format('6210_host_ussd_input: key=%s state=1 t=%.9f\n',name,machine.time:as_double()))
    field:set_value(1)
    assert(emu.wait(0.15))
    field:set_value(0)
    machine:logerror(string.format('6210_host_ussd_input: key=%s state=0 t=%.9f\n',name,machine.time:as_double()))
    assert(emu.wait(0.85))
    machine.screens[':screen']:snapshot('6210_host_ussd_key_'..when..'.png')
end
local runner=coroutine.create(function()
    for i=1,4 do press('Keypad '..i,7+i) end
    press('Left Softkey / Menu',12)
    assert(emu.wait(20-machine.time:as_double()))
    print('6210_host_ussd: ready')
    io.stdout:flush()
    assert(emu.wait(24-machine.time:as_double()))
    machine.screens[':screen']:snapshot(_G.npe3_host_ussd_capture or '6210_host_ussd_notice.png')
    for _,action in ipairs(_G.npe3_host_ussd_actions or {{26,'Right Softkey / C'},{30,'Left Softkey / Menu'}}) do
        press(action[2],action[1])
    end
    if _G.npe3_host_ussd_after_actions then
        _G.npe3_host_ussd_after_actions()
        return
    end
    assert(emu.wait((_G.npe3_host_ussd_finish or 35)-machine.time:as_double()))
    machine:logerror('6210_host_ussd: complete=1 firmware_state_forcing=0\n')
end)
_G.npe3_host_ussd_runner=runner
assert(coroutine.resume(runner))
