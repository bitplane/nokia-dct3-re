-- Dedicated MMIO conformance fixture; not firmware boot or idle acceptance.
local machine=manager.machine
local reports={noki3310=true,noki3330=true,noki3210=false}
local expected=reports[machine.system.name]
assert(expected~=nil,'wake-report fixture requires an explicit product profile')
local memory=machine.devices[':maincpu'].spaces.program
local key
for _,port in pairs(machine.ioport.ports) do
    for _,field in pairs(port.fields) do
        if field.name=='Scroll Up' then key=field end
    end
end
assert(key,'physical Up contact required')
local function sample(phase,want)
    local pending=memory:read_u8(0x20009)
    local ctrl=memory:read_u8(0x2000c)
    machine:logerror(string.format('mad2_wake_report_fixture: phase=%s machine=%s configured=%d irq=%02x ctrl=%02x t=%.9f mmio_conformance=1\n',
        phase,machine.system.name,expected and 1 or 0,pending,ctrl,machine.time:as_double()))
    assert(pending&9==want,'wake/keypad pending-source ownership differs')
    assert(ctrl&0x0f==0x0a,'wake must not enable global CPU delivery')
end
local function contact(value)
    key:set_value(value)
    memory:read_u8(0x2002a) -- Sample the physical matrix without executing firmware.
    memory:write_u8(0x2006b,0) -- Re-evaluate the column change detector.
end
local fixture=coroutine.create(function()
    assert(emu.wait(0.2))
    memory:write_u8(0x2000c,0x0a)
    memory:write_u8(0x2000a,0xff)
    memory:write_u8(0x2000b,0xfe) -- IRQ3 masked; notification must still latch.
    memory:write_u8(0x20016,0x06)
    memory:write_u8(0x20008,0xff)
    memory:write_u8(0x20009,0xff)
    memory:write_u8(0x200a8,0xff)
    memory:write_u8(0x20028,0)
    memory:write_u8(0x2006b,0)
    contact(1)
    sample('awake_edge',1)
    contact(0)
    memory:write_u8(0x20009,0xff)
    sample('armed',0)
    memory:write_u8(0x2000d,memory:read_u8(0x2000d)|2)
    assert(emu.wait(0.05))
    contact(1)
    sample('wake_edge',expected and 9 or 1)
    memory:write_u8(0x20009,8)
    sample('ack_report',1) -- Do not acknowledge the independent keypad source.
    memory:write_u8(0x2000d,memory:read_u8(0x2000d)|2)
    sample('blocked_request',1) -- Pending IRQ0 prevents another sleep/wake.
    contact(0)
    memory:write_u8(0x20009,1)
    sample('complete',0)
    machine:exit()
end)
_G.mad2_wake_report_fixture=fixture
assert(coroutine.resume(fixture))
