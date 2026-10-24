-- Copy one existing Phone contact to SIM; all storage changes are firmware-owned.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_staged_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(name,label)
    local key=assert(fields[name])
    machine:logerror('6210_phone_memory_copy: action='..label..'\n')
    key:set_value(1)
    assert(emu.wait(0.15))
    key:set_value(0)
    assert(emu.wait(0.85))
    machine.screens[':screen']:snapshot('6210_phone_memory_copy_'..label..'.png')
end
local input=coroutine.create(function()
    assert(emu.wait(20))
    press('Right Softkey / C','names')
    for i=1,4 do press('Scroll Down','option_'..i) end
    press('Left Softkey / Menu','phone_to_sim')
    press('Left Softkey / Menu','one_by_one')
    press('Left Softkey / Menu','contact')
    press('Left Softkey / Menu','keep_original')
    press('Left Softkey / Menu','copied')
    machine:logerror('6210_phone_memory_copy: complete=1 firmware_state_forcing=0\n')
end)
_G.noki6210_phone_memory_copy=input
assert(coroutine.resume(input))
