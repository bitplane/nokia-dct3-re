-- Own command-0d consumer, read-only; no message or flag injection.
local machine=manager.machine
assert(machine.system.name=='nsb7hle','NSB-7 HLE research composition required')
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local taps={}
local counts={}
taps[#taps+1]=memory:install_write_tap(0x13fbf0,0x13fbf3,'nsb7_result_index12',function(_,value,mask)
    if mask&0x0000ff00==0 or (counts.result12 or 0)>=16 then return end
    counts.result12=(counts.result12 or 0)+1
    machine:logerror(string.format('nsb7_result: index=12 pc=%08x value=%02x mask=%08x t=%.9f observer_writes=0\n',
        cpu.state['R15'].value,(value>>8)&0xff,mask,machine.time:as_double()))
end)
taps[#taps+1]=memory:install_write_tap(0x13fbec,0x13fbef,'nsb7_fault12',function(_,value,mask)
    if mask&0xff000000==0 or (counts.fault12 or 0)>=16 then return end
    counts.fault12=(counts.fault12 or 0)+1
    machine:logerror(string.format('nsb7_fault12: pc=%08x value=%02x mask=%08x t=%.9f observer_writes=0\n',
        cpu.state['R15'].value,(value>>24)&0xff,mask,machine.time:as_double()))
end)
for _,pc in ipairs({0x248b74,0x248c28}) do
    local start=pc & ~3
    taps[#taps+1]=memory:install_read_tap(start,start+3,'nsb7_selftest_'..pc,function()
        if cpu.state['R15'].value~=pc or (counts[pc] or 0)>=16 then return end
        counts[pc]=(counts[pc] or 0)+1
        local message=cpu.state['R4'].value
        machine:logerror(string.format(
            'nsb7_selftest: pc=%08x command=%02x reply=%02x flags=%02x faults=%02x,%02x,%02x t=%.9f observer_writes=0\n',
            pc,memory:read_u8(message+8),memory:read_u8(message+9),memory:read_u8(0x13fde1),
            memory:read_u8(0x13fbef),memory:read_u8(0x13fbf0),memory:read_u8(0x13fbf1),machine.time:as_double()))
    end)
end
_G.nsb7_selftest_taps=taps
taps[#taps+1]=memory:install_read_tap(0x296590,0x296593,'nsb7_prefix_checksum',function()
    if cpu.state['R15'].value~=0x296590 or (counts.prefix or 0)>=8 then return end
    counts.prefix=(counts.prefix or 0)+1
    machine:logerror(string.format('nsb7_prefix_compare: computed=%08x stored=%08x t=%.9f observer_writes=0\n',
        cpu.state['R0'].value,memory:read_u32(cpu.state['R4'].value+0x11c),machine.time:as_double()))
end)
taps[#taps+1]=memory:install_read_tap(0x24892c,0x24892f,'nsb7_calibration_compare',function()
    if cpu.state['R15'].value~=0x24892c or (counts.compare or 0)>=8 then return end
    counts.compare=(counts.compare or 0)+1
    local sp=cpu.state['R13'].value
    machine:logerror(string.format('nsb7_calibration_compare: computed=%04x stored=%04x marker=%04x t=%.9f observer_writes=0\n',
        cpu.state['R9'].value&0xffff,memory:read_u16(sp+4),memory:read_u16(sp+6),machine.time:as_double()))
end)
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_boot_observe.lua')
local status=coroutine.create(function()
    assert(emu.wait(6))
    local faults={}
    for offset=0,28 do faults[#faults+1]=string.format('%02x',memory:read_u8(0x13fbe0+offset)) end
    machine:logerror(string.format('nsb7_status: flags=%02x faults=%s t=%.9f observer_writes=0\n',
        memory:read_u8(0x13fde1),table.concat(faults),machine.time:as_double()))
end)
_G.nsb7_status_observer=status
assert(coroutine.resume(status))
