function gadget:GetInfo() return {
	name    = "AI DBG",
	desc    = "AI DBG description",
	author  = "rlcevg",
	date    = "something, 2019",
	license = "GNU GPL, v2 or later",
	layer   = -1337,
	enabled = true
} end

local threatData = {
	map = {},
	width = 0,
	height = 0,
	size = 0,  -- tile size
	div = 1.0,
	base = 1.0,
	isDraw = false,
	isPrint = false
}
_G.threatData = threatData

local spGetGroundHeight  = Spring.GetGroundHeight

local mapWidth  = Game.mapSizeX
local mapHeight = Game.mapSizeZ

-------- GL4 THINGS ----------
local luaShaderDir = "LuaUI/Widgets/Include/"
local LuaShader = VFS.Include(luaShaderDir.."LuaShader.lua")
VFS.Include(luaShaderDir.."instancevbotable.lua")

local circleShader = nil
local circleInstanceVBO = nil -- THIS IS THE MOST IMPORTANT THING!
local circleSegments = 8

local vsSrc = [[
#version 420
#line 10000

layout (location = 0) in vec4 circlepointposition; // This contains the circle stuff
layout (location = 1) in vec4 centerposxyz_radius; // this is what is passed for each instance
layout (location = 2) in vec4 color; 

uniform vec4 circleuniforms; // none yet

uniform sampler2D heightmapTex;

out DataVS {
	vec4 worldPos; // pos and radius
	vec4 blendedcolor;
};

//__ENGINEUNIFORMBUFFERDEFS__

#line 11000

float heightAtWorldPos(vec2 w){ // this gets the world height
	vec2 uvhm =   vec2(clamp(w.x,8.0,mapSize.x-8.0),clamp(w.y,8.0, mapSize.y-8.0))/ mapSize.xy; 
	return textureLod(heightmapTex, uvhm, 0.0).x;
}

void main() {
	// blend start to end on mod gf%15
	float timemix = mod(timeInfo.x,10)*(0.1);
	vec4 circleWorldPos = centerposxyz_radius;
	circleWorldPos.xz = circlepointposition.xy * circleWorldPos.w +  circleWorldPos.xz;
	
	// get heightmap 
	circleWorldPos.y = max(0.0,heightAtWorldPos(circleWorldPos.xz))+16.0; // add 16, and make sure its > 0
	
	// dump to FS
	worldPos = circleWorldPos;
	blendedcolor = color; // just dumping this
	gl_Position = cameraViewProj * vec4(circleWorldPos.xyz, 1.0);
}
]]

local fsSrc =  [[
#version 330

#extension GL_ARB_uniform_buffer_object : require
#extension GL_ARB_shading_language_420pack: require

#line 20000

uniform vec4 circleuniforms; 

uniform sampler2D heightmapTex;

//__ENGINEUNIFORMBUFFERDEFS__

in DataVS {
	vec4 worldPos; // w = range
	vec4 blendedcolor;
};

out vec4 fragColor;

void main() {
	fragColor.rgba = blendedcolor.rgba;
	//fragColor.a *= sin((worldPos.x+worldPos.z)*0.12	 - timeInfo.z*0.033415); // stippling or whatever you want
}
]]


local function goodbye(reason)
  Spring.Echo("AI DBG widget exiting with reason: "..reason)
  gadgetHandler:RemoveGadget()
end

local function initgl4()
	local engineUniformBufferDefs = LuaShader.GetEngineUniformBufferDefs()
	circleShader =  LuaShader(
    {
      vertex = vsSrc:gsub("//__ENGINEUNIFORMBUFFERDEFS__", engineUniformBufferDefs),
      fragment = fsSrc:gsub("//__ENGINEUNIFORMBUFFERDEFS__", engineUniformBufferDefs),
      --geometry = gsSrc, no geom shader for now
      uniformInt = {
        heightmapTex = 0, -- but not really needed here
        },
      uniformFloat = {
        circleuniforms = {1,1,1,1}, -- for uniform floats, 4 of them 
      },
    },
    "AI DBG shader GL4" -- the human name
  )
  shaderCompiled = circleShader:Initialize()
  if not shaderCompiled then goodbye("Failed to compile AI DBG shader ") end
  local circleVBO,numVertices = makeCircleVBO(circleSegments)
  local circleInstanceVBOLayout = {
		  {id = 1, name = 'startposrad', size = 4}, -- the start pos + radius
		  {id = 2, name = 'endposrad', size = 4}, --  end pos + radius
		  {id = 3, name = 'color', size = 4}, --- color
		}
  circleInstanceVBO = makeInstanceVBOTable(circleInstanceVBOLayout,128, "AI DBG VBO")
  circleInstanceVBO.numVertices = numVertices
  circleInstanceVBO.vertexVBO = circleVBO
  circleInstanceVBO.VAO = makeVAOandAttach(
	circleInstanceVBO.vertexVBO, 
	circleInstanceVBO.instanceVBO
   )
end


----  END GL4 THINGS --------------

function gadget:Initialize()
	Spring.Echo("Initialize AI DBG")
	
end

local threatMapChanged = false

function gadget:RecvSkirmishAIMessage(teamID, dataStr)
--	Spring.Echo("teamID: " .. tostring(teamID) .. " | dataStr: " .. dataStr)
	local commandData  = "ai_thr_data:"
	local commandSize  = "ai_thr_size:"
	local commandDiv   = "ai_thr_div:"
	local commandDraw  = "ai_thr_draw:"
	local commandPrint = "ai_thr_print:"

	if dataStr:sub(1, #commandData) == commandData then
		-- "ai_thr_data:<val1> <val2> <val3> ..."
		local mapStr = dataStr:sub(#commandData + 1)
		threatMap = {}
		for m in mapStr:gmatch("%S+") do
			threatMap[#threatMap + 1] = m + 0.0;
		end
		threatData.map = threatMap
		threatMapChanged = true
	elseif dataStr:sub(1, #commandSize) == commandSize then
		-- "ai_thr_size:<square_size> <threat_base>"
		local sb = dataStr:sub(commandSize:len() + 1)
		local slash = sb:find(" ", 1, true)
		if not slash then return end
		local ss = tonumber(sb:sub(1, slash - 1))
		local tb = tonumber(sb:sub(slash + 1))
		if not ss or not tb then return end
		threatData.base = tb
		threatData.size = ss
		threatData.width = mapWidth / threatData.size
		threatData.height = mapHeight / threatData.size
	elseif dataStr:sub(1, #commandDiv) == commandDiv then
		-- "ai_thr_div:<divider>"
		threatData.div = tonumber(dataStr:sub(#commandDiv + 1))
	elseif dataStr:sub(1, #commandDraw) == commandDraw then
		-- "ai_thr_draw:"
		threatData.isDraw = not threatData.isDraw
		return threatData.isDraw and "1" or "0"
	elseif dataStr:sub(1, #commandPrint) == commandPrint then
		-- "ai_thr_print:"
		threatData.isPrint = not threatData.isPrint
		return threatData.isPrint and "1" or "0"
	end
end

function gadget:DrawWorldPreUnit()
	if SYNCED and #SYNCED.threatData.map > 0 then
		local threatMap = SYNCED.threatData.map
		local width = SYNCED.threatData.width
		local height = SYNCED.threatData.height
		local size = SYNCED.threatData.size
		local div = SYNCED.threatData.div
		local base = SYNCED.threatData.base
--		Spring.Echo(threatMap[0 * width + 1])
--		Spring.Echo(threatMap[(height - 1) * width + width])

		if SYNCED.threatData.isDraw then
			-- I AM GOING TO DO THIS THE STUPID AND SLOW WAY, ideally, you 
			
			if threatMapChanged then 
				threatMapChanged = false
				clearInstanceTable(circleInstanceVBO) -- remove all our previous geometry from buffer
				for x = 1, width do
					px = (x - 1) * size
					for z = 0, height - 1 do
						value = threatMap[z * width + x] - base
						local mycolor = {0.0, 0.0, 0.0, 0.0}
						
						if value > 0 then
							pz = z * size
							value = value / div
							mycolor[1] = value
							--gl.Color(value, 0.0, 0.0, 0.6)
							--gl.DrawGroundQuad(px, pz, px + size, pz + size)
						elseif value < 0 then
							pz = z * size
							value = -value / div
							mycolor[3] = value
							--gl.Color(0.0, 0.0, value, 0.6)
							--gl.DrawGroundQuad(px, pz, px + size, pz + size)
						end
						pushElementInstance( -- pushElementInstance(iT,thisInstance, instanceID, updateExisting, noUpload, unitID)
							circleInstanceVBO, -- the buffer to push into
							{ -- the data per instance 
								px + size/2, 0, pz + size/2, size/2, -- in vec4 centerposxyz_radius; 
								mycolor[1],mycolor[2],mycolor[3],mycolor[4], -- in vec4 color;
							},
							nil, -- no instance key given, if given, it will update existing instance with same key instead of new instance
							true, -- updateExisting
							true) -- noUpload = true, we will upload whole buffer to gpu after we are done filling it
					end
				end
				uploadAllElements(circleInstanceVBO) -- upload everything if noUpload was used
			end
			
			
			-- This part is the drawing part, the whole pushElementInstance only needs to be called every time the 
			glDepthTest(false) -- so that it doesnt get hidden under terrain
			gl.Texture(0, "$heightmap")
			circleShader:Activate()
			drawInstanceVBO(circleInstanceVBO)
			circleShader:Deactivate()
			gl.Texture(0, false)
			glDepthTest(false)
			
		end

		if SYNCED.threatData.isPrint then
			local halfSize = size / 2;
--			local cx, cy, cz = Spring.GetCameraDirection()
--			local dir = ((math.atan2(cx, cz) / math.pi) + 1) * 180

			for x = 1, width do
				px = (x - 1) * size + halfSize
				for z = 0, height - 1 do
					value = threatMap[z * width + x]
					if value ~= base then
						pz = z * size + halfSize
						local py = spGetGroundHeight(px, pz)
						if py < 0 then py = 0 end

						gl.PushMatrix()

						gl.Translate(px, py, pz)
						gl.Rotate(-90, 1, 0, 0)
--						gl.Rotate(dir, 0, 0, 1)
						gl.Text(("%.2f"):format(value), 0.0, 0.0, 14, "cno")

						gl.PopMatrix()
					end
				end
			end
		end

		gl.Color(1,1,1,1)
	end
end
