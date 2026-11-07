-- Bounded native complex-product observations; no firmware writes.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_adc_lab.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
machine.debugger.visible_cpu=cpu
-- Base observations reserve temp0..4 and temp9. One phase counter advances
-- at both boundaries, so the cap cannot leave an output breakpoint enabled.
machine.debugger:command('temp5=0;temp6=0')
cpu.debug:bpset(0x419c,'temp5<40',
    'temp5=temp5+1;logerror "rom6_detect_product_input: phase=%d mode=%04x values=%04x,%04x,%04x,%04x st1=%04x observer_writes=0\\n",temp5,dw@56e,dw@2f52,dw@2f53,dw@2f54,dw@2f55,st1;g')
cpu.debug:bpset(0x41a0,'(temp5&1)==1',
    'logerror "rom6_detect_product_output: phase=%d a=%010x b=%010x st1=%04x observer_writes=0\\n",temp5,a,b,st1;temp5=temp5+1;g')
cpu.debug:bpset(0x41da,'temp6<10',
    'temp6=temp6+1;logerror "rom6_detect_metric: sequence=%d a=%010x corr=%04x,%04x,%04x,%04x energy=%04x,%04x thresholds=%04x,%04x mode=%04x st1=%04x observer_writes=0\\n",temp6,a,dw@2f4a,dw@2f4b,dw@2f4c,dw@2f4d,dw@2f4e,dw@2f4f,dw@3eb0,dw@3eb1,dw@56e,st1;g')
local threshold_reads=0
_G.rom6_detect_threshold_tap=cpu.spaces.data:install_read_tap(0x3eaf,0x3eaf,'rom6_detect_threshold',function(_,value)
    threshold_reads=threshold_reads+1
    if threshold_reads<=32 then
        machine:logerror(string.format('rom6_detect_threshold: sequence=%d t=%.9f value=%04x a=%010x cpu_pc=%06x observer_writes=0\n',
            threshold_reads,machine.time:as_double(),value,cpu.state.A.value,cpu.state.GENPC.value))
    end
end)
_G.rom6_detect_stop=emu.add_machine_stop_notifier(function()
    machine.debugger.visible_cpu=cpu
    machine.debugger:command('logerror "rom6_detect_product_complete: phases=%d cap=64 observer_writes=0\\n",temp5')
    machine.debugger:command('logerror "rom6_detect_metric_complete: calls=%d cap=16 observer_writes=0\\n",temp6')
    machine.debugger:command('dasm rom6_detect_program.asm,b40,210,1,:dsp_c54x:cpu')
    machine:logerror(string.format('rom6_detect_threshold_complete: reads=%d cap=32 observer_writes=0\n',threshold_reads))
end)
