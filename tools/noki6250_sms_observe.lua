-- Physical received-message UI probe; no firmware-memory writes.
local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
dofile(directory .. "noki6250_runtime_observe.lua")
local machine = manager.machine
local function cell(column, row)
    for _, field in pairs(machine.ioport.ports[":COL." .. column].fields) do
        if field.mask == (1 << row) then return field end
    end
    error("missing physical matrix cell")
end
local actions = {
    {18, cell(1, 1), 1}, {18.15, cell(1, 1), 0},
    {22, cell(1, 1), 1}, {22.15, cell(1, 1), 0},
    {26, cell(1, 1), 1}, {26.15, cell(1, 1), 0},
}
if os.getenv("NOKIA_DCT3_6250_SMS_DELETE") == "1" then
    actions[#actions + 1] = {30, cell(1, 1), 1}
    actions[#actions + 1] = {30.15, cell(1, 1), 0}
end
if os.getenv("NOKIA_DCT3_6250_SMS_REPLY") == "1" then
    actions = {
        {18, cell(1, 1), 1}, {18.15, cell(1, 1), 0},
        {22, cell(1, 1), 1}, {22.15, cell(1, 1), 0},
        {24, cell(1, 3), 1}, {24.15, cell(1, 3), 0},
        {26, cell(1, 1), 1}, {26.15, cell(1, 1), 0},
        {28, cell(1, 1), 1}, {28.15, cell(1, 1), 0},
        {30, cell(2, 2), 1}, {30.15, cell(2, 2), 0},
        {30.3, cell(2, 2), 1}, {30.45, cell(2, 2), 0},
        {32, cell(2, 2), 1}, {32.15, cell(2, 2), 0},
        {32.3, cell(2, 2), 1}, {32.45, cell(2, 2), 0},
        {32.6, cell(2, 2), 1}, {32.75, cell(2, 2), 0},
        {36, cell(1, 1), 1}, {36.15, cell(1, 1), 0},
        {38, cell(1, 1), 1}, {38.15, cell(1, 1), 0},
        {42, cell(1, 1), 1}, {42.15, cell(1, 1), 0},
    }
end
local captures = {17, 20, 24, 28, 34, 37, 40, 44, 48}
local next_action, next_capture = 1, 1
emu.register_periodic(function()
    local time = machine.time:as_double()
    local action = actions[next_action]
    if action and time >= action[1] then
        action[2]:set_value(action[3])
        machine:logerror(string.format("6250_sms_input: step=%d pressed=%d t=%.6f\n",
            next_action, action[3], time))
        next_action = next_action + 1
    end
    if captures[next_capture] and time >= captures[next_capture] then
        machine.screens[":screen"]:snapshot("6250_sms_" .. next_capture .. ".png")
        next_capture = next_capture + 1
    end
end)
