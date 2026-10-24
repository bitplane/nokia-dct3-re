-- Passive own-ROM write-buffer observation, composed with the Toolkit fixture.
local directory = debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nsk1_toolkit_input.lua')
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local space = cpu.spaces.program
local count = 0
-- 28ee2e is the write routine entry, before its prologue changes arguments.
_G.nsk1_nv_source = space:install_read_tap(0x28ee2c,0x28ee2f,'nsk1_nv_source',function()
    if cpu.state.R15.value ~= 0x28ee2e then return end
    count = count+1
    assert(count <= 4096,'NV write capture overflow')
    local address,pointer,length = cpu.state.R0.value,cpu.state.R1.value,cpu.state.R2.value
    local data = {}
    if pointer >= 0x100000 and pointer+length <= 0x120000 and length <= 64 then
        for i=0,length-1 do data[#data+1]=string.format('%02x',space:read_u8(pointer+i)) end
    end
    machine:logerror(string.format('nsk1_nv_source: address=%04x pointer=%08x length=%d caller=%08x data=%s t=%.9f observer_writes=0\n',
        address,pointer,length,cpu.state.R14.value,table.concat(data),machine.time:as_double()))
end)
