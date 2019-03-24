#include <iostream>

#include "game_canvas.h"
#include "stb_image.h"

#include <cmath>
#include <string>
#include <utility>
#include <algorithm>
#include <memory>

#define rad(x) (x * 0.0174533f)

struct Vec3 {
	f32 x, y, z;

	Vec3(f32 x, f32 y, f32 z) : x(x), y(y), z(z) {}
	Vec3(f32 angle, f32 z = 0.0f) : x(std::cosf(angle)), y(std::sinf(angle)), z(z) {}
	Vec3() : x(0.0f), y(0.0f), z(0.0f) {}

	f32 dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }

	Vec3 cross(const Vec3& o) const {
		return Vec3(y * o.z - z * o.y,  z * o.x - x * o.z,  x * o.y - y * o.x);
	}

	f32 length() const { return std::sqrtf(dot(*this)); }
	Vec3 normalized() const { return (*this) / length(); }
	f32 angleZ() const { return std::atan2f(y, x); }

	Vec3 rotateZ(f32 angle) const {
		const float s = std::sinf(angle), c = std::cosf(angle);
		f32 rx = x * c - y * s;
		f32 ry = x * s + y * c;
		return Vec3(rx, ry, z);
	}

	Vec3 lerp(const Vec3& to, f32 fac) const {
		return (*this) * (1.0f - fac) + to * fac;
	}

	Vec3 operator +(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
	Vec3 operator -(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
	Vec3 operator *(const Vec3& o) const { return Vec3(x * o.x, y * o.y, z * o.z); }
	Vec3 operator *(f32 o) const { return Vec3(x * o, y * o, z * o); }
	Vec3 operator /(f32 o) const { return Vec3(x / o, y / o, z / o); }
};

struct Object {
	Vec3 position{ 0.0f, 0.0f, 0.0f };
	float rotation{ 0.0f };
};

struct Viewer : public Object {
	float fov{ rad(60.0f) };
};

inline bool raySeg(
	const Vec3& o, const Vec3& d, const Vec3& a, const Vec3& b,
	Vec3& hit, Vec3& norm, float& t, float& u)
{
	Vec3 v1 = o - a;
	Vec3 v2 = b - a;
	Vec3 v3 = Vec3(-d.y, d.x, 0.0f);

	f32 d23 = v2.dot(v3);
	f32 t1 = v2.cross(v1).z / d23;
	f32 t2 = v1.dot(v3) / d23;

	if (t1 >= 0.0 && t2 >= 0.0 && t2 <= 1.0) {
		hit = Vec3(a.x + v2.x * t2, a.y + v2.y * t2, 0.0f);
		norm = Vec3(-v2.y, v2.x);
		t = t1;
		u = t2;
		return true;
	}
	return false;
}

const f32 blockSize = 8.0f;
const f32 maxDepth = 60.0f;

class Texture {
public:
	Texture() = default;
	~Texture() = default;

	Texture(const std::string& fileName) {
		i32 w, h, comp;
		u8* data = stbi_load(fileName.c_str(), &w, &h, &comp, 3);
		if (data) {
			m_width = w;
			m_height = h;
			m_pixels = std::vector<u8>(data, data + (w * h * 3));
			stbi_image_free(data);
		}
	}

	inline Vec3 sample(f32 u, f32 v) {
		u = u * m_width;
		v = v * m_height;

		u32 x = ::floor(u);
		u32 y = ::floor(v);

		f32 ur = u - x;
		f32 vr = v - y;
		f32 uo = 1.0f - ur;
		f32 vo = 1.0f - vr;

		Vec3 res =
			(get(x, y) * uo + get(x + 1, y) * ur) * vo +
			(get(x, y + 1) * uo + get(x + 1, y + 1) * ur) * vr;
		return res;
	}

	inline Vec3 get(u32 x, u32 y) {
		if (m_width == 0 || m_height == 0) return Vec3(1.0f, 0.0f, 1.0f);

		x = x % m_width;
		y = y % m_height;
		u32 uvi = (x + y * m_width) * 3;
		f32 r = f32(m_pixels[uvi + 0]) / 255.0f;
		f32 g = f32(m_pixels[uvi + 1]) / 255.0f;
		f32 b = f32(m_pixels[uvi + 2]) / 255.0f;
		return Vec3(r, g, b);
	}

private:
	u32 m_width{ 0 }, m_height{ 0 };
	std::vector<u8> m_pixels;
};

struct Line {
	Vec3 a, b;
	f32 u0, u1;
	Texture* texture{ nullptr };

	inline float uv(float t) {
		return (1.0f - t) * u0 + u1 * t;
	}
};

struct HitInfo {
	Line* line;
	Vec3 position, normal;
	f32 distance, u, length;
};

struct Model : public Object {
	struct Vert {
		Vec3 pos;
		f32 u;
	};

	Texture texture;
	std::vector<Vert> vertices;
	std::vector<u32> indices;

	inline void addVert(const Vec3& pos, f32 u) {
		Vert v;
		v.pos = pos;
		v.u = u;
		vertices.push_back(v);
	}

	inline void addIndex(u32 i) {
		indices.push_back(i);
	}

	Model() : Object() {}
	~Model() = default;
};

struct Block : public Model {
	Block(f32 x, f32 y, f32 w, f32 h) : Model() {
		position.x = x;
		position.y = y;

		const f32 u1 = w*2.0f;
		const f32 u2 = h*2.0f;
		addVert(Vec3(0, 0, 0), 0);
		addVert(Vec3(w, 0, 0), u1);
		addVert(Vec3(w, 0, 0), 0);
		addVert(Vec3(w, h, 0), u2);
		addVert(Vec3(w, h, 0), 0);
		addVert(Vec3(0, h, 0), u1);
		addVert(Vec3(0, h, 0), 0);
		addVert(Vec3(0, 0, 0), u2);

		addIndex(0);
		addIndex(1);
		addIndex(2);
		addIndex(3);
		addIndex(4);
		addIndex(5);
		addIndex(6);
		addIndex(7);
	}
};

struct Pillar : public Model {
	Pillar(f32 x, f32 y, f32 radius) : Model() {
		position.x = x;
		position.y = y;

		const u32 segments = 12;
		const f32 step = (M_PI * 2.0f) / segments;
		const f32 maxu = M_PI * 2.0f * radius;
		const f32 ustep = maxu / (segments / 2.0f);

		f32 u = 0.0f;
		for (f32 a = 0.0f; a < M_PI * 2.0f; a += step) {
			f32 cx = ::cosf(a) * radius;
			f32 cy = ::sinf(a) * radius;
			addVert(Vec3(cx + x, cy + y, 0.0f), u);
			u += ustep;
		}

		for (u32 i = 0; i < segments-1; i++) {
			addIndex(i);
			addIndex(i + 1);
		}
		addIndex(0);
		addIndex(segments - 1);
	}
};

class RayCastGame : public GameAdapter {
public:
	void onSetup(GameCanvas *canvas) {
		viewer.position = Vec3(8.0f, 8.0f, 0.0f);
		viewer.fov = rad(90);

		tfloor = Texture("floor.png");
		tceil = Texture("ceiling.png");
		twall = Texture("bricks.png");
		tpillar = Texture("pillar.png");

		Block* main = new Block(0, 0, 6, 6);
		main->texture = twall;
		add(main);

		const u32 pillars = 16;
		const f32 step = (M_PI * 2.0f) / pillars;
		for (f32 r = 0.0f; r < M_PI * 2.0f; r += step) {
			Pillar* pil = new Pillar(::cosf(r) + 1.5f, ::sinf(r) + 1.5f, 0.1f);
			pil->texture = tpillar;
			add(pil);
		}
	}

	void add(Model* model) {
		models.push_back(std::unique_ptr<Model>(model));
	}

	void onUpdate(GameCanvas *canvas, f32 dt) {
		if (canvas->isHeld(SDLK_x)) {
			viewer.fov += dt;
			if (viewer.fov >= rad(120)) {
				viewer.fov = rad(120);
			}
		} else if (canvas->isHeld(SDLK_z)) {
			viewer.fov -= dt;
			if (viewer.fov <= rad(20)) {
				viewer.fov = rad(20);
			}
		}

		if (canvas->isHeld(SDLK_LEFT)) {
			viewer.rotation -= dt * 1.8f;
		} else if (canvas->isHeld(SDLK_RIGHT)) {
			viewer.rotation += dt * 1.8f;
		}

		Vec3 dir(viewer.rotation);
		if (canvas->isHeld(SDLK_UP)) {
			Vec3 delta = dir * dt * 4.0f;
			viewer.position = viewer.position + delta;
			if (circleLines(viewer.position, 0.8f)) {
				viewer.position = viewer.position - delta;
			}
		} else if (canvas->isHeld(SDLK_DOWN)) {
			Vec3 delta = dir * dt * 4.0f;
			viewer.position = viewer.position - delta;
			if (circleLines(viewer.position, 0.8f)) {
				viewer.position = viewer.position + delta;
			}
		}
	}

	void onDraw(GameCanvas *canvas) {
		// Create lines
		lines.clear();
		for (auto&& model : models) {
			for (u32 i = 0; i < model->indices.size(); i += 2) {
				Model::Vert va = model->vertices[model->indices[i + 0]];
				Model::Vert vb = model->vertices[model->indices[i + 1]];
				Line ln;
				ln.a = va.pos + model->position;
				ln.b = vb.pos + model->position;
				ln.u0 = va.u;
				ln.u1 = vb.u;
				ln.texture = &model->texture;
				lines.push_back(ln);
			}
		}

		// Render
		canvas->clear();

		const f32 w2 = canvas->width() / 2;
		const f32 h2 = canvas->height() / 2;
		
		const f32 thf = ::tanf(viewer.fov / 2.0f);
		const f32 planeDist = w2 / thf;
		Vec3 plane(
			0.0f,
			thf,
			0.0f
		);
		plane = plane.rotateZ(viewer.rotation);

		for (u32 x = 0; x < canvas->width(); x++) {
			// Calculate the angle of the ray
			const f32 xf = (f32(x) / f32(canvas->width())) * 2.0f - 1.0f;

			Vec3 rayPos = viewer.position;
			Vec3 rayDir(
				::cosf(viewer.rotation) + plane.x * xf,
				::sinf(viewer.rotation) + plane.y * xf,
				0.0f
			);

			HitInfo info;
			if (rayLines(rayPos, rayDir, info) && info.distance < maxDepth) {
				const f32 d = info.distance * thf;
				const f32 ceil = h2 - f32(canvas->height()) / d;
				const f32 floor = canvas->height() - ceil;
				const f32 wh = floor - ceil;

				f32 fog = 1.0f - (d / maxDepth);
				for (u32 y = 0; y < canvas->height(); y++) {
					f32 fwx = info.position.x;
					f32 fwy = info.position.y;

					if (y <= ceil) {
						f32 dist = f32(canvas->height()) / ((canvas->height() - y) - h2);
						f32 we = (dist / d);
						f32 cfog = std::min(((h2 - y) / maxDepth), 1.0f);

						f32 fu = (we * fwx + (1.0f - we) * viewer.position.x) / 2.0f;
						f32 fv = (we * fwy + (1.0f - we) * viewer.position.y) / 2.0f;

						Vec3 c = tceil.sample(fu, fv) * cfog;
						canvas->put(x, y, c.x, c.y, c.z);
					} else if (y > ceil && y <= floor) {
						f32 u = info.line->uv(info.u);
						f32 v = f32(y - ceil) / wh;
						
						Vec3 c = info.line->texture->sample(u, v) * fog;
						canvas->put(x, y, c.x, c.y, c.z);
					} else { // Floor
						f32 u = info.line->uv(info.u);
						f32 v = f32(y - floor) / wh;

						f32 dist = f32(canvas->height()) / (y - h2);
						f32 we = (dist / d);
						f32 cfog = std::min(((y - h2) / maxDepth), 1.0f);

						f32 fu = (we * fwx + (1.0f - we) * viewer.position.x) / 2.0f;
						f32 fv = (we * fwy + (1.0f - we) * viewer.position.y) / 2.0f;

						Vec3 c = tfloor.sample(fu, fv) * cfog;
						if (v < 1.0f) {
							f32 mixFac = (1.0f - v) * we;
							Vec3 t = info.line->texture->sample(u, 1.0f - v) * fog * cfog;
							c = c + t * mixFac;
						}
						canvas->put(x, y, c.x, c.y, c.z);
					}
				}
			}
		}

		canvas->str("X: " + std::to_string(viewer.position.x), 5, 5);
		canvas->str("Y: " + std::to_string(viewer.position.y), 5, 13);
	}

	Vec3 closestPoint(const Vec3& a, const Vec3& b, const Vec3& p, f32& t) {
		Vec3 ap = p - a;
		Vec3 ab = b - a;
		f32 atb = ab.dot(ab);
		f32 apab = ap.dot(ab);
		t = apab / atb;
		return a + ab * t;
	}

	bool circleLines(const Vec3& o, f32 radius) {
		for (auto&& line : lines) {
			f32 t;
			Vec3 p = closestPoint(line.a * blockSize, line.b * blockSize, o, t);
			if (t >= 0.0f && t <= 1.0f) {
				f32 d = (p - o).length();
				if (d < radius) {
					return true;
				}
			}
		}
		return false;
	}

	bool rayLines(const Vec3& o, const Vec3& d, HitInfo& info) {
		using IDist = std::pair<u32, HitInfo>;
		std::vector<IDist> md;
		for (u32 i = 0; i < lines.size(); i++) {
			Vec3 hitPos, hitNorm;
			f32 dist, u;
			Vec3 a = lines[i].a * blockSize, b = lines[i].b * blockSize;
			if (raySeg(o, d, a, b, hitPos, hitNorm, dist, u)) {
				HitInfo hi;
				hi.distance = dist;
				hi.position = hitPos;
				hi.normal = hitNorm;
				hi.length = (b - a).length() / blockSize * 2.0f;
				hi.u = u;
				hi.line = &lines[i];
				md.push_back(std::make_pair(i, hi));
			}
		}

		std::sort(md.begin(), md.end(), [](const IDist& a, const IDist& b) {
			return a.second.distance < b.second.distance;
		});

		if (!md.empty()) {
			info = md[0].second;
			return true;
		}
		return false;
	}

	Viewer viewer{};

	std::vector<std::unique_ptr<Model>> models;
	std::vector<Line> lines;
	
	Texture twall, tfloor, tceil, tpillar;
};

int main(int argc, char** argv) {
	GameCanvas gc{ new RayCastGame(), 640, 480 };
	return gc.run();
}