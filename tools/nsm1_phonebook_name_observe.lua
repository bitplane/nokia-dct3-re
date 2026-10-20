-- Bounded formatter namespace/record regression; no guest writes.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_phonebook_reload.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
machine.debugger:command('bpset 259ec8,temp9<20 && totalcycles>c000000,{temp9=temp9+1;logerror "nsm1_name_record_request: pointer=%08x record=%08x selector=%08x offset=%08x cycles=%016x observer_writes=0\\n",r0,r1,r2,r3,totalcycles;g}')
machine.debugger:command('bpset 259ecc,temp9<20 && totalcycles>c000000,{logerror "nsm1_name_record_result: result=%08x data=%08x%08x%08x%08x%08x%08x%08x%08x cycles=%016x observer_writes=0\\n",r0,d@(r13+4),d@(r13+8),d@(r13+c),d@(r13+10),d@(r13+14),d@(r13+18),d@(r13+1c),d@(r13+20),totalcycles;g}')
machine.debugger:command('bpset 259e78,temp6<8 && totalcycles>c000000,{temp6=temp6+1;logerror "nsm1_name_format_entry: mode=%08x index=%08x data=%08x output=%08x caller=%08x cycles=%016x observer_writes=0\\n",r0,r1,r2,r3,r14,totalcycles;g}')
machine.debugger:command('bpset 259cd4,temp7<8 && totalcycles>c000000,{temp7=temp7+1;logerror "nsm1_name_index_result: mode=%08x value=%08x caller=%08x cycles=%016x observer_writes=0\\n",r5,r4,r14,totalcycles;g}')
local taps={}
_G.nsm1_name_taps=taps
local writes=0
taps[#taps+1]=cpu.spaces.program:install_write_tap(0x111cb8,0x111cbb,'nsm1_name_write',function(_,value,mask)
    local now=machine.time:as_double()
    if now<16.6 or now>=17 or writes>=32 then return end
    writes=writes+1
    machine:logerror(string.format('nsm1_name_write: index=%d value=%08x mask=%08x pc=%08x r0=%08x r1=%08x r2=%08x caller=%08x t=%.9f observer_writes=0\n',
        writes,value,mask,cpu.state['PC'].value,cpu.state['R0'].value,cpu.state['R1'].value,cpu.state['R2'].value,cpu.state['R14'].value,now))
end)
