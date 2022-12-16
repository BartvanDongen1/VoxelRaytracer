#pragma once
#include <unordered_map>

enum class Keys
{
	a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z,
	num0, num1, num2, num3, num4, num5, num6, num7, num8, num9,
	Space,
	LMB, RMB, MMB,
	Last,
};

struct KeyStruct
{
	bool pressed{ false };
	bool heldDown{ false };
	bool released{ false };
};

class InputManager
{
	friend class InputDispatcher;
public:
	InputManager();
	~InputManager();

	static void init();
	static void shutdown();

	static KeyStruct* getKey(Keys aKey);
	static void getMouseDeltaPosition(float* aX, float* aY);
	static float getMouseScroll();

	class InputDispatcher
	{
	public:
		static void update();
		static void updateKey(uint8_t aKeyNum, bool aPressed);
		static void updateMouse(float aDeltaX, float aDeltaY);
		static void updateScroll(float aDeltaX);
	};

private:

};

