-- Ordinary PIN1 change via the observed 4-6-5 physical menu path.
_G.nsb7_correct_pin=true
_G.nsb7_pin_continue=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_pin_input.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local input=coroutine.create(function()
    local steps={}
    for index,digit in ipairs({'1','2','3','4','5'}) do steps[#steps+1]={15+index,'Keypad '..digit,'phone'} end
    steps[#steps+1]={21,'Left Softkey / Menu','phone'}
    for _,step in ipairs({{26,'Left Softkey / Menu','menu'},{28,'Keypad 4','menu'},
            {30,'Keypad 6','menu'},{32,'Keypad 5','menu'},{35,'Left Softkey / Menu','menu'}}) do
        steps[#steps+1]=step
    end
    for _,phase in ipairs({{'old',_G.nsb7_change_old or '1234',38},
            {'new','4321',45},{'confirm','4321',52}}) do
        for index=1,4 do steps[#steps+1]={phase[3]+index-1,'Keypad '..phase[2]:sub(index,index),phase[1]} end
        steps[#steps+1]={phase[3]+4,'Left Softkey / Menu',phase[1]}
    end
    for _,step in ipairs(steps) do
        assert(emu.wait(step[1]-machine.time:as_double()))
        local key=assert(fields[step[2]])
        machine:logerror(string.format('nsb7_change_input: phase=%s key=%s state=1 t=%.9f\n',step[3],step[2],machine.time:as_double()))
        key:set_value(1)
        assert(emu.wait(0.2))
        key:set_value(0)
        machine:logerror(string.format('nsb7_change_input: phase=%s key=%s state=0 t=%.9f\n',step[3],step[2],machine.time:as_double()))
        assert(emu.wait(0.8))
        machine.screens[':screen']:snapshot('nsb7_change_'..step[1]..'.png')
    end
    assert(emu.wait(61-machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsb7_change_final.png')
    machine:logerror('nsb7_change_input: complete\n')
    machine:exit()
end)
_G.nsb7_change_input=input
assert(coroutine.resume(input))
