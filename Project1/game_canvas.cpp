#include "game_canvas.h"

#include <iostream>
#include <algorithm>

#define Clamp(x, a, b) (x < a ? a : x > b ? b : x)
#define Log(x) std::cerr << x << std::endl
#define Col(v) u8(Clamp(v * 255.0f, 0.0f, 255.0f));

GameCanvas::GameCanvas(GameAdapter *adapter, u32 width, u32 height, u32 downScale) {
	if (SDL_Init(SDL_INIT_EVERYTHING) > 0) {
		Log(SDL_GetError());
		return;
	}

	downScale = std::max(std::min(downScale, u32(6)), u32(1));
	m_width = width / downScale;
	m_height = height / downScale;
	m_adapter = std::unique_ptr<GameAdapter>(adapter);

	Log("SZ: " << m_width << "x" << m_height);

	m_window = SDL_CreateWindow(
		"Game Canvas",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		width, height,
		SDL_WINDOW_SHOWN
	);
	if (!m_window) {
		Log(SDL_GetError());
		SDL_Quit();
		return;
	}

	m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
	if (!m_renderer) {
		Log(SDL_GetError());
		SDL_Quit();
		return;
	}

	m_buffer = SDL_CreateTexture(
		m_renderer,
		SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
		m_width, m_height
	);

}

void GameCanvas::clear(f32 r, f32 g, f32 b) {
	for (u32 i = 0; i < m_width * m_height; i++) {
		m_pixels[i * 3 + 0] = Col(r);
		m_pixels[i * 3 + 1] = Col(g);
		m_pixels[i * 3 + 2] = Col(b);
	}
}

void GameCanvas::put(i32 x, i32 y, f32 r, f32 g, f32 b) {
	if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
	u32 i = (x + y * m_width) * 3;
	m_pixels[i + 0] = Col(r);
	m_pixels[i + 1] = Col(g);
	m_pixels[i + 2] = Col(b);
}

void GameCanvas::rect(i32 x, i32 y, u32 w, u32 h, f32 r, f32 g, f32 b) {
	for (i32 ry = y; ry < y + h; ry++) {
		for (i32 rx = x; rx < x + w; rx++) {
			put(rx, ry, r, g, b);
		}
	}
}

void GameCanvas::line(i32 x1, i32 y1, i32 x2, i32 y2, f32 r, f32 g, f32 b) {
	int dx = std::abs(x2 - x1);
	int sx = x1 < x2 ? 1 : -1;
	int dy = -std::abs(y2 - y1);
	int sy = y1 < y2 ? 1 : -1;
	int err = dx + dy;
	int e2 = 0;

	int x = x1;
	int y = y1;

	while (true) {
		put(x, y, r, g, b);

		if (x == x2 && y == y2) break;
		e2 = 2 * err;
		if (e2 >= dy) { err += dy; x += sx; }
		if (e2 <= dx) { err += dx; y += sy; }
	}
}

i32 GameCanvas::run() {
	if (m_renderer == nullptr || m_window == nullptr || m_buffer == nullptr)
		return -1;

	SDL_Event evt;
	bool running = true;

	const f64 timeStep = 1.0 / 60.0;
	f64 accum = 0.0, lastTime = f64(SDL_GetTicks()) / 1000.0;

	m_adapter->onSetup(this);

	while (running) {
		bool canRender = false;
		f64 currTime = f64(SDL_GetTicks()) / 1000.0;
		f64 delta = currTime - lastTime;
		lastTime = currTime;
		accum += delta;

		for (auto& e : m_keyboard) {
			e.second.pressed = false;
			e.second.released = false;
		}

		while (SDL_PollEvent(&evt)) {
			switch (evt.type) {
				case SDL_QUIT: running = false; break;
				case SDL_KEYDOWN: {
					m_keyboard[evt.key.keysym.sym].pressed = true;
					m_keyboard[evt.key.keysym.sym].held = true;
				} break;
				case SDL_KEYUP: {
					m_keyboard[evt.key.keysym.sym].released = true;
					m_keyboard[evt.key.keysym.sym].held = false;
				} break;
				default: break;
			}
		}

		while (accum >= timeStep) {
			m_adapter->onUpdate(this, f32(timeStep));
			accum -= timeStep;
			canRender = true;
		}

		if (canRender) {
			int pitch;
			SDL_LockTexture(m_buffer, nullptr, (void**) &m_pixels, &pitch);
			m_adapter->onDraw(this);
			SDL_UnlockTexture(m_buffer);

			SDL_RenderCopy(m_renderer, m_buffer, nullptr, nullptr);
			SDL_RenderPresent(m_renderer);
		}
	}


	SDL_DestroyTexture(m_buffer);
	SDL_DestroyRenderer(m_renderer);
	SDL_DestroyWindow(m_window);
	SDL_Quit();

	return 0;
}
