#ifndef GAME_CANVAS_H
#define GAME_CANVAS_H

#include "integer.h"
#include "SDL.h"

#include <memory>
#include <vector>
#include <unordered_map>

class GameCanvas;
class GameAdapter {
public:
	virtual void onSetup(GameCanvas *canvas) {}
	virtual void onUpdate(GameCanvas *canvas, f32 dt) {}
	virtual void onDraw(GameCanvas *canvas) {}
};

class GameCanvas {
public:
	GameCanvas() {}
	GameCanvas(GameAdapter *adapter, u32 width, u32 height, u32 downScale = 2);

	void clear(f32 r = 0.0f, f32 g = 0.0f, f32 b = 0.0f);
	void put(i32 x, i32 y, f32 r, f32 g, f32 b);
	void rect(i32 x, i32 y, u32 w, u32 h, f32 r, f32 g, f32 b);
	void line(i32 x1, i32 y1, i32 x2, i32 y2, f32 r, f32 g, f32 b);

	i32 chr(char c, i32 x, i32 y, f32 r = 1.0f, f32 g = 1.0f, f32 b = 1.0f);
	i32 str(const std::string& txt, i32 x, i32 y, f32 r = 1.0f, f32 g = 1.0f, f32 b = 1.0f);

	i32 run();

	u32 width() const { return m_width; }
	u32 height() const { return m_height; }

	bool isPressed(u32 key) { return m_keyboard[key].pressed; }
	bool isReleased(u32 key) { return m_keyboard[key].released; }
	bool isHeld(u32 key) { return m_keyboard[key].held; }

private:
	SDL_Window *m_window;
	SDL_Renderer *m_renderer;
	SDL_Texture *m_buffer;

	std::unique_ptr<GameAdapter> m_adapter;

	u32 m_width, m_height;
	u8* m_pixels;

	struct State {
		bool pressed, released, held;
	};

	std::unordered_map<u32, State> m_keyboard;
};

#endif // GAME_CANVAS_H