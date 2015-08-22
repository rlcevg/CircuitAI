/*
 * DebugDrawer.cpp
 *
 *  Created on: Aug 21, 2015
 *	  Author: rlcevg
 */

#include "DebugDrawer.h"
#include "CircuitAI.h"
#include "utils.h"

#include "SSkirmishAICallback.h"
#include "AISCommands.h"
#include "OOAICallback.h"
#include "Debug.h"

namespace circuit {

using namespace springai;

std::map<Uint32, CDebugDrawer::SWindow> CDebugDrawer::allWindows;
std::mutex CDebugDrawer::wndMutex;
unsigned int CDebugDrawer::ddCounter = 0;
std::set<Uint32> CDebugDrawer::needRefresh;

CDebugDrawer::CDebugDrawer(CCircuitAI* circuit, const struct SSkirmishAICallback* sAICallback)
		: circuit(circuit)
		, sAICallback(sAICallback)
		, debug(circuit->GetCallback()->GetDebug())
		, initialized(false)
{
}

CDebugDrawer::~CDebugDrawer()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	delete debug;
	if (initialized) {
		Release();
	}
}

int CDebugDrawer::AddOverlayTexture(const float* texData, int w, int h)
{
	return debug->AddOverlayTexture(texData, w, h);
}

// ---- Missing springai::Debug functions ---- BEGIN
void CDebugDrawer::UpdateOverlayTexture(int overlayTextureId, const float* texData, int x, int y, int w, int h)
{
	SUpdateOverlayTextureDrawerDebugCommand cmd = {overlayTextureId, texData, x, y, w, h};
	sAICallback->Engine_handleCommand(circuit->GetSkirmishAIId(), COMMAND_TO_ID_ENGINE, -1, COMMAND_DEBUG_DRAWER_OVERLAYTEXTURE_UPDATE, &cmd);
}

void CDebugDrawer::DelOverlayTexture(int overlayTextureId)
{
	SDeleteOverlayTextureDrawerDebugCommand cmd = {overlayTextureId};
	sAICallback->Engine_handleCommand(circuit->GetSkirmishAIId(), COMMAND_TO_ID_ENGINE, -1, COMMAND_DEBUG_DRAWER_OVERLAYTEXTURE_DELETE, &cmd);
}

void CDebugDrawer::SetOverlayTexturePos(int overlayTextureId, float x, float y)
{
	SSetPositionOverlayTextureDrawerDebugCommand cmd = {overlayTextureId, x, y};
	sAICallback->Engine_handleCommand(circuit->GetSkirmishAIId(), COMMAND_TO_ID_ENGINE, -1, COMMAND_DEBUG_DRAWER_OVERLAYTEXTURE_SET_POS, &cmd);
}

void CDebugDrawer::SetOverlayTextureSize(int overlayTextureId, float w, float h)
{
	SSetSizeOverlayTextureDrawerDebugCommand cmd = {overlayTextureId, w, h};
	sAICallback->Engine_handleCommand(circuit->GetSkirmishAIId(), COMMAND_TO_ID_ENGINE, -1, COMMAND_DEBUG_DRAWER_OVERLAYTEXTURE_SET_SIZE, &cmd);
}

void CDebugDrawer::SetOverlayTextureLabel(int overlayTextureId, const char* texLabel)
{
	SSetLabelOverlayTextureDrawerDebugCommand cmd = {overlayTextureId, texLabel};
	sAICallback->Engine_handleCommand(circuit->GetSkirmishAIId(), COMMAND_TO_ID_ENGINE, -1, COMMAND_DEBUG_DRAWER_OVERLAYTEXTURE_SET_LABEL, &cmd);
}
// ---- Missing springai::Debug functions ---- END

int CDebugDrawer::Init()
{
	if (ddCounter++ == 0) {
		if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
//			if (SDL_Init(SDL_INIT_VIDEO) != 0) {
				circuit->LOG("SDL_Init Error: %s", SDL_GetError());
				return 1;
//			}
		}
		// NOTE: Spring doesn't handle multiple windows
		SDL_SetEventFilter(CDebugDrawer::WindowEventFilter, this);
	}

	initialized = true;
	return 0;
}

void CDebugDrawer::Release()
{
	if (--ddCounter == 0) {
		SDL_SetEventFilter(nullptr, nullptr);
	}
	SDL_Window* prevWin = SDL_GL_GetCurrentWindow();
	SDL_GLContext prevContext = SDL_GL_GetCurrentContext();

	wndMutex.lock();
	for (auto& kv : windows) {
		SWindow& wnd = *kv.second;
		SDL_GL_MakeCurrent(wnd.window, wnd.glcontext);

		SDL_DestroyTexture(wnd.texture);
		SDL_DestroyRenderer(wnd.renderer);
		SDL_DestroyWindow(wnd.window);

		allWindows.erase(kv.first);
	}
	windows.clear();
	wndMutex.unlock();

	SDL_GL_MakeCurrent(prevWin, prevContext);
//	SDL_Quit();
	initialized = false;
}

Uint32 CDebugDrawer::AddSDLWindow(int width, int height, const char* label)
{
	SDL_Window* prevWin = SDL_GL_GetCurrentWindow();
	SDL_GLContext prevContext = SDL_GL_GetCurrentContext();

	SWindow wnd;
	wnd.window = SDL_CreateWindow(label, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 320, 300, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if (wnd.window == nullptr) {
		circuit->LOG("SDL_CreateWindow Error: %s", SDL_GetError());
		SDL_GL_MakeCurrent(prevWin, prevContext);
		return -1;
	}
	wnd.renderer = SDL_CreateRenderer(wnd.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (wnd.renderer == nullptr) {
		SDL_DestroyWindow(wnd.window);
		circuit->LOG("SDL_CreateRenderer Error: %s", SDL_GetError());
		SDL_GL_MakeCurrent(prevWin, prevContext);
		return -1;
	}
	wnd.glcontext = SDL_GL_GetCurrentContext();
	wnd.texture = SDL_CreateTexture(wnd.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
	if (wnd.texture == nullptr) {
		SDL_DestroyRenderer(wnd.renderer);
		SDL_DestroyWindow(wnd.window);
		circuit->LOG("SDL_CreateTexture Error: %s", SDL_GetError());
		SDL_GL_MakeCurrent(prevWin, prevContext);
		return -1;
	}
	Uint32 windowId = SDL_GetWindowID(wnd.window);
	wndMutex.lock();
	windows[windowId] = &allWindows.emplace(windowId, wnd).first->second;
	wndMutex.unlock();

	SDL_GL_MakeCurrent(prevWin, prevContext);
	return windowId;
}

void CDebugDrawer::DelSDLWindow(Uint32 windowId)
{
	auto it = windows.find(windowId);
	if (it == windows.end()) {
		return;
	}

	SWindow& wnd = *it->second;
	SDL_Window* prevWin = SDL_GL_GetCurrentWindow();
	SDL_GLContext prevContext = SDL_GL_GetCurrentContext();
	SDL_GL_MakeCurrent(wnd.window, wnd.glcontext);

	SDL_DestroyTexture(wnd.texture);
	SDL_DestroyRenderer(wnd.renderer);
	SDL_DestroyWindow(wnd.window);
	wndMutex.lock();
	windows.erase(it);
	allWindows.erase(windowId);
	wndMutex.unlock();

	SDL_GL_MakeCurrent(prevWin, prevContext);
}

void CDebugDrawer::DrawMap(Uint32 windowId, const float* texData, SDL_Color colorMod)
{
	auto it = windows.find(windowId);
	if (it == windows.end()) {
		return;
	}

	SWindow& wnd = *it->second;
	SDL_Window* prevWin = SDL_GL_GetCurrentWindow();
	SDL_GLContext prevContext = SDL_GL_GetCurrentContext();
	SDL_GL_MakeCurrent(wnd.window, wnd.glcontext);

	SDL_Color* pixels;
	int tw, th, pitch;
	SDL_QueryTexture(wnd.texture, nullptr, nullptr, &tw, &th);

	SDL_LockTexture(wnd.texture, nullptr, (void**)&pixels, &pitch);
	for (int i = 0; i < tw * th; ++i) {
		pixels[i] = {(Uint8)(texData[i] * colorMod.b), (Uint8)(texData[i] * colorMod.g), (Uint8)(texData[i] * colorMod.r), 0};  // Assuming 0.0f <= texData[i] <= 1.0f
	}
	SDL_UnlockTexture(wnd.texture);

	SDL_RenderClear(wnd.renderer);
	SDL_RenderCopy(wnd.renderer, wnd.texture, nullptr, nullptr);
	SDL_RenderPresent(wnd.renderer);

	SDL_GL_MakeCurrent(prevWin, prevContext);
}

bool CDebugDrawer::HasWindow(Uint32 windowId)
{
	std::lock_guard<std::mutex> guard(wndMutex);
	return allWindows.find(windowId) != allWindows.end();
}

void CDebugDrawer::NeedRefresh(Uint32 windowId)
{
	std::lock_guard<std::mutex> guard(wndMutex);
	needRefresh.insert(windowId);
}

void CDebugDrawer::Refresh()
{
	if (needRefresh.empty()) {
		return;
	}
	SDL_Window* prevWin = SDL_GL_GetCurrentWindow();
	SDL_GLContext prevContext = SDL_GL_GetCurrentContext();
	wndMutex.lock();
	for (Uint32 windowId : needRefresh) {
		auto it = allWindows.find(windowId);
		if (it == allWindows.end()) {
			continue;
		}
		SWindow& wnd = it->second;
		SDL_GL_MakeCurrent(wnd.window, wnd.glcontext);

		SDL_Rect rect = {0};
		SDL_GetWindowSize(wnd.window, &rect.w, &rect.h);
		SDL_RenderSetViewport(wnd.renderer, &rect);

		SDL_RenderClear(wnd.renderer);
		SDL_RenderCopy(wnd.renderer, wnd.texture, nullptr, nullptr);
		SDL_RenderPresent(wnd.renderer);
	}
	SDL_GL_MakeCurrent(prevWin, prevContext);
	needRefresh.clear();
	wndMutex.unlock();
}

int CDebugDrawer::WindowEventFilter(void* userdata, SDL_Event* event)
{
	CDebugDrawer* self = static_cast<CDebugDrawer*>(userdata);
	// @see rts/System/SpringApp.cpp MainEventHandler
	switch (event->type) {
		case SDL_WINDOWEVENT: {
			if (self->HasWindow(event->window.windowID)) {
				// case SDL_WINDOWEVENT_RESIZED:
				if (event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
					self->NeedRefresh(event->window.windowID);
				}
				return 0;
			} else {
				return 1;
			}
		} break;
//		case SDL_KEYDOWN:
//		case SDL_KEYUP:
//		case SDL_TEXTEDITING:
//		case SDL_TEXTINPUT:
		case SDL_MOUSEMOTION:
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
		case SDL_MOUSEWHEEL:
		case SDL_USEREVENT: {
			return self->HasWindow(event->window.windowID) ? 0 : 1;
		} break;
	};
	return 1;
}

} // namespace circuit
