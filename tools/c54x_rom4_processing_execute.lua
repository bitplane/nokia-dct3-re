-- Execution-entry observations, separate from speculative program-bus fetches.
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
assert(cpu.debug, 'processing execution probes require the debugger')
machine.debugger.visible_cpu=cpu
local sites={0x32f4,0x330a,0x332b,0x3347,0x3357,0x3360,0x3362,0x336c,
    0x336e,0x2400,0x2402,0x3382,0x3385,0x33a1,0x33a4,0x33ac}
for _,address in ipairs(sites) do
    local action=string.format(
        'logerror "rom4_processing_execute: address=%04x word=%%04x fd=%%04x mode=%%04x flags=%%04x candidate=%%04x control2180=%%04x control21c2=%%04x pair94=%%04x%%04x pair96=%%04x%%04x a=%%010x b=%%010x st1=%%04x bk=%%04x ar0=%%04x ar3=%%04x ar4=%%04x ar5=%%04x ar6=%%04x\\n",pw@%x,dw@6fd,dw@ac,dw@6bc,dw@ad,dw@2180,dw@21c2,dw@2194,dw@2195,dw@2196,dw@2197,a,b,st1,bk,ar0,ar3,ar4,ar5,ar6;',address,address)
    if address==0x330a then
        action=action..'logerror "rom4_processing_coefficients: words=%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x\\n",dw@21a9,dw@21aa,dw@21ab,dw@21ac,dw@21ad,dw@21ae,dw@21af,dw@21b0;'
    end
    cpu.debug:bpset(address,'1',action..'g')
end
machine:logerror('rom4_processing_observer: schema=1 phase=instruction_entry observer_writes=0\n')
machine.debugger.execution_state='run'
