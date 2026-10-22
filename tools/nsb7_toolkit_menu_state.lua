-- Replay the same physical card-menu selection from an exact saved machine.
_G.nsb7_toolkit_menu_hold=true
_G.nsb7_state_scenario='toolkit_menu'
_G.nsb7_state_observer='nsb7_toolkit_menu.lua'
_G.nsb7_state_save_time=85.25
_G.nsb7_state_end_time=90
_G.nsb7_state_finish_time=92
local retained={}
_G.nsb7_state_replay_action=function(phase)
    local action=coroutine.create(function()
        assert(emu.wait(0.5))
        local key
        for _,port in pairs(manager.machine.ioport.ports) do
            key=key or port.fields['Left Softkey / Menu']
        end
        key=assert(key)
        manager.machine:logerror('nsb7_toolkit_state: action=menu_select phase='..phase..'\n')
        key:set_value(1)
        if not emu.wait(0.2) then key:set_value(0); error('selection interrupted') end
        key:set_value(0)
    end)
    retained[#retained+1]=action
    assert(coroutine.resume(action))
end
_G.nsb7_toolkit_state_actions=retained
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_state_roundtrip.lua')
