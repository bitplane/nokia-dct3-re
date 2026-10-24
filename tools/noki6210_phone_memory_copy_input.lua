-- Copy/move one contact between banks; firmware owns both mutations.
return function(erase_original,reverse)
local prefix=reverse and '6210_phone_memory_reverse' or '6210_phone_memory_copy'
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_staged_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(name,label)
    local key=assert(fields[name])
    machine:logerror(prefix..': action='..label..'\n')
    key:set_value(1)
    assert(emu.wait(0.15))
    key:set_value(0)
    assert(emu.wait(0.85))
    machine.screens[':screen']:snapshot(prefix..'_'..label..'.png')
end
local input=coroutine.create(function()
    assert(emu.wait(20))
    press('Right Softkey / C','names')
    for i=1,4 do press('Scroll Down','option_'..i) end
    press('Left Softkey / Menu','phone_to_sim')
    if reverse then press('Scroll Down','sim_to_phone') end
    press('Left Softkey / Menu','one_by_one')
    press('Left Softkey / Menu','contact')
    press('Left Softkey / Menu','keep_original')
    if erase_original then press('Scroll Down','erase_original') end
    press('Left Softkey / Menu','copied')
    machine:logerror(prefix..': complete=1 firmware_state_forcing=0\n')
end)
_G.noki6210_phone_memory_copy=input
assert(coroutine.resume(input))
end
