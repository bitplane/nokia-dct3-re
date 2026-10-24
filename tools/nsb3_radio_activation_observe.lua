-- Own activation entrances only; debugger counters are not handset state.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_radio_consumer_observe.lua')
local machine = manager.machine
local probes = {
    {0x2937c4, 'r0==43f || r0==440', 'controller_construct'},
    {0x218bb0, '1', 'initializer'},
    {0x218cac, '1', 'input13a3'},
    {0x211174, '1', 'construct13a3'}}
for index, probe in ipairs(probes) do
    -- Parents reserve temp0..4 (startup plus acquisition observations).
    local counter = 'temp' .. (index + 4)
    machine.debugger:command(counter .. '=0')
    machine.debugger:command(string.format('bpset %x,(%s)&&%s<16,{%s=%s+1;logerror "nsb3_radio_activation: site=%s r0=%%08x caller=%%08x count=%%d observer_writes=0\\n",r0,r14,%s;g}',
        probe[1], probe[2], counter, counter, counter, probe[3], counter))
end
machine.debugger:command('temp9=0')
machine.debugger:command('bpset 213950,r0>=100000&&r0<110000&&temp9<40,{temp9=temp9+1;logerror "nsb3_radio_upstream: header=%08x caller=%08x count=%d observer_writes=0\\n",d@r0,r14,temp9;g}')
machine.debugger:command('bpset 2112dc,temp9<40,{temp9=temp9+1;logerror "nsb3_radio_upstream_entry: site=2112dc caller=%08x count=%d observer_writes=0\\n",r14,temp9;g}')
machine.debugger:command('bpset 2111bc,temp9<40,{temp9=temp9+1;logerror "nsb3_radio_upstream_entry: site=2111bc caller=%08x count=%d observer_writes=0\\n",r14,temp9;g}')
machine.debugger:command('bpset 212658,temp9<40,{temp9=temp9+1;logerror "nsb3_radio_service_start: mode=%08x caller=%08x count=%d observer_writes=0\\n",r0,r14,temp9;g}')
machine.debugger:command('bpset 26c8cc,r0==3&&r1>=100000&&r1<110000&&b@(r1+3)==1a&&temp9<40,{temp9=temp9+1;logerror "nsb3_radio_service_post: object=%08x header=%08x body=%08x caller=%08x count=%d observer_writes=0\\n",r1,d@r1,d@(r1+4),r14,temp9;g}')
local summary = coroutine.create(function()
    assert(emu.wait(64))
    machine.debugger:command('logerror "nsb3_radio_activation_counts: controller=%d initializer=%d input13a3=%d construct13a3=%d observer_writes=0\\n",temp5,temp6,temp7,temp8')
    machine.debugger:command('logerror "nsb3_radio_upstream_count: count=%d cap=64 observer_writes=0\\n",temp9')
end)
assert(coroutine.resume(summary))
