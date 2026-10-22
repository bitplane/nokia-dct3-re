-- Physical dial-permission controls against the isolated laboratory network.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_staged_observe.lua')
local machine=manager.machine
machine.devices[':maincpu'].debug:bpset(0x4fad30,nil,
    'logerror "6210_keypad_decoded: key=%02x\\n",r0;g')
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(name,when)
    assert(emu.wait(when-machine.time:as_double()))
    local field=assert(fields[name])
    machine:logerror(string.format('6210_fdn_dial: key=%s state=1 t=%.9f\n',name,machine.time:as_double()))
    if name=='Send' or name=='End' then
        machine:logerror('6210_call_physical: action='..(name=='Send' and 'send' or 'end')..'\n')
    end
    field:set_value(1)
    assert(emu.wait(0.15))
    field:set_value(0)
    machine:logerror(string.format('6210_fdn_dial: key=%s state=0 t=%.9f\n',name,machine.time:as_double()))
    assert(emu.wait(0.85))
end
local runner=coroutine.create(function()
    for i=1,4 do press('Keypad '..i,7+i) end
    press('Left Softkey / Menu',12)
    local digits=_G.npe3_fdn_dial_digits or '12345'
    for i=1,#digits do press('Keypad '..digits:sub(i,i),19+i) end
    press('Send',26)
    machine.screens[':screen']:snapshot('6210_fdn_dial_early.png')
    assert(emu.wait(30-machine.time:as_double()))
    machine.screens[':screen']:snapshot('6210_fdn_dial_result.png')
    press('End',38)
    press('Left Softkey / Menu',40)
    assert(emu.wait(2))
    machine.screens[':screen']:snapshot('6210_fdn_dial_menu.png')
    machine:logerror('6210_fdn_dial: complete=1 firmware_state_forcing=0\n')
end)
_G.npe3_fdn_dial_runner=runner
assert(coroutine.resume(runner))
