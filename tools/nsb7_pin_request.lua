-- Physical SIM-card-security Off/On; credentials and state stay firmware-owned.
local source=debug.getinfo(1,'S').source:sub(2)
local directory=assert(source:match('^(.*[/\\])'))
local enable=_G.nsb7_request_enable==true
if enable then
    _G.nsb7_security_continue=true
    dofile(directory..'nsb7_security_observe.lua')
else
    _G.nsb7_correct_pin=true
    _G.nsb7_pin_continue=true
    dofile(directory..'nsb7_pin_input.lua')
end
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local input=coroutine.create(function()
    local steps={}
    if not enable then
        for index,digit in ipairs({'1','2','3','4','5'}) do steps[#steps+1]={15+index,'Keypad '..digit} end
        steps[#steps+1]={21,'Left Softkey / Menu'}
    end
    for _,step in ipairs({{26,'Left Softkey / Menu'},{28,'Keypad 4'},
            {30,'Keypad 6'},{32,'Keypad 1'},{35,'Left Softkey / Menu'},
            {38,'Keypad 1'},{39,'Keypad 2'},{40,'Keypad 3'},{41,'Keypad 4'},
            {42,'Left Softkey / Menu'},{45,enable and 'Scroll Up' or 'Scroll Down'},
            {48,'Left Softkey / Menu'}}) do steps[#steps+1]=step end
    for _,step in ipairs(steps) do
        assert(emu.wait(step[1]-machine.time:as_double()))
        local key=assert(fields[step[2]])
        machine:logerror(string.format('nsb7_request_input: enable=%u key=%s state=1 t=%.9f\n',enable and 1 or 0,step[2],machine.time:as_double()))
        key:set_value(1)
        assert(emu.wait(0.2))
        key:set_value(0)
        machine:logerror(string.format('nsb7_request_input: enable=%u key=%s state=0 t=%.9f\n',enable and 1 or 0,step[2],machine.time:as_double()))
        assert(emu.wait(0.8))
        machine.screens[':screen']:snapshot('nsb7_request_'..step[1]..'.png')
    end
    assert(emu.wait(53-machine.time:as_double()))
    machine.screens[':screen']:snapshot('nsb7_request_final.png')
    machine:logerror('nsb7_request_input: complete\n')
    machine:exit()
end)
_G.nsb7_request_input=input
assert(coroutine.resume(input))
