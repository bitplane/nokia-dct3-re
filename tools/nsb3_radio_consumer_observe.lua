-- Follow the captured 13b8 object; address is a run-specific diagnostic only.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_radio_observe.lua')
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces.program
cpu.debug:bpset(0x219d82, '1',
    'logerror "nsb3_radio_result_branch: object=%08x state=%02x phase=%02x cache=%08x observer_writes=0\\n",r5,b@106fcc,b@106fd1,d@106b80;g')
cpu.debug:bpset(0x219dee, '1',
    'logerror "nsb3_radio_result_tail: state=%02x phase=%02x cache=%08x observer_writes=0\\n",b@106fcc,b@106fd1,d@106b80;g')
local writes = 0
_G.nsb3_radio_state_watch = memory:install_write_tap(0x106fcc, 0x106fcf, 'nsb3_radio_state_watch', function(_, data, mask)
    if mask & 0xff000000 == 0 then return end
    writes = writes + 1
    if writes <= 32 then
        machine:logerror(string.format('nsb3_radio_controller_state: value=%02x pc=%08x t=%.9f observer_writes=0\n',
            (data >> 24) & 255, cpu.state['PC'].value, machine.time:as_double()))
    end
end)
local capture = coroutine.create(function()
    assert(emu.wait(45))
    local cache = memory:read_u32(0x106b80)
    assert(cache >= 0x100000 and cache + 166 <= 0x110000, 'cache outside own RAM')
    local file = assert(io.open('nsb3_radio_cached_vector.bin', 'wb'))
    for offset = 0, 159 do file:write(string.char(memory:read_u8(cache + 6 + offset))) end
    file:close()
    machine:logerror(string.format('nsb3_radio_cached_vector: pointer=%08x bytes=160 state=%02x phase=%02x t=45 observer_writes=0\n',
        cache, memory:read_u8(0x106fcc), memory:read_u8(0x106fd1)))
end)
assert(coroutine.resume(capture))
local seen = {}
local reads = 0
_G.nsb3_radio_object = memory:install_read_tap(0x102c40, 0x102c43, 'nsb3_radio_object', function(_, data, mask)
    if machine.time:as_double() < 40 or reads >= 40 or mask & 0xffff0000 == 0 or (data >> 16) & 0xffff ~= 0x13b8 then return end
    local pc = cpu.state['PC'].value
    if seen[pc] then return end
    seen[pc] = true
    reads = reads + 1
    machine:logerror(string.format('nsb3_radio_object_read: object=00102c40 data=%08x mask=%08x pc=%08x lr=%08x r0=%08x r1=%08x r4=%08x t=%.9f observer_writes=0\n',
        data, mask, pc, cpu.state['R14'].value, cpu.state['R0'].value, cpu.state['R1'].value,
        cpu.state['R4'].value, machine.time:as_double()))
end)
