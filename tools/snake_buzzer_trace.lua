-- Test harness, not an observer: forces meals, then logs DSP shared and PUP writes.
-- TOP_SCORE=100 selects the death tones; unset leaves the record fanfare.
dofile(os.getenv("REPO") .. "/mame_nokia_dct3_input_exerciser.lua")
local machine = manager.machine
local space = machine.devices[":maincpu"].spaces["program"]
snake_trace_taps = {}
local deltas = {[0]={0,-1},[1]={1,0},[2]={0,1},[3]={-1,0}}
local top_score = tonumber(os.getenv("TOP_SCORE") or "")
local stop_after = tonumber(os.getenv("EATS") or "3")
local until_t, last_key, last_score = 0, "", 0
local done_eating = false
local function open(t) until_t = machine.time:as_double() + t end
snake_trace_taps[1] = space:install_write_tap(0x10000, 0x10fff, "st_shared", function(o, d, m)
  if machine.time:as_double() < until_t then
    machine:logerror(string.format("SHARED t=%.6f off=%05x data=%08x mask=%08x\n", machine.time:as_double(), o, d, m))
  end
  return d
end)
snake_trace_taps[2] = space:install_write_tap(0x20014, 0x2001f, "st_pup", function(o, d, m)
  machine:logerror(string.format("PUP t=%.6f off=%05x data=%08x mask=%08x\n", machine.time:as_double(), o, d, m))
  return d
end)
local function tick()
  local t = machine.time:as_double()
  local hx, hy, dir = space:read_u8(0x110318), space:read_u8(0x110319), space:read_u8(0x110320)
  local fx, fy = space:read_u8(0x110340), space:read_u8(0x110341)
  local score = space:read_u16(0x110414)
  local key = string.format("h=%d,%d f=%d,%d dir=%d score=%d", hx, hy, fx, fy, dir, score)
  if key ~= last_key then machine:logerror(string.format("SNAKE t=%.6f %s\n", t, key)); last_key = key end
  if score ~= last_score then open(1000.0); machine:logerror(string.format("EAT t=%.6f score=%d\n", t, score)); last_score = score end
  if score >= stop_after then done_eating = true; if top_score then space:write_u16(0x110416, top_score) end end
  if not done_eating and score < stop_after and hx < 30 and hy < 30 and deltas[dir] then
    local nx, ny = hx + deltas[dir][1], hy + deltas[dir][2]
    if nx >= 0 and ny >= 0 and (nx ~= fx or ny ~= fy) then
      space:write_u8(0x110340, nx); space:write_u8(0x110341, ny)
    end
  end
end
snake_trace_co = coroutine.create(function()
  while machine.phase ~= "exit" do
    emu.wait(0.02)
    tick()
  end
end)
assert(coroutine.resume(snake_trace_co))
