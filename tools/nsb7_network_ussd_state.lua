-- Save the real reply editor, then replay identical physical replies.
_G.nsb7_security_with_pin=true
local offset=_G.nsb7_ussd_state_host and 4 or 0
_G.nsb7_ussd_snapshots={23}
_G.nsb7_ussd_actions={{24,'End'},{26+offset,'Left Softkey / Menu'},{28+offset,'Left Softkey / Menu'}}
_G.nsb7_ussd_response_times={25+offset,29+offset}
_G.nsb7_ussd_finish=1000
_G.nsb7_state_scenario='ussd'
_G.nsb7_state_observer='nsb7_host_ussd_observe.lua'
_G.nsb7_state_save_time=29.25+offset
_G.nsb7_state_end_time=36+offset
_G.nsb7_state_finish_time=38+offset
local retained={}
_G.nsb7_state_replay_action=function(phase)
    local action=coroutine.create(function()
        local start=manager.machine.time:as_double()
        for index,name in ipairs({'Keypad 2','Left Softkey / Menu'}) do
            assert(emu.wait(start+index-0.5-manager.machine.time:as_double()))
            local key
            for _,port in pairs(manager.machine.ioport.ports) do key=key or port.fields[name] end
            key=assert(key)
            manager.machine:logerror(string.format('nsb7_ussd_replay: phase=%s key=%s state=1 t=%.9f\n',phase,name,manager.machine.time:as_double()))
            key:set_value(1)
            assert(emu.wait(0.2))
            key:set_value(0)
            manager.machine:logerror(string.format('nsb7_ussd_replay: phase=%s key=%s state=0 t=%.9f\n',phase,name,manager.machine.time:as_double()))
        end
    end)
    retained[#retained+1]=action
    assert(coroutine.resume(action))
end
_G.nsb7_ussd_state_actions=retained
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_state_roundtrip.lua')
