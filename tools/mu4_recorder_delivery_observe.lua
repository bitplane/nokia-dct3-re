-- Execute-boundary message construction and RAM-write acknowledgements only.
-- A is the already-computed record pointer at 038acd; live SP is not used.
-- Debugger logerror supports %d/%x, not the C++ logger's %u conversion.
local machine = manager.machine
local debugger = machine.debugger
assert(machine.system.name == 'mu4nand')
debugger:command('temp0=0')
debugger:command('temp1=0')
debugger:command('bpset 38acd,temp0<1000,{temp0=temp0+1;logerror "mu4_recorder_delivery_post: ordinal=%d length=%04x selector=%04x source=%04x words=%04x tail=%04x,%04x ea=%04x ed=%04x ee=%04x f0=%04x f1=%04x f2=%04x f3=%04x cycles=%016x observer_writes=0\\n",temp0,dw@((a&ffff)-2),dw@(a&ffff),dw@((a&ffff)+1),dw@((a&ffff)+2),dw@((a&ffff)+3),dw@((a&ffff)+4),dw@05ea,dw@05ed,dw@05ee,dw@05f0,dw@05f1,dw@05f2,dw@05f3,totalcycles;g}')
for _,address in ipairs({'38b50','38b87'}) do
    debugger:command('bpset '..address..',temp1<1000,{temp1=temp1+1;logerror "mu4_recorder_delivery_consume: ordinal=%d instruction=%06x before=%04x amount=%04x after=%04x f0=%04x cycles=%016x observer_writes=0\\n",temp1,curpc,dw@05ee,dw@05ef,a&ffff,dw@05f0,totalcycles;g}')
end
local stop = emu.add_machine_stop_notifier(function()
    debugger:command('logerror "mu4_recorder_delivery_terminal: posts=%d consumes=%d cap=1000 observer_writes=0\\n",temp0,temp1')
end)
_G.mu4_recorder_delivery_handles = {stop}
debugger:command('g')
