-- Reject one physical PIN, then authenticate normally; no guest-state edits.
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
    machine:logerror(string.format('6210_pin_retry_input: key=%s state=1 t=%.9f\n',name,machine.time:as_double()))
    field:set_value(1)
    assert(emu.wait(0.15))
    field:set_value(0)
    machine:logerror(string.format('6210_pin_retry_input: key=%s state=0 t=%.9f\n',name,machine.time:as_double()))
end
local runner=coroutine.create(function()
    assert(emu.wait(7))
    machine.screens[':screen']:snapshot('6210_pin_retry_prompt.png')
    for attempt=1,(_G.npe3_pin_retry_attempts or 1) do
        local start=8+(attempt-1)*8
        for index=0,3 do press('Keypad 9',start+index) end
        press('Left Softkey / Menu',start+4)
        assert(emu.wait(2))
        if attempt==1 then machine.screens[':screen']:snapshot('6210_pin_retry_rejected.png') end
        if attempt==3 then machine.screens[':screen']:snapshot('6210_pin_blocked.png') end
    end
    if _G.npe3_pin_retry_wrong_only then
        machine:logerror('6210_pin_retry_input: wrong_complete=1 firmware_state_forcing=0\n')
        return
    end
    for index=1,4 do press('Keypad '..index,15+index) end
    press('Left Softkey / Menu',20)
    assert(emu.wait(39-machine.time:as_double()))
    machine.screens[':screen']:snapshot('6210_pin_retry_idle.png')
    press('Left Softkey / Menu',40)
    assert(emu.wait(2))
    machine.screens[':screen']:snapshot('6210_security_then_menu.png')
    machine:logerror('6210_pin_retry_input: complete=1 firmware_state_forcing=0\n')
end)
_G.npe3_security_retry_runner=runner
assert(coroutine.resume(runner))
