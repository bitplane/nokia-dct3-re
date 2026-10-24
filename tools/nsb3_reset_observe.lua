-- Read-only reset-policy provenance on an otherwise ordinary authenticated boot.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_pin_correct.lua')
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces.program
local count = 0
_G.nsb3_reason_watch = memory:install_write_tap(0x10ff80, 0x10ff83,
    'nsb3_reset_reason', function(_, data, mask)
        if mask & 0xff000000 == 0 then return end
        count = count + 1
        if count <= 64 then
            machine:logerror(string.format('nsb3_reset_reason_write: value=%02x pc=%08x lr=%08x t=%.9f observer_writes=0\n',
                (data >> 24) & 255, cpu.state.R15.value, cpu.state.R14.value, machine.time:as_double()))
        end
    end)
cpu.debug:bpset(0x23fdfe, '1',
    'logerror "nsb3_reset_event_tail: reason=%02x caller=%08x observer_writes=0\\n",b@0x10ff80,r14;g')
cpu.debug:bpset(0x2b3b94, '1',
    'logerror "nsb3_receive_window: activity=%04x observer_writes=0\\n",w@0x10c4ce;g')
cpu.debug:bpset(0x2a78a6, '1',
    'logerror "nsb3_reset_policy_entry: kind=%08x caller=%08x observer_writes=0\\n",r0,r14;g')
