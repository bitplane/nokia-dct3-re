-- Physical USSD input/readback; no firmware or device-state writes.
_G.nsb7_security_with_pin=true
_G.nsb7_security_continue=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_lapdm_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local input=coroutine.create(function()
    assert(emu.wait(24-machine.time:as_double()))
    print('nsb7_ussd: ready')
    io.stdout:flush()
    for _,target in ipairs(_G.nsb7_ussd_snapshots or {27,29}) do
        assert(emu.wait(target-machine.time:as_double()))
        machine.screens[':screen']:snapshot('nsb7_ussd_'..target..'.png')
    end
    for index,action in ipairs(_G.nsb7_ussd_actions or {
        {30,'Left Softkey / Menu'}, {32,'End'}, {34,'Left Softkey / Menu'},
    }) do
        if not emu.wait(action[1]-machine.time:as_double()) then return end
        local field=assert(fields[action[2]])
        machine:logerror(string.format('nsb7_ussd_input: key=%s state=1 t=%.9f\n',action[2],machine.time:as_double()))
        field:set_value(1)
        if not emu.wait(0.2) then field:set_value(0); return end
        field:set_value(0)
        machine:logerror(string.format('nsb7_ussd_input: key=%s state=0 t=%.9f\n',action[2],machine.time:as_double()))
        if not emu.wait(0.8) then return end
        machine.screens[':screen']:snapshot('nsb7_ussd_key_'..index..'.png')
    end
    if not emu.wait((_G.nsb7_ussd_finish or 37)-machine.time:as_double()) then return end
    machine:logerror('nsb7_ussd: complete\n')
    machine:exit()
end)
_G.nsb7_host_ussd_input=input
assert(coroutine.resume(input))
if _G.nsb7_ussd_response_times then
    local captures=coroutine.create(function()
        for _,target in ipairs(_G.nsb7_ussd_response_times) do
            if not emu.wait(target-machine.time:as_double()) then return end
            machine.screens[':screen']:snapshot('nsb7_ussd_response_'..target..'.png')
        end
    end)
    _G.nsb7_host_ussd_captures=captures
    assert(coroutine.resume(captures))
end
