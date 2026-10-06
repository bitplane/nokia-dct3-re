-- Bounded writes to the own self-test record; no values are changed.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki5510_bootstrap_observe.lua')
local cpu = assert(manager.machine.devices[':maincpu'])
cpu.debug:wpset(cpu.spaces['program'], 'w', 0x13fbe0, 0x20, 'temp8<80',
    'temp8=temp8+1;logerror "5510_selftest_write: pc=%08x address=%08x data=%08x\\n",pc,wpaddr,wpdata;g')
cpu.debug:bpset(0x24caf4, nil,
    'logerror "5510_selftest_return: command=%02x flags=%02x records=%08x/%08x/%08x/%08x/%08x\\n",b@(r4+8),b@13fe5d,d@13fbe0,d@13fbe4,d@13fbe8,d@13fbec,d@13fbf0;g')
local snapshot = coroutine.create(function()
    if not emu.wait(4) then return end
    local bytes = {}
    for offset = 0, 0x37ff do
        bytes[#bytes + 1] = string.char(cpu.spaces['program']:read_u8(0x100044 + offset))
    end
    local output = assert(io.open('npm5_nv_cache.bin', 'wb'))
    output:write(table.concat(bytes))
    output:close()
end)
assert(coroutine.resume(snapshot))
