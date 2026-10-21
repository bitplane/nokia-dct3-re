-- Physical wrong/correct SIM PIN; distinct from the phone Security code.
_G.nsb7_observe_only=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_sim_observe.lua')
local machine=manager.machine
local correct=_G.nsb7_correct_pin==true
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local input=coroutine.create(function()
    assert(emu.wait(7.5))
    machine.screens[':screen']:snapshot('nsb7_pin_prompt.png')
    local digits={}
    for digit in (_G.nsb7_pin_digits or (correct and '1234' or '4321')):gmatch('.') do
        digits[#digits+1]=digit
    end
    digits[#digits+1]='Menu'
    for index,digit in ipairs(digits) do
        assert(emu.wait(7+index-machine.time:as_double()))
        local name=digit=='Menu' and 'Left Softkey / Menu' or 'Keypad '..digit
        local key=assert(fields[name])
        machine:logerror(string.format('nsb7_pin_input: correct=%u key=%s state=1 t=%.9f\n',correct and 1 or 0,digit,machine.time:as_double()))
        key:set_value(1)
        assert(emu.wait(0.2))
        key:set_value(0)
        machine:logerror(string.format('nsb7_pin_input: correct=%u key=%s state=0 t=%.9f\n',correct and 1 or 0,digit,machine.time:as_double()))
    end
    assert(emu.wait(14-machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsb7_pin_result.png')
    assert(emu.wait((_G.nsb7_pin_finish or 18)-machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsb7_pin_final.png')
    machine:logerror('nsb7_pin_input: complete\n')
    if not _G.nsb7_pin_continue then machine:exit() end
end)
_G.nsb7_pin_input=input
assert(coroutine.resume(input))
