-- MMIO conformance only: these controller writes are not a phone-boot fixture.
local machine=manager.machine
local native=machine.system.name=='npe3r6'
assert(native or machine.system.name=='noki6210')
local space=assert(machine.devices[':maincpu']).spaces.program
local address=0x20002
local function expect(write,read)
    space:write_u8(address,write)
    assert(space:read_u8(address)==read,string.format('MAD2 control write=%02x expected=%02x',write,read))
end
expect(0,0)
expect(0x44,native and 0x14 or 0x10)
expect(0x45,native and 0x15 or 0x10)
local read=space:read_u8(address)
expect(read|2,native and 0x15 or 2)
-- A copied ready bit cannot keep hardware status asserted after release drops.
expect(0x10,0)
expect(0,0)
machine:logerror(string.format('mad2_dsp_readback_conformance: cases=6 readback_mask=%02x firmware_boot_claim=0 PASS\n',native and 5 or 0))
machine:exit()
