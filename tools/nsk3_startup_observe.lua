-- Native own-ROM restart producer; breakpoints only observe registers.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_boot_observe.lua')
local machine=manager.machine
assert(machine.system.name=='nsk3r4t' and machine.debugger)
machine.debugger:command('focus :maincpu')
machine.debugger:command('temp0=0')
machine.debugger:command('bpset 2ac65e,temp0<8,{temp0=temp0+1;logerror "nsk3_restart_entry: reason=%08x caller=%08x cycles=%016x observer_writes=0\\n",r0,r14,totalcycles;g}')
machine.debugger:command('temp1=0')
machine.debugger:command('temp6=0')
machine.debugger:command('bpset 23aca4,temp6<4,{temp6=temp6+1;logerror "nsk3_security_checksum: stored=%04x computed=%04x cycles=%016x observer_writes=0\\n",r0,r1,totalcycles;g}')
machine.debugger:command('temp7=0')
machine.debugger:command('bpset 23ad7e,temp7<96 && r1>=100000 && r1<110000,{temp7=temp7+1;logerror "nsk3_startup_result: index=%02x code=%02x pointer=%08x cycles=%016x observer_writes=0\\n",r0,b@r1,r1,totalcycles;g}')
machine.debugger:command('temp8=0')
machine.debugger:command('bpset 26f4c2,temp8<8 && r4==10c018 && b@(r4+2)==2,{temp8=temp8+1;logerror "nsk3_record_accept: context=%08x copied=%08x%08x%08x%08x%08x%08x cycles=%016x observer_writes=0\\n",r4,d@(r4+1c),d@(r4+20),d@(r4+24),d@(r4+28),d@(r4+2c),d@(r4+30),totalcycles;g}')
machine.debugger:command('bpset 26f2d0,temp1<8 && r6>=100000 && r6<10ffdc,{temp1=temp1+1;logerror "nsk3_record_format: valid=%08x pointer=%08x data=%08x%08x%08x%08x%08x%08x%08x%08x%08x cycles=%016x observer_writes=0\\n",r5,r6,d@r6,d@(r6+4),d@(r6+8),d@(r6+c),d@(r6+10),d@(r6+14),d@(r6+18),d@(r6+1c),d@(r6+20),totalcycles;g}')
for index,site in ipairs({0x266a2c}) do
    local counter='temp'..(index+1)
    local words={}
    for offset=0,60,4 do words[#words+1]=string.format('d@(r1+%x)',offset) end
    machine.debugger:command(counter..'=0')
    machine.debugger:command('bpset '..string.format('%x',site)..','..counter..'<32 && r1>=100000 && r1<10ffc0 && (b@(r1+3)==70 || b@(r1+3)==74),{'..counter..'='..counter..'+1;logerror "nsk3_record_post: site='..string.format('%08x',site)..' target=%08x pointer=%08x data='..string.rep('%08x',16)..' cycles=%016x observer_writes=0\\n",r0,r1,'..table.concat(words,',')..',totalcycles;g}')
end
for index,site in ipairs({0x26ef5c,0x26f150}) do
    local counter='temp'..(index+3)
    local words={}
    for offset=0,60,4 do words[#words+1]=string.format('d@(r0+%x)',offset) end
    machine.debugger:command(counter..'=0')
    machine.debugger:command('bpset '..string.format('%x',site)..','..counter..'<4 && r0>=100000 && r0<10ffc0,{'..counter..'='..counter..'+1;logerror "nsk3_record_reply: consumer='..string.format('%08x',site)..' pointer=%08x data='..string.rep('%08x',16)..' cycles=%016x observer_writes=0\\n",r0,'..table.concat(words,',')..',totalcycles;g}')
end
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local count=0
_G.nsk3_startup_marker_tap=memory:install_write_tap(0x10fdc8,0x10fdcf,'nsk3_startup_marker',function(offset,value,mask)
    count=count+1
    if count<=32 then machine:logerror(string.format('nsk3_startup_marker: offset=%08x value=%08x mask=%08x pc=%08x t=%.9f observer_writes=0\n',offset,value,mask,cpu.state['PC'].value,machine.time:as_double())) end
end)
