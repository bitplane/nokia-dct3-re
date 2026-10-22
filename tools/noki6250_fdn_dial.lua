-- Physical dial controls against the isolated laboratory network.
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
    machine:logerror(string.format('6250_fdn_dial: key=%s state=1 t=%.9f\n',key,machine.time:as_double()))
    field:set_value(1)
    assert(emu.wait(0.15))
    field:set_value(0)
    machine:logerror(string.format('6250_fdn_dial: key=%s state=0 t=%.9f\n',key,machine.time:as_double()))
    assert(emu.wait(0.85))
end
local runner=coroutine.create(function()
    for index=1,4 do press('Keypad '..index,7+index) end
    press('Left Softkey / Menu',12)
    local digits=_G.nhm3_fdn_dial_digits or '12345'
    for index=1,#digits do press('Keypad '..digits:sub(index,index),19+index) end
    press('Send',26)
    machine.screens[':screen']:snapshot('6250_fdn_dial_early.png')
    assert(emu.wait(30-machine.time:as_double()))
    machine.screens[':screen']:snapshot('6250_fdn_dial_result.png')
    press('End',38)
    press('Left Softkey / Menu',40)
    assert(emu.wait(2))
    machine.screens[':screen']:snapshot('6250_fdn_dial_menu.png')
    machine:logerror('6250_fdn_dial: complete=1 firmware_state_forcing=0\n')
end)
_G.noki6250_fdn_dial=runner
assert(coroutine.resume(runner))
