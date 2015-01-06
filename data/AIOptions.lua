--
-- Custom Options Definition Table format
--
-- A detailed example of how this format works can be found
-- in the spring source under:
-- AI/Skirmish/NullAI/data/AIOptions.lua
--
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

local options = {
	{ -- section
		key    = 'performance',
		name   = 'Performance Relevant Settings',
		desc   = 'These settings may be relevant for both CPU usage and AI difficulty.',
		type   = 'section',
	},
	{ -- list
		key     = 'difficulty',
		name    = 'Level Of Difficulty',
		desc    = 'How tough the AI should be.\nkey: difficulty',
		type    = 'list',
		section = 'performance',
		def     = 'normal',
		items   = {
			{
				key  = 'easy',
				name = 'Easy',
				desc = 'The AI will attack seamlessly.',
			},
			{
				key  = 'normal',
				name = 'Normal',
				desc = 'The AI will try to think before attack.',
			},
			{
				key  = 'hard',
				name = 'Hard',
				desc = 'The AI will do its best.',
			},
		},
	},
	{ -- bool
		key     = 'ally_aware',
		name    = 'Alliance awareness',
		desc    = 'Consider allies presence while making expansion desicions',
		type    = 'bool',
		section = 'performance',
		def     = true,
	},
}

return options

