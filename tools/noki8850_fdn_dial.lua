-- Isolated laboratory call controls through physical keys only.
_G.noki8850_security_only=true
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki8850_radio_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(name,when)
    assert(emu.wait(when-machine.time:as_double()))
    local field=assert(fields[name])
    for _,state in ipairs({1,0}) do
        machine:logerror(string.format('8850_fdn_dial: key=%s state=%d t=%.9f\n',
            name,state,machine.time:as_double()))
        field:set_value(state)
        assert(emu.wait(state==1 and 0.15 or 0.85))
    end
end
local input=coroutine.create(function()
    press('End',24)
    local digits=_G.nsm2_fdn_unlisted and '12346' or '12345'
    for i=1,#digits do press('Keypad '..digits:sub(i,i),25+i) end
    press('Call / Send',32)
    machine.screens[':screen']:snapshot('8850_fdn_dial_early.png')
    assert(emu.wait(38-machine.time:as_double()))
    machine.screens[':screen']:snapshot('8850_fdn_dial_result.png')
    press('End',44)
    press('Menu',46)
    machine.screens[':screen']:snapshot('8850_fdn_dial_menu.png')
    machine:logerror('8850_fdn_dial: complete=1 firmware_state_forcing=0\n')
end)
_G.noki8850_fdn_dial=input
assert(coroutine.resume(input))
