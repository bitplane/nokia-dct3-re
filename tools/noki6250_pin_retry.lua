-- Physical wrong-PIN rejection followed by ordinary correct-PIN recovery.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_runtime_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(key,when)
    assert(emu.wait(when-machine.time:as_double()))
    local field=assert(fields[key])
    machine:logerror(string.format('6250_pin_retry: key=%s state=1 t=%.9f\n',key,machine.time:as_double()))
    field:set_value(1)
    assert(emu.wait(0.15))
    field:set_value(0)
    machine:logerror(string.format('6250_pin_retry: key=%s state=0 t=%.9f\n',key,machine.time:as_double()))
    assert(emu.wait(0.85))
end
local runner=coroutine.create(function()
    for second=8,11 do press('Keypad 9',second) end
    press('Left Softkey / Menu',12)
    machine.screens[':screen']:snapshot('6250_pin_retry_error.png')
    for index=1,4 do press('Keypad '..index,17+index) end
    press('Left Softkey / Menu',22)
    assert(emu.wait(32-machine.time:as_double()))
    machine.screens[':screen']:snapshot('6250_pin_retry_idle.png')
    press('Left Softkey / Menu',34)
    machine.screens[':screen']:snapshot('6250_pin_retry_menu.png')
    machine:logerror('6250_pin_retry: complete=1 firmware_state_forcing=0\n')
end)
_G.noki6250_pin_retry=runner
assert(coroutine.resume(runner))
