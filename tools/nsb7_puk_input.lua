-- PUK and replacement PIN are entered only through physical contacts.
_G.nsb7_observe_only=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_sim_observe.lua')
local machine=manager.machine
local wrong=_G.nsb7_wrong_puk==true
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local input=coroutine.create(function()
    assert(emu.wait(7.5))
    machine.screens[':screen']:snapshot('nsb7_puk_prompt.png')
    for _,phase in ipairs({{'puk',wrong and '12345679' or '12345678',8},
            {'new','4321',19},{'confirm','4321',26}}) do
        local keys={}
        for digit in phase[2]:gmatch('.') do keys[#keys+1]=digit end
        keys[#keys+1]='Menu'
        for index,digit in ipairs(keys) do
            assert(emu.wait(phase[3]+index-1-machine.time:as_double()))
            local name=digit=='Menu' and 'Left Softkey / Menu' or 'Keypad '..digit
            local key=assert(fields[name])
            machine:logerror(string.format('nsb7_puk_input: wrong=%u phase=%s key=%s state=1 t=%.9f\n',wrong and 1 or 0,phase[1],digit,machine.time:as_double()))
            key:set_value(1)
            assert(emu.wait(0.2))
            key:set_value(0)
            machine:logerror(string.format('nsb7_puk_input: wrong=%u phase=%s key=%s state=0 t=%.9f\n',wrong and 1 or 0,phase[1],digit,machine.time:as_double()))
        end
        assert(emu.wait(0.8))
        machine.screens[':screen']:snapshot('nsb7_puk_'..phase[1]..'.png')
    end
    assert(emu.wait(35-machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsb7_puk_final.png')
    machine:logerror('nsb7_puk_input: complete\n')
    machine:exit()
end)
_G.nsb7_puk_input=input
assert(coroutine.resume(input))
