-- Read-only own-ROM journal caller observation after physical time/date save.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_clock_read.lua')
local cpu = assert(manager.machine.devices[':maincpu'])
assert(cpu.debug, '8890 NV observation requires debugger')
cpu.debug:bpset(0x3064ac, 'r0>=3d9654 && r0<=3d965a',
    'logerror "8890_clock_nv_read: address=%08x value=%04x caller=%08x r4=%08x r5=%08x r6=%08x r7=%08x sp=%08x\\n",r0,w@r0,r14,r4,r5,r6,r7,r13;g')
cpu.debug:bpset(0x3062cc, 'r1==11ee94',
    'logerror "8890_clock_nv_copy: source=%08x destination=%08x words=%08x caller=%08x value=%08x\\n",r1,r3,r2,r14,d@r1;g')
cpu.debug:bpset(0x2c641c, 'r0==47c',
    'logerror "8890_clock_nv_get: offset=%08x destination=%08x length=%08x caller=%08x\\n",r0,r1,r2,r14;g')
cpu.debug:bpset(0x2c6456, 'r5==488',
    'logerror "8890_clock_nv_result: result=%08x flags=%02x caller=%08x\\n",r0,b@(r4+13),d@(r13+8);g')
local memory = cpu.spaces['program']
local count = 0
local function observe(kind, address, data, mask)
    if count >= 80 then return end
    count = count + 1
    manager.machine:logerror(string.format(
        '8890_clock_nv_cache: kind=%s pc=%08x address=%08x data=%08x mask=%08x t=%.9f\n',
        kind, cpu.state['PC'].value, address, data, mask,
        manager.machine.time:as_double()))
end
_G.noki8890_clock_nv_cache_read = memory:install_read_tap(0x11ee94, 0x11ee97,
    '8890_clock_nv_cache_read', function(a, d, m) observe('read', a, d, m) end)
_G.noki8890_clock_nv_cache_write = memory:install_write_tap(0x11ee94, 0x11ee97,
    '8890_clock_nv_cache_write', function(a, d, m) observe('write', a, d, m) end)
