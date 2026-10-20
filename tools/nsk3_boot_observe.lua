-- Cold own-MCU boundary with erased storage and no DSP completion.
local machine=manager.machine
assert(machine.system.name=='noki6130')
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
assert(memory:read_u32(0x200000)==0x20610f6a,'requires own NSK-3 image')
local writes=0
_G.nsk3_boot_shared_tap=memory:install_write_tap(0x10000,0x1000f,'nsk3_boot_shared',function(offset,value,mask)
    writes=writes+1
    if writes<=32 then machine:logerror(string.format('nsk3_boot_shared: offset=%08x value=%08x mask=%08x pc=%08x t=%.9f observer_writes=0\n',offset,value,mask,cpu.state['PC'].value,machine.time:as_double())) end
end)
local next_sample=1
emu.register_frame_done(function()
    local now=machine.time:as_double()
    if now<next_sample then return end
    machine:logerror(string.format('nsk3_boot_sample: t=%.6f pc=%08x sp=%08x ctrl=%02x clock=%02x fiq=%02x irq=%02x shared=%04x,%04x,%04x,%04x writes=%d observer_writes=0\n',
        now,cpu.state['PC'].value,cpu.state['R13'].value,memory:read_u8(0x2000c),memory:read_u8(0x2000d),memory:read_u8(0x20008),memory:read_u8(0x20009),
        memory:read_u16(0x10000),memory:read_u16(0x10002),memory:read_u16(0x10004),memory:read_u16(0x10006),writes))
    if next_sample==5 or next_sample==15 then machine.screens[':screen']:snapshot('nsk3_boot_'..next_sample..'.png') end
    next_sample=next_sample+1
end)
