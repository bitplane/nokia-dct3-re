-- Own NSE-2 write/encoding entry observations; never initializes guest buffers.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse2_toolkit_fixture.lua')
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces.program
local retained, counts = {}, {}
_G.nse2_nv_source_taps = retained
local function bytes(address, length)
    assert(address >= 0x100000 and address + length <= 0x180000 and length <= 64,
        'NSE-2 NV source outside bounded RAM')
    local result = {}
    for i = 0, length - 1 do
        result[#result + 1] = string.format('%02x', memory:read_u8(address + i))
    end
    return table.concat(result)
end
local function observe(address, label, callback)
    counts[label] = 0
    retained[#retained + 1] = memory:install_read_tap(address & ~3,
        (address & ~3) + 3, label, function()
            if cpu.state.R15.value ~= address then return end
            counts[label] = counts[label] + 1
            assert(counts[label] <= 4096, 'NSE-2 NV observation overflow')
            callback()
        end)
end
observe(0x293c8e, 'nse2_nv_source', function()
    local address, pointer, length = cpu.state.R0.value, cpu.state.R1.value, cpu.state.R2.value
    machine:logerror(string.format(
        'nse2_nv_source: address=%04x pointer=%08x length=%d caller=%08x data=%s t=%.9f observer_writes=0\n',
        address, pointer, length, cpu.state.R14.value, bytes(pointer, length), machine.time:as_double()))
end)
observe(0x28a1d0, 'nse2_nv_encode', function()
    local source, output = cpu.state.R0.value, cpu.state.R1.value
    machine:logerror(string.format(
        'nse2_nv_encode: source=%08x first=%02x output=%08x caller=%08x t=%.9f observer_writes=0\n',
        source, memory:read_u8(source), output, cpu.state.R14.value, machine.time:as_double()))
end)
