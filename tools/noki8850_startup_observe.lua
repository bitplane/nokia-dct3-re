-- Read-only CPU probes plus one raw physical matrix press, not UI acceptance.
-- Invoke with -debug -debugger none. ARM debugger expressions use r14, not lr.
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
assert(cpu.debug, "8850 startup probes require the MAME debugger")
cpu.debug:bpset(0x2408c8, nil,
    'logerror "8850_checksum_entry r14=%08x\\n",r14;g')
cpu.debug:bpset(0x240b92, nil,
    'logerror "8850_checksum_compare computed=%04x stored=%04x\\n",r1,r0;g')
cpu.debug:bpset(0x240dbc, nil,
    'logerror "8850_service_reply class=%02x command=%02x status=%02x\\n",b@(r0+3),b@(r0+8),b@(r0+9);g')
cpu.debug:bpset(0x28846c, "temp1<200",
    'temp1=temp1+1;logerror "8850_queue_publish destination=%08x source=%08x r14=%08x\\n",r0,r1,r14;g')
cpu.debug:bpset(0x3054ac, nil,
    'logerror "8850_keypad_disable r14=%08x\\n",r14;g')
cpu.debug:bpset(0x305410, nil,
    'logerror "8850_keypad_raw key=%02x\\n",r0;g')
for _, address in ipairs({0x2a12e0, 0x2a12fe, 0x2a1358}) do
    cpu.debug:bpset(address, nil,
        'logerror "8850_keypad_decoded key=%02x\\n",r0;g')
end
cpu.debug:bpset(0x2885bc, "r0==1 && temp2<200",
    'temp2=temp2+1;logerror "8850_task1_post report=%08x r14=%08x\\n",r1,r14;g')
cpu.debug:bpset(0x2a1be8, nil,
    'logerror "8850_startup_receive report=%08x\\n",r0;g')
cpu.debug:bpset(0x2a0dde, "temp3<200",
    'temp3=temp3+1;logerror "8850_task1_receive report=%08x r14=%08x\\n",r0,r14;g')
cpu.debug:bpset(0x2a1a10, "temp4<200",
    'temp4=temp4+1;logerror "8850_startup_dispatch report=%08x state=%04x base=%08x\\n",r0,w@(r4+4),r4;g')
cpu.debug:bpset(0x2a1c96, "temp5<200",
    'temp5=temp5+1;logerror "8850_startup_check power=%02x reports=%02x\\n",b@13ff00,b@137fdd;g')
cpu.debug:bpset(0x244caa, nil,
    'logerror "8850_report14_predecessor pc=%08x\\n",pc;g')
cpu.debug:bpset(0x2ff870, nil,
    'logerror "8850_report14_stub r14=%08x\\n",r14;g')
cpu.debug:bpset(0x2463a4, "temp6<200",
    'temp6=temp6+1;logerror "8850_report14_owner_receive event=%08x state=%04x base=%08x count=%02x init=%02x flag=%02x\\n",r0,w@(r6+1c),r6,b@(r6+4),b@(r6+a),b@(r6+d);g')
cpu.debug:bpset(0x245c28, nil,
    'logerror "8850_owner_status_read value=%08x\\n",r0;g')
cpu.debug:bpset(0x2429a6, nil,
    'logerror "8850_channel_map_receive command=%02x length=%02x\\n",b@(r0+8),b@(r0+5);g')
cpu.debug:bpset(0x304a72, nil,
    'logerror "8850_channel_map_apply source=%08x mode=%02x node=%02x kind=%02x\\n",r3,r1,r0,r2;g')
cpu.debug:bpset(0x304a4a, "temp7<100",
    'temp7=temp7+1;logerror "8850_channel_available resource=%08x\\n",r0;g')
cpu.debug:bpset(0x2d1140, nil,
    'logerror "8850_ui_start r14=%08x\\n",r14;g')
cpu.debug:bpset(0x3014b6, "temp8<100",
    'temp8=temp8+1;logerror "8850_ui_catalogue input=%08x r14=%08x\\n",r0,r14;g')
cpu.debug:bpset(0x2886c0, "b@1115d2==5 && temp0<100",
    'temp0=temp0+1;logerror "8850_task5_receive_entry r14=%08x\\n",r14;g')
cpu.debug:bpset(0x30134e, "temp9<100",
    'temp9=temp9+1;logerror "8850_catalogue_receive message=%08x input=%04x\\n",r0,w@r0;g')
for _, address in ipairs({0x300aae, 0x2fce32, 0x27bd84, 0x270f00,
        0x2c3d38, 0x271bd8, 0x266730, 0x2bd40c}) do
    cpu.debug:bpset(address, "r0==731 || r0==735 || r0==e77 || r0==5e4 || r0==23f",
        string.format('logerror "8850_ui_filter entry=%08x input=%%04x\\n",r0;g', address))
end
cpu.debug:bpset(0x268a3c, nil,
    'logerror "8850_ui_735_handler entry=268a3c\\n";g')
cpu.debug:bpset(0x301564, "temp1<250",
    'temp1=temp1+1;logerror "8850_catalogue_internal input=%08x r14=%08x\\n",r0,r14;g')
cpu.debug:bpset(0x287a0e, "r0==51",
    'logerror "8850_ui_timer51_arm delay=%08x r14=%08x\\n",r1,r14;g')
cpu.debug:bpset(0x300894, "(r0>=247 && r0<=263) || r0==621",
    'logerror "8850_startup_transition record=%04x selector=%02x raw_slot=%02x\\n",r0,b@(325b80+r0*8),b@(13fc00+b@(325b80+r0*8));g')
cpu.debug:bpset(0x303214, nil,
    'logerror "8850_virtual_db source=%02x\\n",b@13805a;g')
cpu.debug:bpset(0x301548, "(r0&1fff)==e77",
    'logerror "8850_ui_fallback_queued input=%08x r14=%08x\\n",r0,r14;g')
cpu.debug:bpset(0x268068, nil,
    'logerror "8850_ui_fallback_handler input=%04x\\n",r0;g')
cpu.debug:bpset(0x255c5c, "r14==268073",
    'logerror "8850_ui_fallback_context input=%04x first=%02x second=%02x\\n",r0,r1,r2;g')
cpu.debug:bpset(0x255ebc, "r7==e77",
    'logerror "8850_ui_fallback_default input=%04x\\n",r7;g')
cpu.debug:bpset(0x268dec, nil,
    'logerror "8850_ui_timer51_handler gate=%02x\\n",b@13fc57;g')
cpu.debug:bpset(0x2f1d9c, nil,
    'logerror "8850_ui_mode_update selector=%02x value=%02x r14=%08x\\n",r0,r1,r14;g')
cpu.debug:bpset(0x2f19de, nil,
    'logerror "8850_ui_index_refresh index=%02x enabled=%02x\\n",r0,b@(137e94+r0);g')
cpu.debug:bpset(0x25f2ec, "r0==74",
    'logerror "8850_ui_resource74 index=%02x value=%02x\\n",r1,r2;g')
cpu.debug:bpset(0x2f1a28, nil,
    'logerror "8850_ui_index_activate index=%02x first=%08x second=%08x third=%08x r14=%08x\\n",r0,r1,r2,r3,r14;g')
cpu.debug:bpset(0x25f12e, "r0==5a || r0==5b || r0==78",
    'logerror "8850_ui_resource_request class=%02x value=%08x r14=%08x\\n",r0,r1,r14;g')
cpu.debug:bpset(0x25f114, "r5==5a",
    'logerror "8850_ui_resource_constructed object=%08x content=%08x predecessor=%08x\\n",r4,r6,d@(r4+4);g')
cpu.debug:bpset(0x25f514, "b@1350a5!=0 && temp7<120",
    'temp7=temp7+1;logerror "8850_ui_resource_flush pending=%02x r14=%08x\\n",b@1350a5,r14;g')
cpu.debug:bpset(0x25f660, "b@(r4+3e)==5a",
    'logerror "8850_ui_resource_renderer object=%08x target=%08x command=%02x flags=%04x\\n",r4,r2,r1,w@(r4+3c);g')
cpu.debug:bpset(0x25fece, "b@(r4+3e)==5a",
    'logerror "8850_ui_resource_paint_dispatch object=%08x target=%08x command=%02x flags=%04x\\n",r4,r2,r1,w@(r4+3c);g')
cpu.debug:bpset(0x23fcf0, "b@(r4+3e)==5a",
    'logerror "8850_ui_renderer_layout_done object=%08x flags=%04x options=%08x\\n",r4,w@(r4+3c),d@(r4+28);g')
cpu.debug:bpset(0x23d4c0, "b@(r1+3e)==5a",
    'logerror "8850_ui_layout_entry object=%08x command=%02x\\n",r1,r2;g')
cpu.debug:bpset(0x23ea62, "b@(r4+3e)==5a",
    'logerror "8850_ui_text_resolved object=%08x source=%08x text=%08x prefix=%08x\\n",r4,d@(r4+34),r0,d@r0;g')
for _, address in ipairs({0x23eab6, 0x23ef1e, 0x23f4d4, 0x23e8f8, 0x23f5e8, 0x23f610}) do
    cpu.debug:bpset(address, nil,
        string.format('logerror "8850_ui_text_stage pc=%08x r0=%%08x r1=%%08x r2=%%08x r3=%%08x\\n",r0,r1,r2,r3;g', address))
end
cpu.debug:bpset(0x27ed94, nil,
    'logerror "8850_ui_glyph_state first=%08x second=%08x\\n",r1,d@(r0+10);g')
cpu.debug:bpset(0x27ed9e, nil,
    'logerror "8850_ui_glyph_request x=%02x y=%02x source=%08x count=%02x mode=%02x\\n",b@(r4+8),b@(r4+9),d@r4,b@(r4+a),b@(r4+b);g')
cpu.debug:bpset(0x27e662, "temp7<180",
    'temp7=temp7+1;logerror "8850_ui_glyph_blit destination=%08x operation=%02x\\n",d@r0,b@(r0+d);g')
cpu.debug:bpset(0x304a18, "r5==7305",
    'logerror "8850_ui_display_submit resource=%04x allowed=%02x\\n",r5,r0;g')
cpu.debug:bpset(0x27f550, nil,
    'logerror "8850_lcd_framebuffer_transfer start=%02x count=%02x flags=%02x pixels=%08x/%08x mask=%08x/%08x\\n",r0,r1,b@13029e,d@1304e3,d@130592,d@1302eb,d@13039a;g')
cpu.debug:bpset(0x27ed5a, nil,
    'logerror "8850_glyph_framebuffer_done pixels=%08x/%08x mask=%08x/%08x\\n",d@1304e3,d@130592,d@1302eb,d@13039a;g')
cpu.debug:bpset(0x2fd09c, "r0==5e4 || r0==731 || r0==735",
    'logerror "8850_ui_context_dispatch input=%04x target=%08x context=%02x\\n",r0,r1,b@(r5+3);g')
cpu.debug:bpset(0x2ad608, "r0==5e4 || r0==731 || r0==735 || r0==ca || r0==35c || r0==370 || r0==5dc || r0==5e1",
    'logerror "8850_ui_context_mode input=%04x mode=%02x phase=%02x flag=%02x\\n",r0,b@13fc6c,b@13fc72,b@13fc4b;g')
for _, address in ipairs({0x2ad1be, 0x2ad2c0, 0x2ad334, 0x2ad3d4, 0x2ad4c0}) do
    cpu.debug:bpset(address, "r0==5e4 || r0==731 || r0==735",
        string.format('logerror "8850_ui_context_branch entry=%08x input=%%04x\\n",r0;g', address))
end
cpu.debug:go()

local mask_writes = 0
local mask_tap = cpu.spaces["program"]:install_write_tap(0x20068, 0x2006b,
    "8850_keypad_control", function(address, data, mask)
        if mask_writes >= 32 then return end
        mask_writes = mask_writes + 1
        machine:logerror(string.format(
            "8850_keypad_control: address=%08x data=%08x mask=%08x pc=%08x t=%.6f\n",
            address, data, mask, cpu.state["PC"].value, machine.time:as_double()))
    end)
_G.noki8850_keypad_control_tap = mask_tap
local timer_writes = 0
_G.noki8850_ui_timer_tap = cpu.spaces["program"]:install_write_tap(
    0x111b18, 0x111b23, "8850_ui_timer51", function(address, data, mask)
        if timer_writes >= 64 then return end
        timer_writes = timer_writes + 1
        machine:logerror(string.format(
            "8850_ui_timer51_write: address=%08x data=%08x mask=%08x pc=%08x t=%.6f\n",
            address, data, mask, cpu.state["PC"].value, machine.time:as_double()))
    end)
_G.noki8850_mode_source_tap = cpu.spaces["program"]:install_write_tap(
    0x138058, 0x13805b, "8850_mode_source", function(address, data, mask)
        machine:logerror(string.format(
            "8850_mode_source_write: data=%08x mask=%08x pc=%08x t=%.6f\n",
            data, mask, cpu.state["PC"].value, machine.time:as_double()))
    end)

local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
dofile(directory .. "noki8850_frontier_observe.lua")

local input = coroutine.create(function()
    if not emu.wait(5) then return end
    -- Own-ROM table 33f504 maps row 1/column 1 to softkey 19.
    local key = assert(machine.ioport.ports[":COL.1"].fields["Menu"])
    key:set_value(1)
    machine:logerror("8850_matrix_press: column=1 host_bit=02\n")
    if not emu.wait(0.15) then key:set_value(0); return end
    key:set_value(0)
    if not emu.wait(0.85) then return end
    machine.screens[":screen"]:snapshot("8850_after_matrix.png")
end)
_G.noki8850_startup_input = input
assert(coroutine.resume(input))
