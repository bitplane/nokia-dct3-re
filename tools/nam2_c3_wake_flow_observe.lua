-- Physical wake and its immediate firmware continuation; no guest writes.
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local count=0
local taps={}
for _,pc in ipairs({0x2f85fa,0x2f860e,0x2f866a,0x2f8688}) do
    local start=pc & ~3
    taps[#taps+1]=memory:install_read_tap(start,start+3,'nam2_wake_return_'..pc,function()
        local now=machine.time:as_double()
        if now<8.95 or now>=8.96 or count>=64 or cpu.state['R15'].value~=pc then return end
        count=count+1
        local sp=cpu.state['R13'].value
        machine:logerror(string.format('nam2_wake_flow: index=%d pc=%08x lr=%08x sp=%08x cpsr=%08x r0=%08x ctrl=%02x t=%.9f observer_writes=0\n',
            count,pc,cpu.state['R14'].value,sp,cpu.state['CPSR'].value,cpu.state['R0'].value,
            memory:read_u8(0x2000c),now))
    end)
end
_G.nam2_wake_return_taps=taps
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nam2_c3_clock_key_observe.lua')
machine.debugger.visible_cpu=cpu
machine.debugger:command('temp8=0')
for _,site in ipairs({
    {0x305b82,'flash_family: family=%08x id0=%04x maker=%04x device=%04x third=%04x','r0,w@r13,w@(r13+2),w@(r13+4),w@(r13+6)'},
    {0x291b3e,'checksum: computed=%08x stored=%08x','r0,r1'},
    {0x2f84aa,'release: result=%08x','r0'}
}) do
    machine.debugger:command(string.format('bpset %x,temp8<8,{temp8=temp8+1;logerror "nam2_v521_%s observer_writes=0\\n",%s;g}',site[1],site[2],site[3]))
end
