-- Passive continuation context; physical fixture owns temp1 and temp6..temp9.
local machine = manager.machine
local debugger = machine.debugger
assert(machine.system.name == 'nsm3hle')
local sites = {{'21c348','store',0},{'21c41c','dispatch',2},{'21bf86','zero_gate',3},
               {'21c01c','nonzero_exit',4},{'21bffa','publish',5}}
for _,site in ipairs(sites) do debugger:command('temp'..site[3]..'=0') end
for _,site in ipairs(sites) do
    local counter = 'temp'..site[3]
    debugger:command('bpset '..site[1]..','..counter..'<512 && r4>=100000 && r4<17fff0,{'..counter..'='..counter..'+1;logerror "8210_phase_context: site='..site[2]..' ordinal=%d root=%08x phase=%08x state=%04x input=%04x message=%08x r0=%08x lr_context=%08x cycles=%016x observer_writes=0\\n",'..counter..',r4,d@(r4+0xc),w@(r4+2),w@(r4+4),d@(r4+8),r0,r14,totalcycles;g}')
end
local stop = emu.add_machine_stop_notifier(function()
    local memory = machine.devices[':maincpu'].spaces.program
    local root = memory:read_u32(0x21bbd4)
    assert(root >= 0x100000 and root < 0x17fff0)
    debugger:command('logerror "8210_phase_terminal: stores=%d dispatches=%d gates=%d exits=%d publications=%d cap=512 t='..
        string.format('%.6f root=%08x phase=%08x state=%04x input=%04x',machine.time:as_double(),root,
                      memory:read_u32(root+12),memory:read_u16(root+2),memory:read_u16(root+4))..
        ' observer_writes=0\\n",temp0,temp2,temp3,temp4,temp5')
end)
_G.noki8210_phase_handles = {stop}
