function gadget:GetInfo() return {
	name    = "AI DBG",
	desc    = "AI DBG description",
	author  = "rlcevg, Beherith",
	date    = "something, 2019",
	license = "GNU GPL, v2 or later",
	layer   = -1337,
	enabled = true
} end

local mapWidth  = Game.mapSizeX
local mapHeight = Game.mapSizeZ

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
if (gadgetHandler:IsSyncedCode()) then  -- Synced
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

local aiData = {
	map = {},
	width = 0,
	height = 0,
	size = 0,  -- tile size
	div = 1.0,
	base = 1.0,
	isDraw = false,
	isPrint = false,
	mapChanged = 0,  -- frame of last change

	marks = {},
}
_G.aiData = aiData

function gadget:RecvSkirmishAIMessage(teamID, dataStr)
	-- TODO: SendToUnsynced() raw strings and parse in unsynced
--	Spring.Echo("teamID: " .. tostring(teamID) .. " | dataStr: " .. dataStr)
	local cmdThrData  = "ai_thr_data:"
	local cmdThrSize  = "ai_thr_size:"
	local cmdThrDiv   = "ai_thr_div:"
	local cmdThrDraw  = "ai_thr_draw:"
	local cmdThrPrint = "ai_thr_print:"
	local cmdMrkAdd   = "ai_mrk_add:"
	local cmdMrkClear = "ai_mrk_clear:"

	if dataStr:sub(1, #cmdThrData) == cmdThrData then
		-- "ai_thr_data:<val1> <val2> <val3> ..."
		local mapStr = dataStr:sub(#cmdThrData + 1)
		local threatMap = {}
		for m in mapStr:gmatch("%S+") do
			threatMap[#threatMap + 1] = m + 0.0;
		end
		aiData.map = threatMap
		aiData.mapChanged = Spring.GetGameFrame()
		--Spring.Echo("aiData.mapChanged", Spring.GetGameFrame())
	elseif dataStr:sub(1, #cmdThrSize) == cmdThrSize then
		-- "ai_thr_size:<square_size> <threat_base>"
		local sb = dataStr:sub(cmdThrSize:len() + 1)
		local slash = sb:find(" ", 1, true)
		if not slash then return end
		local ss = tonumber(sb:sub(1, slash - 1))
		local tb = tonumber(sb:sub(slash + 1))
		if not ss or not tb then return end
		aiData.base = tb
		aiData.size = ss
		aiData.width = mapWidth / aiData.size
		aiData.height = mapHeight / aiData.size
	elseif dataStr:sub(1, #cmdThrDiv) == cmdThrDiv then
		-- "ai_thr_div:<divider>"
		aiData.div = tonumber(dataStr:sub(#cmdThrDiv + 1))
	elseif dataStr:sub(1, #cmdThrDraw) == cmdThrDraw then
		-- "ai_thr_draw:"
		aiData.isDraw = not aiData.isDraw
		return aiData.isDraw and "1" or "0"
	elseif dataStr:sub(1, #cmdThrPrint) == cmdThrPrint then
		-- "ai_thr_print:"
		aiData.isPrint = not aiData.isPrint
		return aiData.isPrint and "1" or "0"
	elseif dataStr:sub(1, #cmdMrkAdd) == cmdMrkAdd then
		local markStr = dataStr:sub(#cmdMrkAdd + 1)
		local mark = {}
		for m in markStr:gmatch("%S+") do
			mark[#mark + 1] = m + 0.0;
		end
		aiData.marks[#aiData.marks + 1] = mark;
	elseif dataStr:sub(1, #cmdMrkClear) == cmdMrkClear then
		aiData.marks = {}
	end
end

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
else  -- Unsynced
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

-------- GL4 THINGS ----------
local function InitGL4()
	Spring.Echo("Initializing GL4")
	local goodbye = function(reason)
		Spring.Echo("AI DBG widget exiting with reason: "..reason)
		gadgetHandler:RemoveGadget()
	end


	local luaShaderDir = "LuaUI/Widgets/Include/"
	local LuaShader = VFS.Include(luaShaderDir.."LuaShader.lua")
	VFS.Include(luaShaderDir.."instancevbotable.lua")

	local circleShader = nil
	local circleInstanceVBO = nil -- THIS IS THE MOST IMPORTANT THING!
	local circleSegments = 16

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
		// float timemix = mod(timeInfo.x,10)*(0.1); //timeInfo.x contains gameframe
		vec4 circleWorldPos = centerposxyz_radius;
		circleWorldPos.xz = circlepointposition.xy * circleWorldPos.w +  circleWorldPos.xz;

		// get heightmap
		circleWorldPos.y = max(0.0,heightAtWorldPos(circleWorldPos.xz))+2.0; // add 2.0, and make sure its > 0

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
	if not shaderCompiled then goodbye("Failed to compile AI DBG shader") end
	local circleVBO,numVertices = makeCircleVBO(circleSegments + 1)
	local circleInstanceVBOLayout = {
		{id = 1, name = 'centerposxyz_radius', size = 4}, -- the start pos + radius
		{id = 2, name = 'color', size = 4}, --- color
	}
	circleInstanceVBO = makeInstanceVBOTable(circleInstanceVBOLayout, 128, "AI DBG VBO")
	circleInstanceVBO.numVertices = numVertices
	circleInstanceVBO.vertexVBO = circleVBO
	circleInstanceVBO.primitiveType = GL.TRIANGLE_FAN -- ugh forgot this one
	circleInstanceVBO.VAO = makeVAOandAttach(
		circleInstanceVBO.vertexVBO,
		circleInstanceVBO.instanceVBO
	)
	return circleInstanceVBO, circleShader
end
----  END GL4 THINGS --------------

local circleInstanceVBOsynced = nil
local circleShadersynced = nil

function gadget:Initialize()
	Spring.Echo("Initialize AI DBG")
	-- NOTE: Uncomment to enable GL4 threat drawing
	circleInstanceVBOsynced, circleShadersynced = InitGL4()
end

function gadget:Shutdown()
	Spring.Echo("Shutdown AI DBG")
end

local spGetGroundHeight  = Spring.GetGroundHeight

local lastupdateframe = 0
local aiData = nil

function gadget:DrawWorldPreUnit()
	if not SYNCED then
		return
	end

	aiData = SYNCED.aiData  -- access to SYNCED copies whole state, so use once or better SendToUnsynced()
	if aiData == nil then return end
	local width = aiData.width
	local height = aiData.height
	local size = aiData.size
	local div = aiData.div
	local base = aiData.base
--	Spring.Echo(threatMap[0 * width + 1])
--	Spring.Echo(threatMap[(height - 1) * width + width])

	if aiData.isDraw then
		local threatMap = aiData.map
		if circleInstanceVBOsynced == nil then
			-- old way
			for x = 1, width do
				px = (x - 1) * size
				for z = 0, height - 1 do
					value = threatMap[z * width + x] - base
					draw = (value ~= 0)
					local mycolor = {0.0, 0.0, 0.0, 0.6} -- alpha defined here!

					if value > 0 then
						pz = z * size
						value = value / div
						mycolor[1] = value
					elseif value < 0 then
						pz = z * size
						value = -value / div
						mycolor[3] = value
					end
					if draw then
						-- could create DrawList and update it on aiData.mapChanged. But old way will be deprecated anyway
						gl.Color(mycolor)
						gl.DrawGroundQuad(px, pz, px + size, pz + size)
					end
				end
			end
		else
			-- GL4 way
			if aiData.mapChanged > lastupdateframe then
				lastupdateframe = Spring.GetGameFrame()
-- 				Spring.Echo("mapChanged updated", lastupdateframe, aiData.mapChanged)

				clearInstanceTable(circleInstanceVBOsynced) -- remove all our previous geometry from buffer
				for x = 1, width do
					px = (x - 1) * size
					for z = 0, height - 1 do
						value = threatMap[z * width + x] - base
						draw = (value ~= 0)
						local mycolor = {0.0, 0.0, 0.0, 0.6} -- alpha defined here!

						if value > 0 then
							pz = z * size
							value = value / div
							mycolor[1] = value
						elseif value < 0 then
							pz = z * size
							value = -value / div
							mycolor[3] = value
						end
						if draw then
							pushElementInstance( -- pushElementInstance(iT,thisInstance, instanceID, updateExisting, noUpload, unitID)
								circleInstanceVBOsynced, -- the buffer to push into
								{ -- the data per instance
									px + size/2, 0, (z * size) + size/2, size/2, -- in vec4 centerposxyz_radius;
									mycolor[1],mycolor[2],mycolor[3],mycolor[4], -- in vec4 color;
								},
								nil, -- no instance key given, if given, it will update existing instance with same key instead of new instance
								true, -- updateExisting
								true) -- noUpload = true, we will upload whole buffer to gpu after we are done filling it
						end
					end
				end
				uploadAllElements(circleInstanceVBOsynced) -- upload everything if noUpload was used
			end

			-- This part is the drawing part, the whole pushElementInstance only needs to be called every time the
			gl.DepthTest(false) -- so that it doesnt get hidden under terrain
			gl.Texture(0, "$heightmap")
			circleShadersynced:Activate()
			--Spring.Echo("Drawing AI DBG circleInstanceVBOsynced", circleInstanceVBOsynced.usedElements)
			drawInstanceVBO(circleInstanceVBOsynced)
			circleShadersynced:Deactivate()
			gl.Texture(0, false)
			gl.DepthTest(false)
		end
	end

	if aiData.isPrint then
		local threatMap = aiData.map
		local halfSize = size / 2;
--		local cx, cy, cz = Spring.GetCameraDirection()
--		local dir = ((math.atan2(cx, cz) / math.pi) + 1) * 180

		for x = 1, width do
			px = (x - 1) * size + halfSize
			for z = 0, height - 1 do
				value = threatMap[z * width + x]
				if value ~= base then
					pz = z * size + halfSize

					local py = spGetGroundHeight(px, pz)
					if Spring.IsSphereInView(px, py, pz, size) then
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
	end

	if #aiData.marks > 0 then
		local halfSize = 16
		gl.Color(0.1, 0.8, 0.1, 0.8)
		for k, v in pairs(aiData.marks) do
			gl.DrawGroundQuad(v[1] - halfSize, v[2] - halfSize, v[1] + halfSize, v[2] + halfSize)
		end
	end

	gl.Color(1,1,1,1)
end

end
