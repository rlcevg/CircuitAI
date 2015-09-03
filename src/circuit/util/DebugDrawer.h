/*
 * DebugDrawer.h
 *
 *  Created on: Aug 21, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_DEBUGDRAWER_H_
#define SRC_CIRCUIT_UTIL_DEBUGDRAWER_H_

#include <SDL.h>
#include <map>
#include <mutex>
#include <set>

class SSkirmishAICallback;
namespace springai {
	class Debug;
}

namespace circuit {

class CCircuitAI;

class CDebugDrawer {
public:
	CDebugDrawer(CCircuitAI* circuit, const struct SSkirmishAICallback* sAICallback);
	virtual ~CDebugDrawer();

	int AddOverlayTexture(const float* texData, int w, int h);
// ---- Missing springai::Debug functions ---- BEGIN
	void UpdateOverlayTexture(int overlayTextureId, const float* texData, int x, int y, int w, int h);
	void DelOverlayTexture(int overlayTextureId);
	void SetOverlayTexturePos(int overlayTextureId, float x, float y);
	void SetOverlayTextureSize(int overlayTextureId, float w, float h);
	void SetOverlayTextureLabel(int overlayTextureId, const char* texLabel);
// ---- Missing springai::Debug functions ---- END

	int Init();
	void Release();
	Uint32 AddSDLWindow(int width, int height, const char* label);
	void DelSDLWindow(Uint32 windowId);
	void DrawMap(Uint32 windowId, const float* texData, SDL_Color colorMod = {255, 0, 0, 0});
	void DrawTex(Uint32 windowId, const float* texData);

	bool HasWindow(Uint32 windowId);
	void NeedRefresh(Uint32 windowId);
	void Refresh();
	static int WindowEventFilter(void* userdata, SDL_Event* event);

private:
	CCircuitAI* circuit;
	const struct SSkirmishAICallback* sAICallback;
	springai::Debug* debug;

	bool initialized;
	struct SWindow {
		SDL_Window* window;
		SDL_GLContext glcontext;
		SDL_Renderer* renderer;
		SDL_Texture* texture;
	};
	std::map<Uint32, SWindow*> windows;
	static std::map<Uint32, SWindow> allWindows;
	static std::mutex wndMutex;
	static unsigned int ddCounter;
	static std::set<Uint32> needRefresh;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_DEBUGDRAWER_H_
