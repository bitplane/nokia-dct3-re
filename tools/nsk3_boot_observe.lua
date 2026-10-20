-- Cold own-MCU boundary with erased storage and no DSP completion.
local machine=manager.machine
assert(machine.system.name=='noki6130' or machine.system.name=='nsk3r4t')
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
assert(memory:read_u32(0x200000)==0x20610f6a,'requires own NSK-3 image')
local writes=0
local dsp=machine.devices[':dsp_c54x:cpu']
local loader_events=0
local handoffs=0
if dsp then
    _G.nsk3_loader_handoff_tap=memory:install_write_tap(0x100fc,0x10103,'nsk3_loader_handoff',function(offset,value,mask)
        local pc=cpu.state['PC'].value
        if pc~=0x28cea8 and pc~=0x28ceb8 and pc~=0x28ceec and pc~=0x28cef6 then return end
        handoffs=handoffs+1
        if handoffs<=128 then machine:logerror(string.format('nsk3_loader_handoff: index=%d offset=%08x value=%08x mask=%08x pc=%08x t=%.9f observer_writes=0\n',handoffs,offset,value,mask,pc,machine.time:as_double())) end
    end)
    _G.nsk3_loader_state_tap=memory:install_write_tap(0x10b838,0x10b847,'nsk3_loader_state',function(offset,value,mask)
        loader_events=loader_events+1
        if loader_events<=48 then machine:logerror(string.format('nsk3_loader_state: offset=%08x value=%08x mask=%08x pc=%08x t=%.9f observer_writes=0\n',offset,value,mask,cpu.state['PC'].value,machine.time:as_double())) end
    end)
end
local released,releases=false,0
_G.nsk3_boot_release_tap=memory:install_write_tap(0x20000,0x20003,'nsk3_boot_release',function(offset,value,mask)
    if mask & 0x00ff0000 ~= 0 and ((value >> 16) & 4) ~= 0 then
        machine:logerror(string.format('nsk3_reset_request: command=%02x pc=%08x t=%.9f reason=%02x observer_writes=0\n',(value >> 16)&0xff,cpu.state['PC'].value,machine.time:as_double(),memory:read_u8(0x10ff60)))
    end
    if mask & 0x0000ff00 == 0 then return end
    local command=(value >> 8) & 0xff
    if command & 1 == 0 then released=false;return end
    if released then return end
    released=true
    releases=releases+1
    if releases>4 then return end
    local arguments,payload={},{}
    for i=0,6 do arguments[#arguments+1]=string.format('%04x',memory:read_u16(0x100f6+i*2)) end
    for i=0,185 do payload[#payload+1]=string.format('%04x',memory:read_u16(0x11e00+i*2)) end
    machine:logerror(string.format('nsk3_boot_release: index=%d command=%02x args=%s payload=%s t=%.9f observer_writes=0\n',releases,command,table.concat(arguments,','),table.concat(payload),machine.time:as_double()))
end)
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
    if dsp then machine:logerror(string.format('nsk3_native_sample: t=%.6f pc=%04x releases=%d handoffs=%d observer_writes=0\n',now,dsp.state['PC'].value,releases,handoffs)) end
    next_sample=next_sample+1
end)
