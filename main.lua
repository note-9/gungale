-- =========================================================
-- HOLLOW KNIGHT–STYLE PLATFORMER (BIG WORLD + CAMERA)
-- =========================================================

local TILE = 32
local GRAVITY = 1800
local MAX_FALL = 900

-- ======================
-- WORLD
-- ======================
local world = {
    width = 120,
    height = 40,
    tiles = {}
}

for y = 1, world.height do
    world.tiles[y] = {}
    for x = 1, world.width do
        world.tiles[y][x] = 0
    end
end

-- Ground
for x = 1, world.width do
    for y = 35, world.height do
        world.tiles[y][x] = 1
    end
end

-- Boundaries
for y = 1, world.height do
    world.tiles[y][1] = 1
    world.tiles[y][world.width] = 1
end

-- Platforms
for x = 10, 20 do world.tiles[30][x] = 1 end
for x = 25, 35 do world.tiles[26][x] = 1 end
for x = 40, 55 do world.tiles[28][x] = 1 end
for x = 60, 75 do world.tiles[24][x] = 1 end
for x = 80, 100 do world.tiles[29][x] = 1 end

-- Shafts
for y = 20, 34 do world.tiles[y][22] = 1 end
for y = 18, 34 do world.tiles[y][58] = 1 end
for y = 15, 34 do world.tiles[y][78] = 1 end

-- ======================
-- PLAYER
-- ======================
local player = {
    x = 64, y = 64,
    w = 20, h = 28,
    vx = 0, vy = 0,

    speed = 260,
    accel = 2000,
    friction = 1800,

    jumpForce = 520,
    wallJumpForce = { x = 380, y = 520 },

    grounded = false,
    onWall = false,
    wallDir = 0,

    coyoteTime = 0.1,
    coyoteTimer = 0,

    jumpBuffer = 0.12,
    jumpTimer = 0,

    dashSpeed = 650,
    dashTime = 0.18,
    dashTimer = 0,
    dashCooldown = 0.3,
    dashCooldownTimer = 0,
    dashDir = 0,
}

-- ======================
-- CAMERA
-- ======================
local camera = {
    x = 0, y = 0,
    smooth = 10
}

-- ======================
-- INPUT
-- ======================
local input = {
    left = false,
    right = false,
    jumpPressed = false,
    jumpHeld = false,
    dashPressed = false
}

-- ======================
-- UTILS
-- ======================
local function sign(x) return x < 0 and -1 or x > 0 and 1 or 0 end
local function clamp(x, a, b) return math.max(a, math.min(b, x)) end
local function lerp(a, b, t) return a + (b - a) * t end

-- ======================
-- COLLISION
-- ======================
local function solidAt(px, py)
    local tx = math.floor(px / TILE) + 1
    local ty = math.floor(py / TILE) + 1
    if world.tiles[ty] and world.tiles[ty][tx] then
        return world.tiles[ty][tx] == 1
    end
    return false
end

local function rectCollides(x, y, w, h)
    return solidAt(x, y)
        or solidAt(x + w, y)
        or solidAt(x, y + h)
        or solidAt(x + w, y + h)
end

-- ======================
-- MOVEMENT
-- ======================
local function moveAndCollide(p, dt)
    p.x = p.x + p.vx * dt
    if rectCollides(p.x, p.y, p.w, p.h) then
        p.x = p.x - p.vx * dt
        p.vx = 0
    end

    p.y = p.y + p.vy * dt
    if rectCollides(p.x, p.y, p.w, p.h) then
        p.y = p.y - p.vy * dt
        if p.vy > 0 then
            p.grounded = true
            p.coyoteTimer = p.coyoteTime
        end
        p.vy = 0
    else
        p.grounded = false
    end
end

-- ======================
-- PLAYER UPDATE
-- ======================
local function updatePlayer(p, dt)
    p.coyoteTimer = p.coyoteTimer - dt
    p.jumpTimer = p.jumpTimer - dt
    p.dashCooldownTimer = p.dashCooldownTimer - dt

    p.onWall = false
    p.wallDir = 0
    if solidAt(p.x - 1, p.y + p.h / 2) then
        p.onWall = true
        p.wallDir = -1
    elseif solidAt(p.x + p.w + 1, p.y + p.h / 2) then
        p.onWall = true
        p.wallDir = 1
    end

    if input.left then
        p.vx = p.vx - p.accel * dt
    elseif input.right then
        p.vx = p.vx + p.accel * dt
    else
        p.vx = p.vx - math.min(math.abs(p.vx), p.friction * dt) * sign(p.vx)
    end
    p.vx = clamp(p.vx, -p.speed, p.speed)

    if p.dashTimer <= 0 then
        p.vy = clamp(p.vy + GRAVITY * dt, -9999, MAX_FALL)
    end

    if input.jumpPressed then
        p.jumpTimer = p.jumpBuffer
    end

    if p.jumpTimer > 0 then
        if p.coyoteTimer > 0 then
            p.vy = -p.jumpForce
            p.jumpTimer = 0
            p.coyoteTimer = 0
        elseif p.onWall then
            p.vx = -p.wallDir * p.wallJumpForce.x
            p.vy = -p.wallJumpForce.y
            p.jumpTimer = 0
        end
    end

    if not input.jumpHeld and p.vy < 0 then
        p.vy = p.vy * 0.5
    end

    if p.onWall and not p.grounded and p.vy > 100 then
        p.vy = 100
    end

    if input.dashPressed and p.dashCooldownTimer <= 0 then
        p.dashTimer = p.dashTime
        p.dashCooldownTimer = p.dashCooldown
        p.dashDir = sign(p.vx)
        if p.dashDir == 0 then p.dashDir = 1 end
    end

    if p.dashTimer > 0 then
        p.dashTimer = p.dashTimer - dt
        p.vx = p.dashDir * p.dashSpeed
        p.vy = 0
    end

    moveAndCollide(p, dt)

    input.jumpPressed = false
    input.dashPressed = false
end

-- ======================
-- CAMERA UPDATE
-- ======================
local function updateCamera(dt)
    local screenW, screenH = love.graphics.getDimensions()

    local targetX = player.x + player.w / 2 - screenW / 2
    local targetY = player.y + player.h / 2 - screenH / 2

    camera.x = lerp(camera.x, targetX, camera.smooth * dt)
    camera.y = lerp(camera.y, targetY, camera.smooth * dt)

    -- Clamp to world bounds
    camera.x = clamp(camera.x, 0, world.width * TILE - screenW)
    camera.y = clamp(camera.y, 0, world.height * TILE - screenH)
end

-- ======================
-- LOVE
-- ======================
function love.load()
    love.window.setMode(1280, 720)
end

function love.update(dt)
    updatePlayer(player, dt)
    updateCamera(dt)
end

function love.draw()
    love.graphics.push()
    love.graphics.translate(-camera.x, -camera.y)

    for y = 1, world.height do
        for x = 1, world.width do
            if world.tiles[y][x] == 1 then
                love.graphics.rectangle(
                    "fill",
                    (x - 1) * TILE,
                    (y - 1) * TILE,
                    TILE, TILE
                )
            end
        end
    end

    love.graphics.rectangle("fill", player.x, player.y, player.w, player.h)

    love.graphics.pop()
end

function love.keypressed(k)
    if k == "a" then input.left = true end
    if k == "d" then input.right = true end
    if k == "space" then
        input.jumpPressed = true
        input.jumpHeld = true
    end
    if k == "lshift" then input.dashPressed = true end
end

function love.keyreleased(k)
    if k == "a" then input.left = false end
    if k == "d" then input.right = false end
    if k == "space" then input.jumpHeld = false end
end

