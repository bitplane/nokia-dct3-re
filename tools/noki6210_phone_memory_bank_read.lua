-- Explicitly select a bank before cold physical readback.
return function(bank,empty,current_bank)
assert(bank=='phone' or bank=='sim')
current_bank=current_bank or 'phone'
assert(current_bank=='phone' or current_bank=='sim')
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_staged_observe.lua')
local machine=manager.machine
local fields={}
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do fields[field.name]=field end
end
local function press(name,label)
    local key=assert(fields[name])
    machine:logerror('6210_phone_memory_'..bank..'_read: action='..label..'\n')
    key:set_value(1)
    assert(emu.wait(0.15))
    key:set_value(0)
    assert(emu.wait(0.85))
    machine.screens[':screen']:snapshot('6210_phone_memory_'..bank..'_read_'..label..'.png')
end
local input=coroutine.create(function()
    assert(emu.wait(20))
    press('Right Softkey / C','names')
    -- Phone inserts Add number before Options; SIM does not.
    for i=1,current_bank=='phone' and 6 or 5 do press('Scroll Down','option_'..i) end
    press('Left Softkey / Menu','options')
    press('Left Softkey / Menu','memory')
    -- Memory in use opens with the current selection highlighted.
    if bank~=current_bank then press('Scroll Down',bank) end
    press('Left Softkey / Menu',bank..'_selected')
    press('End','idle')
    press('Right Softkey / C','names_again')
    press('Left Softkey / Menu','search')
    press('Left Softkey / Menu','result')
    if not empty then press('Left Softkey / Menu','contact') end
    machine:logerror('6210_phone_memory_'..bank..'_read: complete=1 firmware_state_forcing=0\n')
end)
_G.noki6210_phone_memory_bank_read=input
assert(coroutine.resume(input))
end
