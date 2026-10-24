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
local lifecycle_count = 0
_G.nsk1_nv_lifecycle = space:install_read_tap(0x249c2c,0x249c2f,'nsk1_nv_lifecycle',function()
    if cpu.state.R15.value ~= 0x249c2c then return end
    lifecycle_count = lifecycle_count+1
    assert(lifecycle_count <= 128,'record lifecycle capture overflow')
    machine:logerror(string.format('nsk1_nv_lifecycle: class=%x index=%x digit=%08x digit_first=%02x other=%08x other_first=%02x caller=%08x sp=%08x t=%.9f observer_writes=0\n',
        cpu.state.R0.value,cpu.state.R1.value,cpu.state.R2.value,space:read_u8(cpu.state.R2.value),
        cpu.state.R3.value,space:read_u8(cpu.state.R3.value),cpu.state.R14.value,cpu.state.R13.value,machine.time:as_double()))
end)
_G.nsk1_nv_clear = space:install_read_tap(0x24b420,0x24b423,'nsk1_nv_clear',function()
    if cpu.state.R15.value ~= 0x24b420 then return end
    machine:logerror(string.format('nsk1_nv_clear: caller=%08x r0=%08x r4=%08x r5=%08x r6=%08x sp=%08x t=%.9f observer_writes=0\n',
        cpu.state.R14.value,cpu.state.R0.value,cpu.state.R4.value,cpu.state.R5.value,cpu.state.R6.value,cpu.state.R13.value,machine.time:as_double()))
end)
_G.nsk1_nv_selector = space:install_read_tap(0x270e74,0x270e77,'nsk1_nv_selector',function()
    if cpu.state.R15.value ~= 0x270e74 then return end
    machine:logerror(string.format('nsk1_nv_selector: input=%04x stage=%02x flags=%02x caller=%08x t=%.9f observer_writes=0\n',
        cpu.state.R0.value,space:read_u8(0x10acd7),space:read_u8(0x10acdf),cpu.state.R14.value,machine.time:as_double()))
end)
-- Empty input sets only the encoded length; the fixed-width record copy can
-- subsequently persist unused stack padding. Observe input, never initialize it.
_G.nsk1_nv_encode = space:install_read_tap(0x285a98,0x285a9b,'nsk1_nv_encode',function()
    if cpu.state.R15.value ~= 0x285a98 or cpu.state.R14.value ~= 0x249c51 then return end
    machine:logerror(string.format('nsk1_nv_encode: source=%08x first=%02x output=%08x lr=%08x t=%.9f observer_writes=0\n',
        cpu.state.R0.value,space:read_u8(cpu.state.R0.value),cpu.state.R1.value,
        cpu.state.R14.value,machine.time:as_double()))
end)
