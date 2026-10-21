-- Bounded read-only NSB-7 bring-up observation, not phone acceptance.
local machine=manager.machine
assert(machine.system.name=='noki8290' or machine.system.name=='nsb7hle','NSB-7 MCU observation profile required')
machine:logerror('nsb7_profile: machine='..machine.system.name..' observer_writes=0\n')
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local observer=coroutine.create(function()
    for _,target in ipairs({0.25,1,3,5,6}) do
        assert(emu.wait(target-machine.time:as_double()))
        machine:logerror(string.format(
            'nsb7_boot: pc=%08x lr=%08x cpsr=%08x reset=%02x ctrl=%02x gensio=%02x shared=%04x,%04x,%04x,%04x t=%.9f observer_writes=0\n',
            cpu.state['R15'].value,cpu.state['R14'].value,cpu.state['CPSR'].value,
            memory:read_u8(0x20002),memory:read_u8(0x2000c),memory:read_u8(0x2006d),
            memory:read_u16(0x10000),memory:read_u16(0x10004),
            memory:read_u16(0x100fe),memory:read_u16(0x10100),machine.time:as_double()))
        if target==6 then machine.screens[':screen']:snapshot('nsb7_boot_6.png') end
    end
end)
_G.nsb7_boot_observer=observer
assert(coroutine.resume(observer))
